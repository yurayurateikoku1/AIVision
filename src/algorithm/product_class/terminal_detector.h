#pragma once
#include "../interface_detector.h"
#include "../../ai_infer/yolo.h"
#include <memory>

/// @brief 端子检测器
class TerminalDetector : public InterfaceDetector
{
public:
    explicit TerminalDetector(const TerminalParam &param);
    void detect(NodeContext &ctx) override;
    void updateParam(const WorkflowParam &wp) override;

private:
    TerminalParam terminal_param_;
    std::unique_ptr<AIInfer::YoloDetector> yolo_;
};
