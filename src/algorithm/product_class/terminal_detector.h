#pragma once
#include "interface_detector.h"
#include "../../ai_infer/yolo.h"
#include "../shape_match.h"
#include <memory>

/// @brief 端子检测器（通过 DetectorRegistry 自注册，type = "Terminal"）
class TerminalDetector : public InterfaceDetector
{
public:
    explicit TerminalDetector(const TerminalParam &param);
    void detect(NodeContext &ctx) override;
    void updateParam(const json &param) override;

private:
    void runAI(NodeContext &ctx);
    void runShapeMatch(NodeContext &ctx);
    void checkCount(int actual, NodeContext &ctx);
    void loadShapeModel(const ShapeMatchParam &param);

    TerminalParam terminal_param_;
    std::unique_ptr<AIInfer::YoloDetector> yolo_;
    std::unique_ptr<ShapeMatcher> matcher_;
};
