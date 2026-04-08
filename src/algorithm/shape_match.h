#pragma once

#include "../common.h"
#include <halconcpp/HalconCpp.h>
#include <vector>

/// @brief 单个 ROI 的 Blob 分析结果
struct BlobResult
{
    int area = 0;                  // 最大连通域面积
    int height = 0;                // 最大连通域外接矩形高度
    HalconCpp::HObject region;     // 最大连通域
    HalconCpp::HObject roi_region; // 变换后的 ROI 区域（用于显示）
};

/// @brief 单个 ROI 的边缘测量结果（用于铜丝长度检测）
struct MeasureResult
{
    int edge_count = 0;            // 检测到的边缘点数
    double length = 0;             // 铜丝长度（边缘间距离）
    HalconCpp::HObject roi_region; // 变换后的 ROI 区域（用于显示）
};

/// @brief 形状模板匹配
class ShapeMatcher
{
public:
    ShapeMatcher(const ShapeMatchParam &param);
    ~ShapeMatcher();

    /// @brief 从模板图像的 ROI 区域创建形状模型
    void createModel(const HalconCpp::HObject &template_image);

    /// @brief 在目标图像中查找匹配，返回是否找到
    bool match(const HalconCpp::HObject &search_image, ShapeMatchResult &result);

    /// @brief 根据匹配结果生成可视化轮廓（模型轮廓仿射变换到各匹配位置）
    HalconCpp::HObject genResultContours(const ShapeMatchResult &result) const;

    /// @brief 对指定匹配位置，在 ROI 内做 Blob 分析（内外卯/胶皮用）
    /// @param image 原始图像
    /// @param result 匹配结果
    /// @param match_index 匹配序号
    /// @param roi 部件 ROI（相对于模板区域中心）
    /// @param threshold 二值化阈值
    BlobResult blobInRoi(const HalconCpp::HObject &image,
                         const ShapeMatchResult &result,
                         int match_index,
                         const PartRoi &roi,
                         int threshold) const;

    /// @brief 对指定匹配位置，在 ROI 内做边缘测量（铜丝/胶皮用）
    /// @param image 原始图像
    /// @param result 匹配结果
    /// @param match_index 匹配序号
    /// @param roi 部件 ROI（相对于模板区域中心）
    /// @param sigma 高斯平滑 sigma
    /// @param contrast_min 边缘对比度下限
    /// @param contrast_max 边缘对比度上限
    MeasureResult measureInRoi(const HalconCpp::HObject &image,
                               const ShapeMatchResult &result,
                               int match_index,
                               const PartRoi &roi,
                               double sigma,
                               int contrast_min,
                               int contrast_max) const;

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
