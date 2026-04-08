#pragma once
#include "detector_registry.h"

/// @brief 检测器工厂：根据 WorkflowParam 的 detector_type 通过注册表创建检测器
namespace DetectorFactory
{
    inline std::unique_ptr<InterfaceDetector> create(const WorkflowParam &param)
    {
        if (param.detector_type.empty())
            return nullptr;
        return DetectorRegistry::create(param.detector_type, param.detector_param);
    }
}
