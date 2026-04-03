#pragma once
#include "../../common.h"

class InterfaceDetector
{
public:
    virtual ~InterfaceDetector() = default;
    virtual void detect(NodeContext &ctx) = 0;

    /// @brief 更新运行时参数（不重建模型），由各子类从 variant 中提取自己的参数
    virtual void updateParam(const WorkflowParam &wp) = 0;
};