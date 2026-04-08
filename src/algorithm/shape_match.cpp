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

BlobResult ShapeMatcher::blobInRoi(const HObject &image,
                                    const ShapeMatchResult &result,
                                    int match_index,
                                    const PartRoi &roi,
                                    int threshold) const
{
    BlobResult br;

    // 从模板区域中心 → 匹配位置的仿射矩阵
    HTuple hom;
    VectorAngleToRigid(
        param_.model_origin_row, param_.model_origin_col, 0,
        result.row[match_index], result.col[match_index], result.angle[match_index], &hom);

    // 生成 ROI 并仿射变换到匹配位置
    HObject roi_rect;
    GenRectangle1(&roi_rect, roi.row1, roi.col1, roi.row2, roi.col2);
    AffineTransRegion(roi_rect, &br.roi_region, hom, "nearest_neighbor");

    // 在 ROI 内二值化 + 连通域分析
    HObject reduced, thresholded, connected;
    ReduceDomain(image, br.roi_region, &reduced);
    Threshold(reduced, &thresholded, threshold, 255);
    Connection(thresholded, &connected);

    HTuple num;
    CountObj(connected, &num);
    if (num.I() == 0)
        return br;

    // 取面积最大的连通域
    HObject sorted;
    SortRegion(connected, &sorted, "character", "true", "row");
    SelectObj(sorted, &br.region, 1);

    // 面积
    HTuple area, row_c, col_c;
    AreaCenter(br.region, &area, &row_c, &col_c);
    br.area = area.I();

    // 外接矩形高度
    HTuple r1, c1, r2, c2;
    SmallestRectangle1(br.region, &r1, &c1, &r2, &c2);
    br.height = static_cast<int>(r2.D() - r1.D());

    return br;
}

MeasureResult ShapeMatcher::measureInRoi(const HObject &image,
                                          const ShapeMatchResult &result,
                                          int match_index,
                                          const PartRoi &roi,
                                          double sigma,
                                          int contrast_min,
                                          int contrast_max) const
{
    MeasureResult mr;

    // 从模板区域中心 → 匹配位置的仿射矩阵
    HTuple hom;
    VectorAngleToRigid(
        param_.model_origin_row, param_.model_origin_col, 0,
        result.row[match_index], result.col[match_index], result.angle[match_index], &hom);

    // 生成 ROI 并仿射变换到匹配位置（用于显示）
    HObject roi_rect;
    GenRectangle1(&roi_rect, roi.row1, roi.col1, roi.row2, roi.col2);
    AffineTransRegion(roi_rect, &mr.roi_region, hom, "nearest_neighbor");

    // 计算 ROI 中心和方向（仿射变换后）
    double center_row = (roi.row1 + roi.row2) / 2.0;
    double center_col = (roi.col1 + roi.col2) / 2.0;
    double half_len1 = (roi.row2 - roi.row1) / 2.0; // 沿纵向的半长
    double half_len2 = (roi.col2 - roi.col1) / 2.0; // 沿横向的半宽
    double angle = result.angle[match_index];

    // 将 ROI 中心仿射变换到匹配位置
    HTuple tx, ty;
    AffineTransPoint2d(hom, center_row, center_col, &ty, &tx);

    // 获取图像尺寸
    HTuple img_w, img_h;
    GetImageSize(image, &img_w, &img_h);

    // 生成测量句柄
    HTuple measure_handle;
    GenMeasureRectangle2(ty, tx, angle, half_len1, half_len2,
                         img_w, img_h, "nearest_neighbor", &measure_handle);

    // 边缘测量：用对比度范围筛选边缘
    // MeasurePos 的 threshold 参数是最小对比度，再通过 amplitude 过滤最大对比度
    HTuple edge_rows, edge_cols, edge_amp, edge_dist;
    MeasurePos(image, measure_handle, sigma, contrast_min, "all", "all",
               &edge_rows, &edge_cols, &edge_amp, &edge_dist);
    CloseMeasure(measure_handle);

    // 按对比度上限过滤
    if (contrast_max < 255)
    {
        HTuple filtered_rows, filtered_cols;
        for (int k = 0; k < edge_amp.Length(); k++)
        {
            if (std::abs(edge_amp[k].D()) <= contrast_max)
            {
                filtered_rows.Append(edge_rows[k]);
                filtered_cols.Append(edge_cols[k]);
            }
        }
        edge_rows = filtered_rows;
        edge_cols = filtered_cols;
    }

    mr.edge_count = edge_rows.Length();
    if (mr.edge_count >= 2)
    {
        // 第一个到最后一个边缘的距离即为铜丝长度
        HTuple dr = edge_rows[mr.edge_count - 1].D() - edge_rows[0].D();
        HTuple dc = edge_cols[mr.edge_count - 1].D() - edge_cols[0].D();
        mr.length = std::sqrt(dr.D() * dr.D() + dc.D() * dc.D());
    }
    else if (mr.edge_count == 1)
    {
        mr.length = 0;
    }

    return mr;
}

HObject ShapeMatcher::genResultContours(const ShapeMatchResult &result) const
{
    HObject all_contours;
    GenEmptyObj(&all_contours);

    if (!model_valid_ || result.count() == 0)
        return all_contours;

    HObject model_contours;
    GetShapeModelContours(&model_contours, model_id_, 1);

    for (int i = 0; i < result.count(); i++)
    {
        HTuple homo;
        HomMat2dIdentity(&homo);
        HomMat2dScale(homo, result.scale[i], result.scale[i], 0, 0, &homo);
        HomMat2dRotate(homo, result.angle[i], 0, 0, &homo);
        HomMat2dTranslate(homo, result.row[i], result.col[i], &homo);

        HObject transformed;
        AffineTransContourXld(model_contours, &transformed, homo);
        ConcatObj(all_contours, transformed, &all_contours);
    }

    return all_contours;
}
