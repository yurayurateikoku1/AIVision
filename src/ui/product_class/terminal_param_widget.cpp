#include "terminal_param_widget.h"
#include "ui_terminal_param_dialog.h"
#include "../camera_window.h"
#include "../../algorithm/shape_match.h"
#include <QTimer>
#include <spdlog/spdlog.h>
#include <filesystem>

TerminalParamWidget::TerminalParamWidget(TerminalParam &param, int di_index, QWidget *parent)
    : QWidget(parent), ui(new Ui::TerminalParamDialog), param_(param), di_index_(di_index)
{
    ui->setupUi(this);

    ui->widget_templateWindow->setStyleSheet("background-color: #1e1e1e;");

    ui->spinBox_terminalCount->setValue(param_.count);

    // 检测方法 — radio 切换 stackedWidget 页面
    ui->radioButton_aiInfer->setChecked(param_.method == DetectMethod::AIInfer);
    ui->radioButton_matchTemplate->setChecked(param_.method == DetectMethod::MatchTemplate);
    ui->stackedWidget_method->setCurrentIndex(param_.method == DetectMethod::AIInfer ? 0 : 1);

    connect(ui->radioButton_aiInfer, &QRadioButton::toggled, this, [this](bool checked)
            { ui->stackedWidget_method->setCurrentIndex(checked ? 0 : 1); });

    // AI 参数
    ui->doubleSpinBox_confThreshold->setValue(param_.yolo_settings.score_threshold);
    ui->doubleSpinBox_nmsThreshold->setValue(param_.yolo_settings.nms_threshold);

    // 形状匹配参数
    ui->doubleSpinBox_scaleMin->setValue(param_.shape_match_param.scale_min);
    ui->doubleSpinBox_scaleMax->setValue(param_.shape_match_param.scale_max);
    ui->doubleSpinBox_angleStart->setValue(param_.shape_match_param.angle_start);
    ui->doubleSpinBox_angleExtent->setValue(param_.shape_match_param.angle_extent);
    ui->doubleSpinBox_minScore->setValue(param_.shape_match_param.min_score);
    ui->spinBox_numLevels->setValue(param_.shape_match_param.num_levels);

    // 点击"创建模板"→ 在相机窗口画 ROI → 裁剪 → 创建模型 → 保存 → 显示
    connect(ui->pushButton_createTemplate, &QPushButton::clicked, this, [this]()
            {
        if (!camera_view_)
        {
            SPDLOG_WARN("No camera window available");
            return;
        }

        camera_view_->enterRoiMode();
        connect(camera_view_, &CameraWindow::sign_roiSelected, this,
            [this](double r1, double c1, double r2, double c2)
            {
                const auto &full_image = camera_view_->getCurrentImage();
                if (!full_image.IsInitialized())
                    return;

                try
                {
                    // 裁剪模板
                    HalconCpp::HObject roi_rect;
                    HalconCpp::GenRectangle1(&roi_rect, r1, c1, r2, c2);
                    HalconCpp::ReduceDomain(full_image, roi_rect, &template_image_);
                    HalconCpp::CropDomain(template_image_, &template_image_);

                    // 生成路径（按 di_index 区分）
                    std::filesystem::create_directories("data/model");
                    std::string img_path = "data/model/template_di" + std::to_string(di_index_) + ".png";
                    std::string shm_path = "data/model/shape_model_di" + std::to_string(di_index_) + ".shm";

                    // 保存模板图到磁盘
                    HalconCpp::WriteImage(template_image_, "png", 0, img_path.c_str());

                    // 创建形状模型并保存
                    ShapeMatcher matcher(param_.shape_match_param);
                    matcher.createModel(template_image_);
                    if (matcher.isValid())
                    {
                        // 通过 Halcon 直接保存模型文件
                        HalconCpp::WriteShapeModel(
                            matcher.getModelId(), shm_path.c_str());
                        SPDLOG_INFO("Shape model saved to {}", shm_path);
                    }

                    // 存路径到参数
                    param_.shape_match_param.template_image_path = img_path;
                    param_.shape_match_param.model_path = shm_path;

                    // 显示
                    initTemplateWindow();
                    displayTemplate(template_image_);
                    SPDLOG_INFO("Template created from ROI ({},{})~({},{})", r1, c1, r2, c2);
                }
                catch (HalconCpp::HException &e)
                {
                    SPDLOG_ERROR("Create template failed: {}", e.ErrorMessage().Text());
                }
            }, Qt::SingleShotConnection); });

    // 延迟初始化：加载已有的模板图
    QTimer::singleShot(0, this, [this]()
                       {
        initTemplateWindow();
        loadExistingTemplate(); });
}

TerminalParamWidget::~TerminalParamWidget()
{
    template_hwindow_.reset();
    delete ui;
}

void TerminalParamWidget::applyParam()
{
    param_.yolo_settings.score_threshold = static_cast<float>(ui->doubleSpinBox_confThreshold->value());
    param_.yolo_settings.nms_threshold = static_cast<float>(ui->doubleSpinBox_nmsThreshold->value());
    param_.count = ui->spinBox_terminalCount->value();
    param_.shape_match_param.num_matches = param_.count;
    param_.method = ui->radioButton_aiInfer->isChecked() ? DetectMethod::AIInfer : DetectMethod::MatchTemplate;

    param_.shape_match_param.scale_min = ui->doubleSpinBox_scaleMin->value();
    param_.shape_match_param.scale_max = ui->doubleSpinBox_scaleMax->value();
    param_.shape_match_param.angle_start = ui->doubleSpinBox_angleStart->value();
    param_.shape_match_param.angle_extent = ui->doubleSpinBox_angleExtent->value();
    param_.shape_match_param.min_score = ui->doubleSpinBox_minScore->value();
    param_.shape_match_param.num_levels = ui->spinBox_numLevels->value();
}

void TerminalParamWidget::loadExistingTemplate()
{
    const auto &path = param_.shape_match_param.template_image_path;
    if (path.empty() || !std::filesystem::exists(path))
        return;

    try
    {
        HalconCpp::ReadImage(&template_image_, path.c_str());
        displayTemplate(template_image_);
        SPDLOG_INFO("Loaded existing template from {}", path);
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("Load template image failed: {}", e.ErrorMessage().Text());
    }
}

void TerminalParamWidget::initTemplateWindow()
{
    QWidget *container = ui->widget_templateWindow;
    if (!container || container->width() <= 0 || container->height() <= 0)
        return;
    if (template_hwindow_)
        return;

    container->setAttribute(Qt::WA_NativeWindow);

    qreal dpr = container->devicePixelRatioF();
    int w = static_cast<int>(container->width() * dpr);
    int h = static_cast<int>(container->height() * dpr);

    try
    {
        template_hwindow_ = std::make_unique<HalconCpp::HWindow>(
            0, 0, w - 1, h - 1,
            static_cast<Hlong>(container->winId()),
            "visible", "");
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("Template HWindow create failed: {}", e.ErrorMessage().Text());
    }
}

void TerminalParamWidget::displayTemplate(const HalconCpp::HObject &image)
{
    if (!template_hwindow_ || !image.IsInitialized())
        return;

    try
    {
        HalconCpp::HTuple img_w, img_h;
        HalconCpp::GetImageSize(image, &img_w, &img_h);
        template_hwindow_->SetPart(0, 0, img_h.I() - 1, img_w.I() - 1);
        template_hwindow_->DispObj(image);
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("Template display failed: {}", e.ErrorMessage().Text());
    }
}
