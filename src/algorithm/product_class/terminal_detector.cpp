#include "terminal_detector.h"
#include "detector_registry.h"
#include "../../config_mgr.h"
#include "../../ai_infer/postprocess.h"
#include "../utils.h"
#include <spdlog/spdlog.h>

// ── 自注册到 DetectorRegistry ───────────────────────────────
static bool _reg = (DetectorRegistry::registry("Terminal",
                                               [](const json &j)
                                               { return std::make_unique<TerminalDetector>(j.get<TerminalParam>()); }),
                    true);

TerminalDetector::TerminalDetector(const TerminalParam &param)
    : terminal_param_(param)
{
    switch (param.method)
    {
    case DetectMethod::AIInfer:
        yolo_ = std::make_unique<AIInfer::YoloDetector>(param.yolo_settings);
        break;
    case DetectMethod::MatchTemplate:
        loadShapeModel(param.shape_match_param);
        break;
    default:
        break;
    }
}

void TerminalDetector::updateParam(const json &param)
{
    terminal_param_ = param.get<TerminalParam>();
    const auto &tp = terminal_param_;

    switch (tp.method)
    {
    case DetectMethod::AIInfer:
        if (!yolo_)
            yolo_ = std::make_unique<AIInfer::YoloDetector>(tp.yolo_settings);
        else
        {
            yolo_->setScoreThreshold(tp.yolo_settings.score_threshold);
            yolo_->setNmsThreshold(tp.yolo_settings.nms_threshold);
        }
        break;
    case DetectMethod::MatchTemplate:
        loadShapeModel(tp.shape_match_param);
        break;
    default:
        break;
    }
}

void TerminalDetector::loadShapeModel(const ShapeMatchParam &param)
{
    const auto &path = param.model_path;
    if (path.empty())
        return;

    matcher_ = std::make_unique<ShapeMatcher>(param);
    try
    {
        HalconCpp::HTuple model_id;
        HalconCpp::ReadShapeModel(path.c_str(), &model_id);
        matcher_->setModelId(model_id);
        SPDLOG_INFO("Shape model loaded from {}", path);
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("ReadShapeModel failed: {}", e.ErrorMessage().Text());
        matcher_.reset();
    }
}

void TerminalDetector::detect(NodeContext &ctx)
{
    SPDLOG_INFO("TerminalDetector detect start");

    ctx.result.pass = true;
    HalconCpp::GenEmptyObj(&ctx.ok_contours);
    HalconCpp::GenEmptyObj(&ctx.ng_contours);

    switch (terminal_param_.method)
    {
    case DetectMethod::AIInfer:
        runAI(ctx);
        break;
    case DetectMethod::MatchTemplate:
        runShapeMatch(ctx);
        break;
    }

    if (!ctx.result.pass)
    {
        for (const auto &d : ctx.result.defects)
            SPDLOG_WARN("Defect: cls={}", d.defect_id);
    }

    SPDLOG_INFO("TerminalDetector detect end");
}

void TerminalDetector::checkCount(int actual, NodeContext &ctx)
{
    if (terminal_param_.count_TERM > 0 && actual != terminal_param_.count_TERM)
    {
        ctx.result.pass = false;
        ctx.result.defects.push_back({static_cast<int>(TermPartCls::TERM), {}});
    }
}

void TerminalDetector::runAI(NodeContext &ctx)
{
    auto det_result = yolo_->detect(ctx.src_image);
    auto groups = std::visit([](auto &dets)
                             { return AIInfer::PostProcessor::groupDetections(dets); }, det_result);

    const auto &p = terminal_param_;
    checkCount(static_cast<int>(groups.size()), ctx);

    auto isPartOk = [&](TermPartCls cls, int area) -> bool
    {
        auto it = p.part_inspectors.find(static_cast<int>(cls));
        if (it == p.part_inspectors.end() || !it->second.enabled)
            return true;
        const auto *bp = std::get_if<BlobInspectParam>(&it->second.param);
        if (bp)
            return area >= bp->area_min && area <= bp->area_max;
        return true;
    };

    for (const auto &group : groups)
    {
        std::visit([&](const auto &dets)
                   {
            for (const auto &det : dets)
            {
                const auto &box = [&]() -> const AIInfer::Detection & {
                    if constexpr (std::is_same_v<std::decay_t<decltype(det)>, AIInfer::DetectionObb>)
                        return det.detection;
                    else
                        return det;
                }();

                auto cls = static_cast<TermPartCls>(box.cls);
                int area = box.box.width * box.box.height;
                HalconCpp::HObject contour = utils::genBoxContourXld(det);
                utils::recordResult(isPartOk(cls, area), cls, contour, ctx);
            } }, group.dets);
    }
}

void TerminalDetector::runShapeMatch(NodeContext &ctx)
{
    if (!matcher_ || !matcher_->isValid())
    {
        SPDLOG_WARN("Shape model not ready, skip");
        ctx.result.pass = false;
        return;
    }

    ShapeMatchResult mr{};
    matcher_->match(ctx.src_image, mr);
    ctx.result.detector_result = mr;

    checkCount(mr.count(), ctx);
    utils::dispMatchOverlay(*matcher_, terminal_param_.shape_match_param, mr, ctx.ok_contours);

    for (int i = 0; i < mr.count(); i++)
        utils::runPartChecks(*matcher_, terminal_param_.part_inspectors,
                             ctx.src_image, mr, i, ctx);
}
