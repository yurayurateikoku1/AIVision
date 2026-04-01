#include "workflow_config_dialog.h"
#include "ui_workflow_config_dialog.h"
#include "product_class/detector_param_widget_factory.h"

WorkflowConfigDialog::WorkflowConfigDialog(const WorkflowParam &param, CameraWindow *camera_view, QWidget *parent)
    : QDialog(parent), ui(new Ui::WorkflowConfigDialog), param_(param)
{
    ui->setupUi(this);
    loadCommon();

    QWidget *detector_widget = createDetectorParamWidget(
        param_.detector_param, detector_apply_fn_, param_.di_index, camera_view, this);
    if (detector_widget)
    {
        ui->label_noDetector->hide();
        ui->verticalLayout_detector->addWidget(detector_widget);
    }

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
