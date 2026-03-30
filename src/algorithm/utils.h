#pragma once

#include "../common.h"

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
        if (!roi.enabled)
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
        if (!roi.enabled || !contours.IsInitialized())
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
}