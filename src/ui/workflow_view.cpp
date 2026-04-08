#include "workflow_view.h"
#include "ui_workflow_view.h"
#include "workflow_row.h"
#include "camera_window.h"
#include "product_class/detector_param_widget_factory.h"
#include "../context.h"
#include "../workflow/workflow_mgr.h"
#include <QVBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
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
                                roi.enable_roi = true;
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

        // 配置 — 仅弹出检测器参数（非模态，允许同时操作相机窗口）
        connect(row, &WorkflowRow::sign_detectorClicked, this,
                [this, i](int index)
                {
                    auto *dlg = new QDialog(this);
                    dlg->setWindowTitle(QString("DI%1 检测器参数").arg(i));
                    dlg->setAttribute(Qt::WA_DeleteOnClose);
                    auto *layout = new QVBoxLayout(dlg);

                    auto param_copy = std::make_shared<json>(ctx_.workflows[i].detector_param);
                    std::function<void()> apply_fn;
                    QWidget *detector_widget = createDetectorParamWidget(
                        ctx_.workflows[i].detector_type, *param_copy, apply_fn,
                        i, camera_view_, dlg);

                    if (detector_widget)
                        layout->addWidget(detector_widget);
                    else
                    {
                        auto *label = new QLabel(QStringLiteral("未绑定检测器"), dlg);
                        label->setAlignment(Qt::AlignCenter);
                        layout->addWidget(label);
                    }

                    auto *buttonBox = new QDialogButtonBox(
                        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
                    layout->addWidget(buttonBox);

                    connect(buttonBox, &QDialogButtonBox::accepted, dlg,
                            [this, i, dlg, apply_fn, param_copy]()
                            {
                                if (apply_fn)
                                    apply_fn();
                                ctx_.workflows[i].detector_param = *param_copy;
                                wf_mgr_.updateParam(i, ctx_.workflows[i]);
                                dlg->accept();
                            });
                    connect(buttonBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
                    dlg->show();
                });
    }
}
