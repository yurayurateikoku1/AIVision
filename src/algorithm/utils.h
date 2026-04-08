#pragma once

#include "../common.h"
#include "shape_match.h"

namespace utils
{
    /// @brief 将 Detection/DetectionObb 生成 Halcon XLD 矩形轮廓
    inline HalconCpp::HObject genBoxContourXld(const AIInfer::Detection &box)
    {
        HalconCpp::HObject contour;

        const double r1 = box.box.y;
        const double c1 = box.box.x;
        const double r2 = r1 + box.box.height;
        const double c2 = c1 + box.box.width;

        const double row = (r1 + r2) / 2.0;
        const double col = (c1 + c2) / 2.0;
        const double len1 = (c2 - c1) / 2.0;
        const double len2 = (r2 - r1) / 2.0;

        HalconCpp::GenRectangle2ContourXld(&contour, row, col, 0.0, len1, len2);
        return contour;
    }

    inline HalconCpp::HObject genBoxContourXld(const AIInfer::DetectionObb &det)
    {
        HalconCpp::HObject contour;
        double row = det.detection.box.y + det.detection.box.height / 2.0;
        double col = det.detection.box.x + det.detection.box.width / 2.0;
        double phi = static_cast<double>(det.angle);
        double len1 = det.detection.box.width / 2.0;
        double len2 = det.detection.box.height / 2.0;
        HalconCpp::GenRectangle2ContourXld(&contour, row, col, phi, len1, len2);
        return contour;
    }

    /// @brief ROI 裁剪：ReduceDomain + CropDomain，不复制像素
    inline bool crop2Roi(HalconCpp::HObject &image, const RoiParam &roi)
    {
        if (!roi.enable_roi)
            return true;
        try
        {
            HalconCpp::HObject roi_rect, reduced;
            HalconCpp::GenRectangle1(&roi_rect, roi.row1, roi.col1, roi.row2, roi.col2);
            HalconCpp::ReduceDomain(image, roi_rect, &reduced);
            HalconCpp::CropDomain(reduced, &image);
            return true;
        }
        catch (const HalconCpp::HException &e)
        {
            spdlog::error("ROI crop failed: {}", e.ErrorMessage().Text());
            return false;
        }
    }

    /// @brief 将检测器输出的轮廓从 ROI 局部坐标平移回全图坐标
    inline void offsetContours(HalconCpp::HObject &contours, const RoiParam &roi)
    {
        if (!roi.enable_roi || !contours.IsInitialized())
            return;
        try
        {
            HalconCpp::HObject moved;
            HalconCpp::MoveRegion(contours, &moved, roi.row1, roi.col1);
            contours = moved;
        }
        catch (const HalconCpp::HException &)
        {
            // contours 可能是 XLD，用仿射变换平移
            try
            {
                HalconCpp::HTuple hom;
                HalconCpp::HomMat2dIdentity(&hom);
                HalconCpp::HomMat2dTranslate(hom, roi.row1, roi.col1, &hom);
                HalconCpp::HObject moved;
                HalconCpp::AffineTransContourXld(contours, &moved, hom);
                contours = moved;
            }
            catch (const HalconCpp::HException &e)
            {
                spdlog::warn("Contour offset failed: {}", e.ErrorMessage().Text());
            }
        }
    }

    /// @brief 记录检测结果：OK 轮廓拼入 ok_contours，NG 拼入 ng_contours 并追加 defect
    inline void recordResult(bool ok, TermPartCls cls,
                             const HalconCpp::HObject &region,
                             NodeContext &ctx)
    {
        if (ok)
        {
            HalconCpp::ConcatObj(ctx.ok_contours, region, &ctx.ok_contours);
        }
        else
        {
            ctx.result.pass = false;
            HalconCpp::ConcatObj(ctx.ng_contours, region, &ctx.ng_contours);
            ctx.result.defects.push_back({static_cast<int>(cls), region});
        }
    }

    /// @brief 生成形状匹配结果的可视化叠加轮廓（轮廓 / 中心十字 / 外接矩形）
    inline void dispMatchOverlay(const ShapeMatcher &matcher,
                                 const ShapeMatchParam &sp,
                                 const ShapeMatchResult &mr,
                                 HalconCpp::HObject &ok_contours)
    {
        if (sp.display_contour)
        {
            HalconCpp::HObject contours = matcher.genResultContours(mr);
            HalconCpp::ConcatObj(ok_contours, contours, &ok_contours);
        }

        for (int i = 0; i < mr.count(); i++)
        {
            if (sp.display_center)
            {
                HalconCpp::HObject cross;
                HalconCpp::GenCrossContourXld(&cross, mr.row[i], mr.col[i], 20, mr.angle[i]);
                HalconCpp::ConcatObj(ok_contours, cross, &ok_contours);
            }

            if (sp.display_rect)
            {
                double h = sp.model_origin_row * 2;
                double w = sp.model_origin_col * 2;
                HalconCpp::HTuple hom;
                HalconCpp::VectorAngleToRigid(
                    sp.model_origin_row, sp.model_origin_col, 0,
                    mr.row[i], mr.col[i], mr.angle[i], &hom);
                HalconCpp::HObject rect, rect_trans;
                HalconCpp::GenRectangle1(&rect, 0, 0, h, w);
                HalconCpp::AffineTransRegion(rect, &rect_trans, hom, "nearest_neighbor");
                HalconCpp::ConcatObj(ok_contours, rect_trans, &ok_contours);
            }
        }
    }

    /// @brief Blob 检查：二值化 + 面积范围判定
    inline void runBlobCheck(const ShapeMatcher &matcher,
                             const BlobInspectParam &p, const PartRoi &roi,
                             const HalconCpp::HObject &image,
                             const ShapeMatchResult &mr, int match_index,
                             TermPartCls cls, NodeContext &ctx)
    {
        BlobResult br = matcher.blobInRoi(image, mr, match_index, roi, p.threshold);
        bool ok = (br.area >= p.area_min && br.area <= p.area_max);
        if (!ok)
            spdlog::warn("TERM[{}] cls={}: area={}, range=[{},{}]",
                         match_index, static_cast<int>(cls), br.area, p.area_min, p.area_max);
        recordResult(ok, cls, br.roi_region, ctx);
    }

    /// @brief Measure 检查：边缘测量 + 特征数判定
    inline void runMeasureCheck(const ShapeMatcher &matcher,
                                const MeasureInspectParam &p, const PartRoi &roi,
                                const HalconCpp::HObject &image,
                                const ShapeMatchResult &mr, int match_index,
                                TermPartCls cls, NodeContext &ctx)
    {
        MeasureResult mmr = matcher.measureInRoi(
            image, mr, match_index, roi, 1.0, p.contrast_min, p.contrast_max);
        bool ok = p.check_ge
                      ? (mmr.edge_count >= p.feature_limit)
                      : (mmr.edge_count <= p.feature_limit);
        if (!ok)
            spdlog::warn("TERM[{}] cls={}: edges={}, contrast=[{},{}], limit={}",
                         match_index, static_cast<int>(cls),
                         mmr.edge_count, p.contrast_min, p.contrast_max, p.feature_limit);
        recordResult(ok, cls, mmr.roi_region, ctx);
    }

    /// @brief 对单个匹配位置，遍历 part_inspectors 执行 Blob/Measure 检查
    inline void runPartChecks(const ShapeMatcher &matcher,
                              const std::map<int, PartInspector> &inspectors,
                              const HalconCpp::HObject &image,
                              const ShapeMatchResult &mr, int match_index,
                              NodeContext &ctx)
    {
        for (auto &[cls_id, insp] : inspectors)
        {
            if (!insp.enabled || !insp.roi.valid())
                continue;

            auto cls = static_cast<TermPartCls>(cls_id);

            std::visit([&](const auto &p)
            {
                using T = std::decay_t<decltype(p)>;
                if constexpr (std::is_same_v<T, BlobInspectParam>)
                    runBlobCheck(matcher, p, insp.roi, image, mr, match_index, cls, ctx);
                else
                    runMeasureCheck(matcher, p, insp.roi, image, mr, match_index, cls, ctx);
            }, insp.param);
        }
    }

}