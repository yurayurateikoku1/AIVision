#include "terminal_param_widget.h"
#include "ui_terminal_param_dialog.h"
#include "../camera_window.h"
#include "../../algorithm/shape_match.h"
#include "../../config_mgr.h"
#include <QTimer>
#include <spdlog/spdlog.h>
#include <filesystem>

PartInspector &TerminalParamWidget::inspector(TermPartCls cls)
{
    return param_.part_inspectors[static_cast<int>(cls)];
}

TerminalParamWidget::TerminalParamWidget(json &detector_param_json, int di_index, QWidget *parent)
    : QWidget(parent), ui(new Ui::TerminalParamDialog),
      param_json_(detector_param_json), di_index_(di_index)
{
    // 从 json 反序列化工作副本
    if (!param_json_.empty())
        param_ = param_json_.get<TerminalParam>();

    ui->setupUi(this);

    ui->widget_templateWindow->setStyleSheet("background-color: #1e1e1e;");

    ui->spinBox_terminalCount->setValue(param_.count_TERM);

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

    // 显示选项
    ui->checkBox_displayRect->setChecked(param_.shape_match_param.display_rect);
    ui->checkBox_displayContour->setChecked(param_.shape_match_param.display_contour);
    ui->checkBox_displayCenter->setChecked(param_.shape_match_param.display_center);

    // 卯口 tab — Blob 参数
    auto &ib = inspector(TermPartCls::IB);
    ui->checkBox_enableIB->setChecked(ib.enabled);
    if (auto *p = std::get_if<BlobInspectParam>(&ib.param))
    {
        ui->spinBox_IBThreshold->setValue(p->threshold);
        ui->spinBox_IBAreaMin->setValue(p->area_min);
        ui->spinBox_IBAreaMax->setValue(p->area_max);
    }

    auto &wb = inspector(TermPartCls::WB);
    ui->checkBox_enableWB->setChecked(wb.enabled);
    if (auto *p = std::get_if<BlobInspectParam>(&wb.param))
    {
        ui->spinBox_WBThreshold->setValue(p->threshold);
        ui->spinBox_WBAreaMin->setValue(p->area_min);
        ui->spinBox_WBAreaMax->setValue(p->area_max);
    }

    // 铜丝 tab — Measure 参数
    auto &ts = inspector(TermPartCls::TS);
    ui->checkBox_enableTS->setChecked(ts.enabled);
    if (auto *p = std::get_if<MeasureInspectParam>(&ts.param))
    {
        ui->spinBox_TSContrastMin->setValue(p->contrast_min);
        ui->spinBox_TSContrastMax->setValue(p->contrast_max);
        ui->spinBox_TSFeatureCount->setValue(p->feature_limit);
    }

    auto &bs = inspector(TermPartCls::BS);
    ui->checkBox_enableBS->setChecked(bs.enabled);
    if (auto *p = std::get_if<MeasureInspectParam>(&bs.param))
    {
        ui->spinBox_BSContrastMin->setValue(p->contrast_min);
        ui->spinBox_BSContrastMax->setValue(p->contrast_max);
        ui->spinBox_BSFeatureCount->setValue(p->feature_limit);
    }

    // 胶皮 tab — Measure 参数
    auto &ins = inspector(TermPartCls::INS);
    ui->checkBox_enableINS->setChecked(ins.enabled);
    if (auto *p = std::get_if<MeasureInspectParam>(&ins.param))
    {
        ui->spinBox_INSContrastMin->setValue(p->contrast_min);
        ui->spinBox_INSContrastMax->setValue(p->contrast_max);
        ui->spinBox_INSFeatureMax->setValue(p->feature_limit);
    }

    // ROI 画图按钮
    connect(ui->pushButton_drawRoiIB, &QPushButton::clicked, this, [this]()
            { drawPartRoi(inspector(TermPartCls::IB).roi); });
    connect(ui->pushButton_drawRoiWB, &QPushButton::clicked, this, [this]()
            { drawPartRoi(inspector(TermPartCls::WB).roi); });
    connect(ui->pushButton_drawRoiTS, &QPushButton::clicked, this, [this]()
            { drawPartRoi(inspector(TermPartCls::TS).roi); });
    connect(ui->pushButton_drawRoiBS, &QPushButton::clicked, this, [this]()
            { drawPartRoi(inspector(TermPartCls::BS).roi); });
    connect(ui->pushButton_drawRoiINS, &QPushButton::clicked, this, [this]()
            { drawPartRoi(inspector(TermPartCls::INS).roi); });

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
                    HalconCpp::HObject roi_rect;
                    HalconCpp::GenRectangle1(&roi_rect, r1, c1, r2, c2);
                    HalconCpp::ReduceDomain(full_image, roi_rect, &template_image_);
                    HalconCpp::CropDomain(template_image_, &template_image_);

                    std::filesystem::create_directories("data/model");
                    std::string img_path = "data/model/template_di" + std::to_string(di_index_) + ".png";
                    std::string shm_path = "data/model/shape_model_di" + std::to_string(di_index_) + ".shm";

                    HalconCpp::WriteImage(template_image_, "png", 0, img_path.c_str());

                    ShapeMatcher matcher(param_.shape_match_param);
                    matcher.createModel(template_image_);
                    if (matcher.isValid())
                    {
                        HalconCpp::WriteShapeModel(
                            matcher.getModelId(), shm_path.c_str());
                        SPDLOG_INFO("Shape model saved to {}", shm_path);
                    }

                    param_.shape_match_param.template_image_path = img_path;
                    param_.shape_match_param.model_path = shm_path;

                    HalconCpp::HTuple tw, th;
                    HalconCpp::GetImageSize(template_image_, &tw, &th);
                    param_.shape_match_param.model_origin_row = th.D() / 2.0;
                    param_.shape_match_param.model_origin_col = tw.D() / 2.0;

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
    param_.count_TERM = ui->spinBox_terminalCount->value();
    param_.shape_match_param.num_matches = param_.count_TERM;
    param_.method = ui->radioButton_aiInfer->isChecked() ? DetectMethod::AIInfer : DetectMethod::MatchTemplate;

    param_.shape_match_param.scale_min = ui->doubleSpinBox_scaleMin->value();
    param_.shape_match_param.scale_max = ui->doubleSpinBox_scaleMax->value();
    param_.shape_match_param.angle_start = ui->doubleSpinBox_angleStart->value();
    param_.shape_match_param.angle_extent = ui->doubleSpinBox_angleExtent->value();
    param_.shape_match_param.min_score = ui->doubleSpinBox_minScore->value();
    param_.shape_match_param.num_levels = ui->spinBox_numLevels->value();

    // 卯口 — Blob
    {
        auto &ib = inspector(TermPartCls::IB);
        ib.enabled = ui->checkBox_enableIB->isChecked();
        ib.param = BlobInspectParam{
            ui->spinBox_IBThreshold->value(),
            ui->spinBox_IBAreaMin->value(),
            ui->spinBox_IBAreaMax->value()};
    }
    {
        auto &wb = inspector(TermPartCls::WB);
        wb.enabled = ui->checkBox_enableWB->isChecked();
        wb.param = BlobInspectParam{
            ui->spinBox_WBThreshold->value(),
            ui->spinBox_WBAreaMin->value(),
            ui->spinBox_WBAreaMax->value()};
    }

    // 铜丝 — Measure
    {
        auto &ts = inspector(TermPartCls::TS);
        ts.enabled = ui->checkBox_enableTS->isChecked();
        ts.param = MeasureInspectParam{
            ui->spinBox_TSContrastMin->value(),
            ui->spinBox_TSContrastMax->value(),
            ui->spinBox_TSFeatureCount->value(),
            true};
    }
    {
        auto &bs = inspector(TermPartCls::BS);
        bs.enabled = ui->checkBox_enableBS->isChecked();
        bs.param = MeasureInspectParam{
            ui->spinBox_BSContrastMin->value(),
            ui->spinBox_BSContrastMax->value(),
            ui->spinBox_BSFeatureCount->value(),
            true};
    }

    // 胶皮 — Measure (check_ge = false: edge_count <= limit 为 OK)
    {
        auto &ins = inspector(TermPartCls::INS);
        ins.enabled = ui->checkBox_enableINS->isChecked();
        ins.param = MeasureInspectParam{
            ui->spinBox_INSContrastMin->value(),
            ui->spinBox_INSContrastMax->value(),
            ui->spinBox_INSFeatureMax->value(),
            false};
    }

    // 显示选项
    param_.shape_match_param.display_rect = ui->checkBox_displayRect->isChecked();
    param_.shape_match_param.display_contour = ui->checkBox_displayContour->isChecked();
    param_.shape_match_param.display_center = ui->checkBox_displayCenter->isChecked();

    // 序列化回 json
    param_json_ = param_;
}

void TerminalParamWidget::loadExistingTemplate()
{
    const auto &path = param_.shape_match_param.template_image_path;
    if (path.empty() || !std::filesystem::exists(path))
        return;

    try
    {
        HalconCpp::ReadImage(&template_image_, path.c_str());

        if (param_.shape_match_param.model_origin_row == 0 &&
            param_.shape_match_param.model_origin_col == 0)
        {
            HalconCpp::HTuple tw, th;
            HalconCpp::GetImageSize(template_image_, &tw, &th);
            param_.shape_match_param.model_origin_row = th.D() / 2.0;
            param_.shape_match_param.model_origin_col = tw.D() / 2.0;
        }

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
        displayAllRois();
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("Template display failed: {}", e.ErrorMessage().Text());
    }
}

void TerminalParamWidget::drawPartRoi(PartRoi &roi)
{
    if (!template_hwindow_ || !template_image_.IsInitialized())
    {
        SPDLOG_WARN("Template not loaded, cannot draw ROI");
        return;
    }

    try
    {
        double r1, c1, r2, c2;
        template_hwindow_->SetColor("green");
        template_hwindow_->DrawRectangle1(&r1, &c1, &r2, &c2);

        roi.row1 = static_cast<int>(r1);
        roi.col1 = static_cast<int>(c1);
        roi.row2 = static_cast<int>(r2);
        roi.col2 = static_cast<int>(c2);

        displayTemplate(template_image_);
        SPDLOG_INFO("ROI drawn: ({},{})~({},{})", roi.row1, roi.col1, roi.row2, roi.col2);
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("Draw ROI failed: {}", e.ErrorMessage().Text());
    }
}

void TerminalParamWidget::displayAllRois()
{
    if (!template_hwindow_)
        return;

    auto drawOne = [&](TermPartCls cls, const char *color)
    {
        auto it = param_.part_inspectors.find(static_cast<int>(cls));
        if (it == param_.part_inspectors.end())
            return;
        const auto &roi = it->second.roi;
        if (!roi.valid())
            return;
        try
        {
            template_hwindow_->SetColor(color);
            template_hwindow_->SetDraw("margin");
            template_hwindow_->SetLineWidth(2);
            HalconCpp::HObject rect;
            HalconCpp::GenRectangle1(&rect, roi.row1, roi.col1, roi.row2, roi.col2);
            template_hwindow_->DispObj(rect);
        }
        catch (HalconCpp::HException &)
        {
        }
    };

    drawOne(TermPartCls::IB, "yellow");
    drawOne(TermPartCls::WB, "cyan");
    drawOne(TermPartCls::TS, "magenta");
    drawOne(TermPartCls::BS, "blue");
    drawOne(TermPartCls::INS, "green");
}
