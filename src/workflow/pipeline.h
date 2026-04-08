#pragma once

#include "../common.h"
#include "../algorithm/product_class/interface_detector.h"
#include <QObject>
#include <halconcpp/HalconCpp.h>
#include <atomic>
#include <mutex>
#include <memory>
#include <chrono>

namespace tf
{
    class Executor;
}
class CameraMgr;
class CommMgr;

/// @brief 单条检测流水线，对应一路 DI 触发信号。
/// 拥有状态机、检测器实例、帧缓存，执行完整的 inspection 流程。
class Pipeline : public QObject
{
    Q_OBJECT
public:
    Pipeline(const WorkflowParam &param, const std::string &camera_name,
             CameraMgr &cam_mgr, CommMgr &comm_mgr,
             const std::string &comm_name, tf::Executor &executor, QObject *parent = nullptr);

    /// @brief 构建/重建检测器（仅 IDLE 状态可调用）
    void buildDetector();

    /// @brief 更新运行时参数（拷贝副本 + 同步到检测器）
    void onParamUpdated(const WorkflowParam &new_param);

    /// @brief DI 边沿检测 + HOLDING_RESULT 超时检查（由 WorkflowMgr IO 轮询调用）
    void onIOUpdate(bool current_di, std::chrono::steady_clock::time_point now);

    /// @brief 帧到达（更新缓存，WAITING_FRAME 时触发检测）
    void onFrameArrived(const HalconCpp::HObject &frame);

    /// @brief 手动触发一次检测（离线测试）
    void triggerOnce();

    /// @brief 设置离线测试图片
    void setOfflineImage(const HalconCpp::HObject &image);

    bool isIdle() const { return state_ == State::IDLE; }

signals:
    /// @brief 检测完成
    void sign_inspectionDone(int di_index,
                             const HalconCpp::HObject &display_image,
                             const HalconCpp::HObject &ok_contours,
                             const HalconCpp::HObject &ng_contours,
                             const InspectionResult &result);

private:
    /// @brief 状态
    enum class State
    {
        IDLE = 0,          // 空闲
        WAITING_FRAME = 1, // 等待帧
        INSPECTING = 2,    // 检测中
        HOLDING_RESULT = 3 // 检测结果保持
    };

    /// @brief 事件
    enum class Event
    {
        DI_RISING,     // DI 上升
        FRAME_ARRIVED, // 帧到达
        INSPECT_DONE,  // 检测完成
        HOLD_EXPIRED   // 检测结果保持超时
    };

    void dispatch(Event event);
    void runInspection();
    void writeDO(bool ok_val, bool ng_val);

    WorkflowParam param_;
    std::string camera_name_;
    CameraMgr &cam_mgr_;
    CommMgr &comm_mgr_;
    std::string comm_name_;
    tf::Executor &executor_;

    std::unique_ptr<InterfaceDetector> detector_;
    std::atomic<State> state_{State::IDLE};
    bool last_di_state_ = false;

    HalconCpp::HObject latest_frame_;
    HalconCpp::HObject offline_image_;
    std::mutex frame_mutex_;
    bool offline_test_ = false;

    bool last_result_pass_ = true;
    std::chrono::steady_clock::time_point hold_start_;
};
