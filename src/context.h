#pragma once

#include "common.h"
#include <taskflow/taskflow.hpp>
#include <array>

struct Context
{
    static Context &getInstance();

    std::map<std::string, CameraControlParam> camera_params; //
    CommunicationParam comm_param;                           // 唯一通信通道
    LightControlParam light_param;
    std::array<WorkflowParam, 4> workflows; // 4 路 DI，下标 = DI 编号

    // UI 状态
    int selected_workflow_index = 0; // 当前选中的 workflow 下标

    tf::Executor &getExecutor() { return executor_; }

private:
    Context();
    tf::Executor executor_;
};
