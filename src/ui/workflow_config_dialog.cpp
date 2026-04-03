// ── WorkflowConfigDialog ─────────────────────────────────────
//
// 工作流配置对话框：编辑一条 Pipeline 的全部参数。
//
// 结构分两部分：
//   1. 公共参数（delay_ms, DO 地址, 曝光覆盖等） — 本类直接管理
//   2. 检测器参数（AI / 形状匹配等） — 由工厂函数动态创建子 widget
//
// 绑定流程：
//   WorkflowConfigDialog 构造时拷贝一份 WorkflowParam（param_），
//   然后调用 createDetectorParamWidget()（见 detector_param_widget_factory.h），
//   该工厂函数对 param_.detector_param（variant）做 std::visit：
//     - TerminalParam → 创建 TerminalParamWidget，嵌入 ui 的 verticalLayout_detector
//     - monostate     → 不创建（显示 label_noDetector 提示）
//   同时工厂函数输出一个 apply_fn（std::function<void()>），
//   绑定到子 widget 的 applyParam() 方法。
//
//   用户点击确定时：
//     applyCommon()       — 将公共 spinBox 值写回 param_
//     detector_apply_fn_() — 调用子 widget 的 applyParam()，将检测器 spinBox 值写回 param_.detector_param
//   然后外部通过 getResult() 取走编辑后的 param_ 副本。
//

#include "workflow_config_dialog.h"
#include "ui_workflow_config_dialog.h"
#include "product_class/detector_param_widget_factory.h"

WorkflowConfigDialog::WorkflowConfigDialog(const WorkflowParam &param, CameraWindow *camera_view, QWidget *parent)
    : QDialog(parent), ui(new Ui::WorkflowConfigDialog), param_(param)
{
    ui->setupUi(this);
    loadCommon();

    // 根据 detector_param variant 类型动态创建检测器参数 widget
    // 同时输出 detector_apply_fn_，指向子 widget 的 applyParam()
    QWidget *detector_widget = createDetectorParamWidget(
        param_.detector_param, detector_apply_fn_, param_.di_index, camera_view, this);
    if (detector_widget)
    {
        ui->label_noDetector->hide();
        ui->verticalLayout_detector->addWidget(detector_widget);
    }

    // 确定：先提交公共参数，再提交检测器参数
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this]()
            {
                applyCommon();
                if (detector_apply_fn_)
                    detector_apply_fn_();
                accept(); });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

WorkflowConfigDialog::~WorkflowConfigDialog()
{
    delete ui;
}

void WorkflowConfigDialog::loadCommon()
{
    ui->spinBox_delayMs->setValue(param_.delay_ms);
    ui->spinBox_doOkAddr->setValue(param_.do_ok_addr);
    ui->spinBox_doNgAddr->setValue(param_.do_ng_addr);
    ui->spinBox_resultHoldMs->setValue(param_.result_hold_ms);
    ui->doubleSpinBox_exposure->setValue(param_.exposure_override);
}

void WorkflowConfigDialog::applyCommon()
{
    param_.delay_ms          = ui->spinBox_delayMs->value();
    param_.do_ok_addr        = static_cast<uint16_t>(ui->spinBox_doOkAddr->value());
    param_.do_ng_addr        = static_cast<uint16_t>(ui->spinBox_doNgAddr->value());
    param_.result_hold_ms    = ui->spinBox_resultHoldMs->value();
    param_.exposure_override = static_cast<float>(ui->doubleSpinBox_exposure->value());
}
