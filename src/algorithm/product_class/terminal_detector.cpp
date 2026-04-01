#include "terminal_detector.h"
#include "../../ai_infer/postprocess.h"
#include "../utils.h"
#include <spdlog/spdlog.h>

TerminalDetector::TerminalDetector(const TerminalParam &param)
{
    terminal_param_ = param;
    if (param.method == DetectMethod::AI)
    {
        yolo_ = std::make_unique<AIInfer::YoloDetector>(param.yolo_settings);
    }
    else if (param.method == DetectMethod::MatchTemplate)
    {
        const auto &shm_path = param.shape_match_param.model_path;
        if (!shm_path.empty())
        {
            matcher_ = std::make_unique<ShapeMatcher>(param.shape_match_param);
            try
            {
                HalconCpp::HTuple model_id;
                HalconCpp::ReadShapeModel(shm_path.c_str(), &model_id);
                matcher_->setModelId(model_id);
                SPDLOG_INFO("Shape model loaded from {}", shm_path);
            }
            catch (HalconCpp::HException &e)
            {
                SPDLOG_ERROR("ReadShapeModel failed: {}", e.ErrorMessage().Text());
                matcher_.reset();
            }
        }
    }
}

void TerminalDetector::updateParam(const WorkflowParam &wp)
{
    auto *tp = std::get_if<TerminalParam>(&wp.detector_param);
    if (!tp)
        return;
    terminal_param_ = *tp;

    if (tp->method == DetectMethod::AI)
    {
        if (!yolo_)
            yolo_ = std::make_unique<AIInfer::YoloDetector>(tp->yolo_settings);
        else
        {
            yolo_->setScoreThreshold(tp->yolo_settings.score_threshold);
            yolo_->setNmsThreshold(tp->yolo_settings.nms_threshold);
        }
    }
    else if (tp->method == DetectMethod::MatchTemplate)
    {
        // 从 .shm 文件加载形状模型
        const auto &shm_path = tp->shape_match_param.model_path;
        if (!shm_path.empty())
        {
            matcher_ = std::make_unique<ShapeMatcher>(tp->shape_match_param);
            try
            {
                HalconCpp::HTuple model_id;
                HalconCpp::ReadShapeModel(shm_path.c_str(), &model_id);
                matcher_->setModelId(model_id);
                SPDLOG_INFO("Shape model loaded from {}", shm_path);
            }
            catch (HalconCpp::HException &e)
            {
                SPDLOG_ERROR("ReadShapeModel failed: {}", e.ErrorMessage().Text());
                matcher_.reset();
            }
        }
    }
}

void TerminalDetector::detect(NodeContext &ctx)
{
    SPDLOG_INFO("TerminalDetector detect start");

    switch (terminal_param_.method)
    {
    case DetectMethod::AI:
        detectAI(ctx);
        break;
    case DetectMethod::MatchTemplate:
        detectShapeMatch(ctx);
        break;
    }

    SPDLOG_INFO("TerminalDetector detect end");
}

void TerminalDetector::detectAI(NodeContext &ctx)
{
    auto det_result = yolo_->detect(ctx.src_image);

    // 按 cls==0 锚框分组，其他类别为部件
    auto groups = std::visit([](auto &dets)
                             { return AIInfer::PostProcessor::groupDetections(dets); }, det_result);

    std::vector<TerminalResult> terminal_results;
    terminal_results.reserve(groups.size());

    HalconCpp::HObject all_contours;
    HalconCpp::GenEmptyObj(&all_contours);

    for (const auto &group : groups)
    {
        TerminalResult tr;
        tr.id = group.id;

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

                TerminalPart part;
                part.part_id = box.cls;
                part.confidence = box.conf;
                part.x = static_cast<float>(box.box.x);
                part.y = static_cast<float>(box.box.y);
                part.width = static_cast<float>(box.box.width);
                part.height = static_cast<float>(box.box.height);
                tr.parts.push_back(part);

                HalconCpp::HObject contour = utils::genBoxContourXld(det);
                HalconCpp::ConcatObj(all_contours, contour, &all_contours);
            } }, group.dets);

        terminal_results.push_back(tr);
    }

    ctx.result_contours = all_contours;
    ctx.result.detector_result = std::move(terminal_results);

    // 判定：端子数量 + 部件完整性
    auto &results_ref = std::get<std::vector<TerminalResult>>(ctx.result.detector_result);
    int expected = terminal_param_.count;
    int expected_parts = terminal_param_.parts_per_terminal;
    bool pass = (expected <= 0) || (static_cast<int>(results_ref.size()) == expected);
    if (pass && expected_parts > 0)
    {
        for (const auto &tr : results_ref)
        {
            if (!tr.is_complete(expected_parts))
            {
                pass = false;
                break;
            }
        }
    }
    ctx.result.pass = pass;

    if (!pass)
    {
        SPDLOG_WARN("AI detect: expected {} terminals with {} parts each, got {} terminals",
                    expected, terminal_param_.parts_per_terminal, results_ref.size());
    }
}

void TerminalDetector::detectShapeMatch(NodeContext &ctx)
{
    if (!matcher_ || !matcher_->isValid())
    {
        SPDLOG_WARN("Shape model not ready, skip");
        ctx.result.pass = false;
        return;
    }

    ShapeMatchResult match_result;
    bool found = matcher_->match(ctx.src_image, match_result);

    int expected = terminal_param_.count;
    ctx.result.pass = (expected <= 0) || (match_result.count() == expected);
    ctx.result.detector_result = match_result;

    // 获取模型轮廓，仿射变换到每个匹配位置
    HalconCpp::HObject all_contours;
    HalconCpp::GenEmptyObj(&all_contours);

    HalconCpp::HObject model_contours;
    HalconCpp::GetShapeModelContours(&model_contours, matcher_->getModelId(), 1);

    for (int i = 0; i < match_result.count(); i++)
    {
        // 构造仿射矩阵：缩放 + 旋转 + 平移
        HalconCpp::HTuple homo;
        HalconCpp::HomMat2dIdentity(&homo);
        HalconCpp::HomMat2dScale(homo,
            match_result.scale[i], match_result.scale[i],
            0, 0, &homo);
        HalconCpp::HomMat2dRotate(homo,
            match_result.angle[i],
            0, 0, &homo);
        HalconCpp::HomMat2dTranslate(homo,
            match_result.row[i], match_result.col[i],
            &homo);

        HalconCpp::HObject transformed;
        HalconCpp::AffineTransContourXld(model_contours, &transformed, homo);
        HalconCpp::ConcatObj(all_contours, transformed, &all_contours);
    }
    ctx.result_contours = all_contours;

    if (!ctx.result.pass)
    {
        SPDLOG_WARN("Shape match: expected {} matches, got {}", expected, match_result.count());
    }
}
