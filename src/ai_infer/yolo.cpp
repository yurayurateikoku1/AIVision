#include "yolo.h"
#include "openvino_infer.h"
namespace AIInfer
{
    YoloDetector::YoloDetector(const YOLOSettings &settings)
        : settings_(settings)
    {
        switch (settings_.engine_type)
        {
        case EngineType::OPENVINO:
            backend_ = std::make_unique<OpenVINOInfer>();
            break;
        default:
            throw std::runtime_error("Unsupported engine type");
        }
        post_processor_ = createPostProcessor(settings_.task_type, settings_.end2end);
        if (backend_->init(settings_.model_path, settings_.input_type) != 0)
            throw std::runtime_error("Failed to init inference backend: " + settings_.model_path);
    }

    DetectionResult YoloDetector::detect(const cv::Mat &image)
    {
        LetterBoxInfo lb_info;
        int target_w = backend_->getInputWidth();
        int target_h = backend_->getInputHeight();

        cv::Mat input;
        if (target_w > 0 && target_h > 0)
        {
            input = letterBox(image, target_w, target_h, lb_info);
        }
        else if (settings_.infer_size > 0)
        {
            // 动态输入：letterbox 到固定尺寸，避免大图推理过慢
            input = letterBox(image, settings_.infer_size, settings_.infer_size, lb_info);
        }
        else
        {
            // 动态输入，不缩放：按 stride 对齐
            int stride = settings_.image_stride;
            int pad_w = (stride - image.cols % stride) % stride;
            int pad_h = (stride - image.rows % stride) % stride;
            cv::copyMakeBorder(image, input, 0, pad_h, 0, pad_w,
                               cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114));
            lb_info = {1.0f, 0, 0};
        }

        std::vector<TensorData> outputs;
        backend_->infer(input, outputs);

        return post_processor_->process(outputs[0], lb_info,
                                        settings_.score_threshold,
                                        settings_.nms_threshold);
    }

    DetectionResult YoloDetector::detect(const HalconCpp::HObject &image)
    {
        // 从 HObject 提取图像尺寸和通道信息
        using namespace HalconCpp;
        HTuple channels;
        CountChannels(image, &channels);
        int c = static_cast<int>(channels.I());

        HTuple pointer, type, width, height;
        if (c == 1)
        {
            GetImagePointer1(image, &pointer, &type, &width, &height);
            cv::Mat gray(height.I(), width.I(), CV_8UC1,
                         reinterpret_cast<void *>(pointer.L()));
            cv::Mat bgr;
            cv::cvtColor(gray, bgr, cv::COLOR_GRAY2BGR);
            return detect(bgr);
        }
        else
        {
            HTuple pr, pg, pb;
            GetImagePointer3(image, &pr, &pg, &pb, &type, &width, &height);
            int h = height.I(), w = width.I();

            cv::Mat mat_r(h, w, CV_8UC1, reinterpret_cast<void *>(pr.L()));
            cv::Mat mat_g(h, w, CV_8UC1, reinterpret_cast<void *>(pg.L()));
            cv::Mat mat_b(h, w, CV_8UC1, reinterpret_cast<void *>(pb.L()));

            cv::Mat bgr;
            cv::merge(std::vector<cv::Mat>{mat_b, mat_g, mat_r}, bgr);
            return detect(bgr);
        }
    }
}