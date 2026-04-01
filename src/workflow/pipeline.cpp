#include "pipeline.h"
#include "../algorithm/detector_factory.h"
#include "../algorithm/utils.h"
#include "../camera/camera_mgr.h"
#include "../communication/comm_mgr.h"
#include <taskflow/taskflow.hpp>
#include <spdlog/spdlog.h>

Pipeline::Pipeline(const WorkflowParam &param, const std::string &camera_name,
                   CameraMgr &cam_mgr, CommMgr &comm_mgr,
                   const std::string &comm_name, tf::Executor &executor, QObject *parent)
    : QObject(parent), param_(param), camera_name_(camera_name),
      cam_mgr_(cam_mgr), comm_mgr_(comm_mgr),
      comm_name_(comm_name), executor_(executor)
{
}

void Pipeline::buildDetector()
{
    detector_ = param_.enabled ? DetectorFactory::create(param_) : nullptr;
    SPDLOG_INFO("Pipeline DI{}: detector {}", param_.di_index,
                detector_ ? "built" : "none");
}

void Pipeline::onParamUpdated(const WorkflowParam &new_param)
{
    param_ = new_param;
    if (detector_)
        detector_->updateParam(param_);
}

void Pipeline::onIOUpdate(bool current_di, std::chrono::steady_clock::time_point now)
{
    if (!param_.enabled)
        return;

    bool rising_edge = current_di && !last_di_state_;
    last_di_state_ = current_di;

    switch (state_)
    {
    case State::IDLE:
        if (rising_edge)
            dispatch(Event::DI_RISING);
        break;

    case State::WAITING_FRAME:
    case State::INSPECTING:
        break;

    case State::HOLDING_RESULT:
    {
        bool di_cleared = !current_di;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - hold_start_).count();

        if ((di_cleared && elapsed_ms >= param_.result_hold_ms) || elapsed_ms > 5000)
        {
            if (elapsed_ms > 5000)
                SPDLOG_WARN("Pipeline DI{}: DI clear timeout", param_.di_index);
            dispatch(Event::HOLD_EXPIRED);
        }
        break;
    }
    }
}

void Pipeline::onFrameArrived(const HalconCpp::HObject &frame)
{
    std::lock_guard lk(frame_mutex_);
    latest_frame_ = frame;

    if (state_ == State::WAITING_FRAME)
        dispatch(Event::FRAME_ARRIVED);
}

void Pipeline::triggerOnce()
{
    SPDLOG_INFO("Pipeline DI{}: manual trigger", param_.di_index);
    offline_test_ = true;
    dispatch(Event::DI_RISING);
}

void Pipeline::setOfflineImage(const HalconCpp::HObject &image)
{
    std::lock_guard lk(frame_mutex_);
    offline_image_ = image;
}

// 状态机
void Pipeline::dispatch(Event event)
{
    switch (state_)
    {
    case State::IDLE:
        if (event == Event::DI_RISING) // DI 上升
        {
            SPDLOG_INFO("DI{} rising", param_.di_index);
            std::lock_guard lk(frame_mutex_);
            if (latest_frame_.IsInitialized() || offline_image_.IsInitialized())
            {
                state_ = State::INSPECTING;
                executor_.silent_async([this]()
                                       { runInspection(); });
            }
            else
            {
                state_ = State::WAITING_FRAME;
            }
        }
        break;

    case State::WAITING_FRAME:
        if (event == Event::FRAME_ARRIVED) // 帧到达
        {
            state_ = State::INSPECTING;
            executor_.silent_async([this]()
                                   { runInspection(); });
        }
        break;

    case State::INSPECTING:
        if (event == Event::INSPECT_DONE) // 检测完成
        {
            if (offline_test_)
            {
                // 离线测试：不写 DO，直接回 IDLE
                offline_test_ = false;
                state_ = State::IDLE;
            }
            else
            {
                if (!comm_name_.empty())
                {
                    bool pass = last_result_pass_;
                    writeDO(pass, !pass);
                }
                hold_start_ = std::chrono::steady_clock::now();
                state_ = State::HOLDING_RESULT;
            }
        }
        break;

    case State::HOLDING_RESULT:
        if (event == Event::HOLD_EXPIRED)
        {
            writeDO(false, false);
            state_ = State::IDLE;
        }
        break;
    }
}

// ── 检测执行（Taskflow 线程池） ──────────────────────────
void Pipeline::runInspection()
{
    bool offline = offline_image_.IsInitialized();
    SPDLOG_INFO("Pipeline DI{} executing{}...", param_.di_index, offline ? " (offline)" : "");

    // 延时 + 曝光覆盖（仅在线）
    if (!offline)
    {
        if (param_.delay_ms > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(param_.delay_ms));

        if (param_.exposure_override > 0)
        {
            auto *cam = cam_mgr_.getCamera(camera_name_);
            if (cam)
                cam->setExposureTime(param_.exposure_override);
        }
    }

    // 采集图像
    NodeContext ctx;
    ctx.result.pass = true;

    if (offline)
    {
        ctx.src_image = offline_image_;
        ctx.display_image = offline_image_;
    }
    else
    {
        auto *cam = cam_mgr_.getCamera(camera_name_);
        if (!cam || !cam->grabOne(ctx.src_image))
        {
            SPDLOG_ERROR("Pipeline DI{}: capture failed", param_.di_index);
            state_ = State::IDLE;
            return;
        }
        ctx.display_image = ctx.src_image;
    }

    // ROI 裁剪
    if (!utils::crop2Roi(ctx.src_image, param_.roi))
        ctx.result.pass = false;

    // 检测
    if (detector_)
        detector_->detect(ctx);

    // 轮廓坐标偏移回全图
    utils::offsetContours(ctx.result_contours, param_.roi);

    // 时间戳
    auto now = std::chrono::system_clock::now();
    ctx.result.timestamp_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    SPDLOG_INFO("Pipeline DI{}: pass={}", param_.di_index, ctx.result.pass);

    // 清离线图
    {
        std::lock_guard lk(frame_mutex_);
        offline_image_.Clear();
    }

    last_result_pass_ = ctx.result.pass;
    emit sign_inspectionDone(param_.di_index, ctx.display_image, ctx.result_contours, ctx.result);

    // 回主线程推进状态机
    QMetaObject::invokeMethod(this, [this]()
                              { dispatch(Event::INSPECT_DONE); }, Qt::QueuedConnection);
}

void Pipeline::writeDO(bool ok_val, bool ng_val)
{
    if (comm_name_.empty())
        return;

    uint16_t ok_addr = param_.do_ok_addr;
    uint16_t ng_addr = param_.do_ng_addr;
    QMetaObject::invokeMethod(&comm_mgr_, [this, ok_addr, ng_addr, ok_val, ng_val]()
                              {
        comm_mgr_.writeSingleCoil(comm_name_, ok_addr, ok_val);
        comm_mgr_.writeSingleCoil(comm_name_, ng_addr, ng_val); }, Qt::QueuedConnection);
}
