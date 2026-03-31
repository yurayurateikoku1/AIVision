#pragma once

#include "../common.h"
#include <halconcpp/HalconCpp.h>

class ShapeMatcher
{
public:
    ShapeMatcher(const ShapeMatchParam &param);
    ~ShapeMatcher();

    /// 从模板图像的 ROI 区域创建形状模型
    void createModel(const HalconCpp::HObject &template_image);

    /// 在目标图像中查找匹配，返回是否找到
    bool match(const HalconCpp::HObject &search_image, ShapeMatchResult &result);

    /// 模型是否已创建
    bool isValid() const { return model_valid_; }

    void setParam(const ShapeMatchParam &param) { param_ = param; }
    ShapeMatchParam getParam() const { return param_; }

private:
    ShapeMatchParam param_;
    HalconCpp::HTuple model_id_;
    bool model_valid_ = false;
};
