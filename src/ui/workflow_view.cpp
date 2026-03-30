#include "workflow_view.h"
#include "ui_workflow_view.h"
#include "workflow_row.h"
#include "camera_window.h"
#include "workflow_config_dialog.h"
#include "../context.h"
#include "../workflow/workflow_mgr.h"
#include <QVBoxLayout>
#include <spdlog/spdlog.h>

WorkflowView::WorkflowView(Context &ctx, WorkflowMgr &wf_mgr, QWidget *parent)
    : QWidget(parent), ui(new Ui::WorkflowView), ctx_(ctx), wf_mgr_(wf_mgr)
{
    ui->setupUi(this);

    connect(ui->toolBox, &QToolBox::currentChanged, this,
            [this](int index)
            {
                if (index >= 0 && index < 4)
                    emit sign_workflowSelected(index);
            });

    initPage();
}

WorkflowView::~WorkflowView()
{
    delete ui;
}

void WorkflowView::setSelectedRow(int di_index)
{
    if (di_index >= 0 && di_index < ui->toolBox->count())
        ui->toolBox->setCurrentIndex(di_index);
}

void WorkflowView::initPage()
{
    // 设置相机标签
    if (!ctx_.camera_params.empty())
        ui->label_camera->setText(QString::fromStdString(ctx_.camera_params.begin()->first));

    for (int i = 0; i < 4; i++)
    {
        auto &wf = ctx_.workflows[i];

        auto *row = new WorkflowRow(i, wf);

        QWidget *page = ui->toolBox->widget(i);
        ui->toolBox->setItemText(i, QString("DI%1").arg(i));
        if (!page->layout())
            page->setLayout(new QVBoxLayout);
        page->layout()->addWidget(row);
        // ROI
        connect(row, &WorkflowRow::sign_roiClicked, this,
                [this, i](int index)
                {
                    if (!camera_view_)
                        return;
                    camera_view_->enterRoiMode();
                    connect(camera_view_, &CameraWindow::sign_roiSelected, this, [this, i](double r1, double c1, double r2, double c2)
                            {
                                auto &roi = ctx_.workflows[i].roi;
                                roi.enabled = true;
                                roi.row1 = static_cast<int>(r1);
                                roi.col1 = static_cast<int>(c1);
                                roi.row2 = static_cast<int>(r2);
                                roi.col2 = static_cast<int>(c2);
                                SPDLOG_INFO("ROI set: DI{} ({},{},{},{})", i, roi.row1, roi.col1, roi.row2, roi.col2); }, Qt::SingleShotConnection);
                });

        // 启用/禁用
        connect(row, &WorkflowRow::sign_enabledChanged, this,
                [this, i](int index, bool enabled)
                {
                    ctx_.workflows[i].enabled = enabled;
                });

        // 配置 — 直接调 WorkflowMgr，不经过 MainWindow
        connect(row, &WorkflowRow::sign_configClicked, this,
                [this, i](int index)
                {
                    WorkflowConfigDialog dlg(ctx_.workflows[i], this);
                    if (dlg.exec() == QDialog::Accepted)
                        wf_mgr_.updateParam(i, dlg.getResult());
                });
    }
}
