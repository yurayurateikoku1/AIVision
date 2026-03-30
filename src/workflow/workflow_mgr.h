#pragma once

#include "../common.h"
#include "pipeline.h"
#include <QObject>
#include <array>
#include <memory>
#include <atomic>

class CameraMgr;
class CommMgr;
struct Context;

/// @brief 工作流管理器：持有 4 条 Pipeline，转发 IO 事件，提供 public API。
class WorkflowMgr : public QObject
{
    Q_OBJECT
public:
    WorkflowMgr(Context &ctx, CameraMgr &cam_mgr, CommMgr &comm_mgr, QObject *parent = nullptr);
    ~WorkflowMgr();

    /// @brief 构建所有 Pipeline 的检测器
    void buildAll();

    /// @brief 开始自动检测
    void start();

    /// @brief 停止自动检测
    void stop();

    bool isRunning() const { return running_; }

    /// @brief 手动触发一次检测（离线测试）
    void triggerOnce(int di_index);

    /// @brief 设置离线测试图片
    void setOfflineImage(int di_index, const HalconCpp::HObject &image);

    /// @brief 更新参数到检测器（不重建模型）
    void updateParam(int di_index, const WorkflowParam &new_param);

    /// @brief 重建指定 Pipeline 的检测器（仅 IDLE 状态）
    void rebuildDetector(int di_index);

signals:
    void sign_inspectionDone(int di_index,
                             const HalconCpp::HObject &display_image,
                             const HalconCpp::HObject &result_contours,
                             const InspectionResult &result);
    void sign_runningChanged(bool running);

public slots:
    /// @brief 帧到达（所有 Pipeline 共享同一个相机）
    void slot_frameArrived(const HalconCpp::HObject &frame);

private slots:
    void slot_onIOStateUpdated();

private:
    Context &ctx_;
    CameraMgr &cam_mgr_;
    CommMgr &comm_mgr_;
    tf::Executor &executor_;

    std::array<std::unique_ptr<Pipeline>, 4> pipelines_;
    std::atomic<bool> running_{false};
};
