#pragma once
#include "../common.h"
#include "interface_detector.h"
#include "product_class/terminal_detector.h"
#include <memory>
#include <spdlog/spdlog.h>

namespace DetectorFactory
{

    /// @brief 根据 WorkflowParam 的 detector_param variant 创建对应检测器
    inline std::unique_ptr<InterfaceDetector> create(const WorkflowParam &param)
    {
        try
        {
            std::unique_ptr<InterfaceDetector> det;
            std::visit([&](const auto &p)
                       {
                           using T = std::decay_t<decltype(p)>;
                           if constexpr (std::is_same_v<T, TerminalParam>)
                               det = std::make_unique<TerminalDetector>(p);
                           // 新检测器在此扩展: else if constexpr (std::is_same_v<T, ScrewParam>) ...
                       },
                       param.detector_param);
            return det;
        }
        catch (const std::exception &e)
        {
            spdlog::error("DetectorFactory::create failed: {}", e.what());
            return nullptr;
        }
    }

}
