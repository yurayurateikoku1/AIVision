#include "shape_match.h"
#include <spdlog/spdlog.h>

using namespace HalconCpp;

ShapeMatcher::ShapeMatcher(const ShapeMatchParam &param)
    : param_(param)
{
}

ShapeMatcher::~ShapeMatcher()
{
    if (model_valid_)
        ClearShapeModel(model_id_);
}

void ShapeMatcher::createModel(const HObject &template_image)
{
    if (model_valid_)
    {
        ClearShapeModel(model_id_);
        model_valid_ = false;
    }

    try
    {
        CreateScaledShapeModel(template_image,
                               HTuple(param_.num_levels),
                               HTuple(param_.angle_start),
                               HTuple(param_.angle_extent),
                               HTuple(param_.angle_step),
                               HTuple(param_.scale_min),
                               HTuple(param_.scale_max),
                               HTuple(param_.scale_step),
                               "auto",  // optimization
                               HTuple(param_.metric.c_str()),
                               "auto",  // contrast
                               "auto",  // min_contrast
                               &model_id_);
        model_valid_ = true;
        SPDLOG_INFO("Shape model created successfully");
    }
    catch (HException &e)
    {
        SPDLOG_ERROR("CreateScaledShapeModel failed: {}", e.ErrorMessage().Text());
    }
}

bool ShapeMatcher::match(const HObject &search_image, ShapeMatchResult &result)
{
    if (!model_valid_)
    {
        SPDLOG_WARN("Shape model not created, skip matching");
        return false;
    }

    try
    {
        FindScaledShapeModel(search_image,
                             model_id_,
                             HTuple(param_.angle_start),
                             HTuple(param_.angle_extent),
                             HTuple(param_.scale_min),
                             HTuple(param_.scale_max),
                             HTuple(param_.min_score),
                             HTuple(param_.num_matches),
                             HTuple(param_.max_overlap),
                             "least_squares", // sub_pixel
                             HTuple(param_.num_levels),
                             HTuple(param_.greediness),
                             &result.row, &result.col,
                             &result.angle, &result.scale, &result.score);

        SPDLOG_INFO("Shape match found {} results", result.count());
        return result.count() > 0;
    }
    catch (HException &e)
    {
        SPDLOG_ERROR("FindScaledShapeModel failed: {}", e.ErrorMessage().Text());
        return false;
    }
}
