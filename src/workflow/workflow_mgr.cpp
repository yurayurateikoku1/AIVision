#include "workflow_mgr.h"
#include "../context.h"
#include "../communication/comm_mgr.h"
#include "../camera/camera_mgr.h"
#include <spdlog/spdlog.h>

WorkflowMgr::WorkflowMgr(Context &ctx, CameraMgr &cam_mgr, CommMgr &comm_mgr, QObject *parent)
    : QObject(parent), ctx_(ctx), cam_mgr_(cam_mgr), comm_mgr_(comm_mgr),
      executor_(ctx.getExecutor())
{
    const auto &cam_name = ctx_.camera_params.begin()->second.name;
    const auto &comm_name = ctx_.comm_param.name;

    for (int i = 0; i < 4; i++)
    {
        pipelines_[i] = std::make_unique<Pipeline>(
            ctx_.workflows[i], cam_name, cam_mgr_, comm_mgr_, comm_name, executor_, this);

        // 转发 Pipeline 的检测完成信号
        connect(pipelines_[i].get(), &Pipeline::sign_inspectionDone,
                this, &WorkflowMgr::sign_inspectionDone);
    }

    connect(&comm_mgr_, &CommMgr::sign_ioStateUpdated,
            this, &WorkflowMgr::slot_onIOStateUpdated);
}

WorkflowMgr::~WorkflowMgr()
{
    stop();
}

void WorkflowMgr::buildAll()
{
    for (auto &p : pipelines_)
        p->buildDetector();
}

void WorkflowMgr::start()
{
    if (running_)
        return;

    for (auto &p : pipelines_)
    {
        // 清 DO，重置状态（Pipeline 构造时已是 IDLE）
    }

    running_ = true;
    emit sign_runningChanged(true);
    SPDLOG_INFO("WorkflowMgr started");
}

void WorkflowMgr::stop()
{
    if (!running_)
        return;

    running_ = false;
    executor_.wait_for_all();

    emit sign_runningChanged(false);
    SPDLOG_INFO("WorkflowMgr stopped");
}

void WorkflowMgr::triggerOnce(int di_index)
{
    if (di_index < 0 || di_index >= 4)
        return;
    pipelines_[di_index]->triggerOnce();
}

void WorkflowMgr::setOfflineImage(int di_index, const HalconCpp::HObject &image)
{
    if (di_index < 0 || di_index >= 4)
        return;
    pipelines_[di_index]->setOfflineImage(image);
}

void WorkflowMgr::updateParam(int di_index, const WorkflowParam &new_param)
{
    if (di_index < 0 || di_index >= 4)
        return;
    ctx_.workflows[di_index] = new_param;
    pipelines_[di_index]->onParamUpdated(new_param);
    SPDLOG_INFO("Pipeline DI{} params updated", di_index);
}

void WorkflowMgr::rebuildDetector(int di_index)
{
    if (di_index < 0 || di_index >= 4)
        return;
    if (!pipelines_[di_index]->isIdle())
    {
        SPDLOG_WARN("Pipeline DI{} not idle, cannot rebuild", di_index);
        return;
    }
    pipelines_[di_index]->buildDetector();
}

void WorkflowMgr::slot_frameArrived(const HalconCpp::HObject &frame)
{
    for (auto &p : pipelines_)
        p->onFrameArrived(frame);
}

void WorkflowMgr::slot_onIOStateUpdated()
{
    if (!running_)
        return;

    auto di = comm_mgr_.diState();
    auto now = std::chrono::steady_clock::now();

    for (int i = 0; i < 4; i++)
    {
        if (static_cast<size_t>(i) < di.size())
            pipelines_[i]->onIOUpdate(di[i], now);
    }
}
