#include "terminal_param_widget.h"
#include "ui_terminal_param_dialog.h"

TerminalParamWidget::TerminalParamWidget(TerminalParam &param, QWidget *parent)
    : QWidget(parent), ui(new Ui::TerminalParamDialog), param_(param)
{
    ui->setupUi(this);
    ui->doubleSpinBox_confThreshold->setValue(param_.yolo_settings.score_threshold);
    ui->doubleSpinBox_nmsThreshold->setValue(param_.yolo_settings.nms_threshold);

    // 隐藏原 dialog 里的"设置参数"按钮，由外部 dialog 统一保存
    ui->pushButton_setTerminalSettings->hide();
}

TerminalParamWidget::~TerminalParamWidget()
{
    delete ui;
}

void TerminalParamWidget::apply()
{
    param_.yolo_settings.score_threshold = static_cast<float>(ui->doubleSpinBox_confThreshold->value());
    param_.yolo_settings.nms_threshold = static_cast<float>(ui->doubleSpinBox_nmsThreshold->value());
}
