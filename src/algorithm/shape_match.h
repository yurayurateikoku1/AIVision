#pragma once

#include "../common.h"
#include <halconcpp/HalconCpp.h>

class ShapeMatcher
{
public:
    ShapeMatcher(const ShapeMatchParam &param);
    ~ShapeMatcher();

    /// @brief 从模板图像的 ROI 区域创建形状模型
    void createModel(const HalconCpp::HObject &template_image);

    /// @brief 在目标图像中查找匹配，返回是否找到
    bool match(const HalconCpp::HObject &search_image, ShapeMatchResult &result);

    /// @brief 模型是否已创建
    bool isValid() const { return model_valid_; }

    /// @brief 获取模型句柄,用于用于保存等操作
    HalconCpp::HTuple getModelId() const { return model_id_; }

    /// @brief 设置已加载的模型句柄,从文件读取后使用
    void setModelId(const HalconCpp::HTuple &id)
    {
        if (model_valid_)
            ClearShapeModel(model_id_);
        model_id_ = id;
        model_valid_ = true;
    }

    void setParam(const ShapeMatchParam &param) { param_ = param; }
    ShapeMatchParam getParam() const { return param_; }

private:
    ShapeMatchParam param_;
    HalconCpp::HTuple model_id_;
    bool model_valid_ = false;
};
