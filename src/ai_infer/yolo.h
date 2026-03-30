#pragma once
#include <memory>
#include <string>
#include <variant>
#include <opencv2/opencv.hpp>
#include <halconcpp/HalconCpp.h>
#include "ai_common.h"
#include "infer_backend.h"
#include "postprocess.h"

namespace AIInfer
{
    struct YOLOSettings
    {
        std::string model_path;
        float score_threshold = 0.5f;
        float nms_threshold = 0.5f;
        int image_stride = 32;
        int infer_size = 0;  // 动态输入时 letterbox 到此尺寸（0=按原图 stride 对齐）
        TaskType task_type = TaskType::YOLO_DET;
        InputDimensionType input_type = InputDimensionType::DYNAMIC;
        EngineType engine_type = EngineType::OPENVINO;
        bool end2end = false;
    };

    class YoloDetector
    {
    public:
        YoloDetector(const YOLOSettings &settings);
        ~YoloDetector() = default;

        DetectionResult detect(const cv::Mat &image);
        DetectionResult detect(const HalconCpp::HObject &image);

        void setScoreThreshold(float threshold) { settings_.score_threshold = threshold; }
        float getScoreThreshold() const { return settings_.score_threshold; }
        void setNmsThreshold(float threshold) { settings_.nms_threshold = threshold; }
        float getNmsThreshold() const { return settings_.nms_threshold; }

    private:
        YOLOSettings settings_;
        std::unique_ptr<InferBackend> backend_;
        std::unique_ptr<PostProcessor> post_processor_;
    };
}