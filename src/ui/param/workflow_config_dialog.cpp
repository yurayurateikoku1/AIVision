#include "workflow_config_dialog.h"
#include "ui_workflow_config_dialog.h"
#include <QFormLayout>
#include <QGroupBox>

WorkflowConfigDialog::WorkflowConfigDialog(const std::array<WorkflowParam, 4> &params, QWidget *parent)
    : QDialog(parent), ui(new Ui::WorkflowConfigDialog), params_(params)
{
    ui->setupUi(this);

    for (int i = 0; i < 4; i++)
    {
        QWidget *page = createTabPage(i);
        ui->tabWidget->addTab(page, QString("DI%1").arg(i));
        loadTab(i);
    }

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this]()
            {
                applyAll();
                accept(); });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

WorkflowConfigDialog::~WorkflowConfigDialog()
{
    delete ui;
}

QWidget *WorkflowConfigDialog::createTabPage(int index)
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);

    auto &t = tabs_[index];

    t.spinBox_delayMs = new QSpinBox;
    t.spinBox_delayMs->setRange(0, 9999);
    form->addRow(QStringLiteral("触发延时 (ms)"), t.spinBox_delayMs);

    t.spinBox_doOkAddr = new QSpinBox;
    t.spinBox_doOkAddr->setRange(0, 4095);
    form->addRow(QStringLiteral("DO OK 地址"), t.spinBox_doOkAddr);

    t.spinBox_doNgAddr = new QSpinBox;
    t.spinBox_doNgAddr->setRange(0, 4095);
    form->addRow(QStringLiteral("DO NG 地址"), t.spinBox_doNgAddr);

    t.spinBox_resultHoldMs = new QSpinBox;
    t.spinBox_resultHoldMs->setRange(0, 9999);
    form->addRow(QStringLiteral("结果保持 (ms)"), t.spinBox_resultHoldMs);

    t.doubleSpinBox_exposure = new QDoubleSpinBox;
    t.doubleSpinBox_exposure->setRange(-1.0, 999999.0);
    t.doubleSpinBox_exposure->setSpecialValueText(QStringLiteral("不覆盖"));
    form->addRow(QStringLiteral("曝光覆盖 (μs)"), t.doubleSpinBox_exposure);

    return page;
}

void WorkflowConfigDialog::loadTab(int index)
{
    const auto &p = params_[index];
    auto &t = tabs_[index];
    t.spinBox_delayMs->setValue(p.delay_ms);
    t.spinBox_doOkAddr->setValue(p.do_ok_addr);
    t.spinBox_doNgAddr->setValue(p.do_ng_addr);
    t.spinBox_resultHoldMs->setValue(p.result_hold_ms);
    t.doubleSpinBox_exposure->setValue(p.exposure_override);
}

void WorkflowConfigDialog::applyAll()
{
    for (int i = 0; i < 4; i++)
    {
        auto &p = params_[i];
        auto &t = tabs_[i];
        p.delay_ms          = t.spinBox_delayMs->value();
        p.do_ok_addr        = static_cast<uint16_t>(t.spinBox_doOkAddr->value());
        p.do_ng_addr        = static_cast<uint16_t>(t.spinBox_doNgAddr->value());
        p.result_hold_ms    = t.spinBox_resultHoldMs->value();
        p.exposure_override = static_cast<float>(t.doubleSpinBox_exposure->value());
    }
}
