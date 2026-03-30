#include "workflow_row.h"
#include "ui_workflow_row.h"
WorkflowRow::WorkflowRow(int workflow_index, const WorkflowParam &wp, QWidget *parent)
    : QWidget(parent), ui(new Ui::WorkflowRow), workflow_index_(workflow_index)
{
        ui->setupUi(this);
        ui->checkBox_enabled->setChecked(wp.enabled);

        connect(ui->checkBox_enabled, &QCheckBox::toggled, this,
                [this](bool checked)
                { emit sign_enabledChanged(workflow_index_, checked); });

        connect(ui->pushButton_roi, &QPushButton::clicked, this,
                [this]()
                { emit sign_roiClicked(workflow_index_); });

        connect(ui->pushButton_config, &QPushButton::clicked, this,
                [this]()
                { emit sign_configClicked(workflow_index_); });
}

WorkflowRow::~WorkflowRow() { delete ui; }