#include "main_window.h"
#include "ui_main_window.h"
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QVBoxLayout>
#include <spdlog/spdlog.h>
#include "camera_window.h"
#include "workflow_view.h"
#include "operation_view.h"
#include "../context.h"
#include "../camera/camera_mgr.h"
#include "../communication/comm_mgr.h"
#include "../workflow/workflow_mgr.h"

MainWindow::MainWindow(Context &ctx, CameraMgr &cam_mgr, CommMgr &comm_mgr, WorkflowMgr &wf_mgr,
                       QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
      ctx_(ctx), cam_mgr_(cam_mgr), comm_mgr_(comm_mgr), wf_mgr_(wf_mgr)
{
    ui->setupUi(this);

    // 单相机
    initCamera();

    // 右侧面板
    workflow_view_ = new WorkflowView(ctx_, wf_mgr_, ui->widget_region_operation);
    operation_view_ = new OperationView(ctx_, wf_mgr_, ui->widget_region_operation);
    auto *op_layout = new QVBoxLayout(ui->widget_region_operation);
    op_layout->setContentsMargins(0, 0, 0, 0);
    op_layout->setSpacing(0);
    op_layout->addWidget(workflow_view_);
    op_layout->addWidget(operation_view_, 1);

    // 传递相机视图
    workflow_view_->setCameraView(camera_view_);
    operation_view_->setCameraView(camera_view_);

    // workflow 选中 → 更新 Context
    connect(workflow_view_, &WorkflowView::sign_workflowSelected, this,
            [this](int di_index)
            {
                ctx_.selected_workflow_index = di_index;
                SPDLOG_INFO("Selected workflow: DI{}", di_index);
            });

    initCommunication();

    // 检测结果 → 刷新相机窗口
    connect(&wf_mgr_, &WorkflowMgr::sign_inspectionDone, this,
            [this](int, const HalconCpp::HObject &display_image,
                   const HalconCpp::HObject &result_contours,
                   const InspectionResult &result)
            {
                if (camera_view_)
                    camera_view_->updateFrameWithOverlay(display_image, result_contours, result.pass);
            });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_action_parameter_triggered()
{
}

void MainWindow::on_action_camera_triggered()
{
}

void MainWindow::on_action_open_folder_triggered()
{
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择图片文件夹"));
    if (dir.isEmpty())
        return;

    QDir folder(dir);
    QStringList filters = {"*.bmp", "*.png", "*.jpg", "*.jpeg", "*.tif", "*.tiff"};
    auto entries = folder.entryInfoList(filters, QDir::Files, QDir::Name);
    if (entries.isEmpty())
    {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("文件夹中没有找到图片"));
        return;
    }

    std::vector<QString> paths;
    for (auto &fi : entries)
        paths.push_back(fi.absoluteFilePath());

    SPDLOG_INFO("Loaded {} offline images from {}", paths.size(), dir.toStdString());
    operation_view_->setImagePaths(paths);
}

void MainWindow::initCamera()
{
    const auto &cam_name = ctx_.camera_params.begin()->second.name;
    camera_view_ = new CameraWindow(cam_name, cam_mgr_, ui->widget_region_camera);

    auto *layout = new QVBoxLayout(ui->widget_region_camera);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(camera_view_);

    // 帧回调注册
    auto *cam = cam_mgr_.getCamera(cam_name);
    if (cam)
        cam->setCallback(camera_view_);
    camera_view_->setStatus(cam && cam->isOpened());

    // 相机帧 → WorkflowMgr
    connect(camera_view_, &CameraWindow::sign_frameArrived, &wf_mgr_,
            [this](const std::string &, const HalconCpp::HObject &frame)
            {
                wf_mgr_.slot_frameArrived(frame);
            });
}

void MainWindow::initCommunication()
{
    const auto &comm_name = ctx_.comm_param.name;

    ui->statusbar->addPermanentWidget(new QLabel(QString::fromStdString(comm_name), this));
    comm_status_led_ = new QLabel(this);
    comm_status_led_->setFixedSize(24, 24);
    comm_status_led_->setPixmap(QPixmap(":/assets/zhuangtaideng0.png"));
    ui->statusbar->addPermanentWidget(comm_status_led_);

    ui->statusbar->addPermanentWidget(new QLabel(QStringLiteral("光源"), this));
    light_status_led_ = new QLabel(this);
    light_status_led_->setFixedSize(24, 24);
    light_status_led_->setPixmap(QPixmap(":/assets/zhuangtaideng0.png"));
    ui->statusbar->addPermanentWidget(light_status_led_);

    // 通信连接到 CommMgr
    QMetaObject::invokeMethod(&comm_mgr_, [this]()
                              { comm_mgr_.addComm(ctx_.comm_param); }, Qt::QueuedConnection);

    connect(&comm_mgr_, &CommMgr::sign_commStatusChanged, this, [this](const std::string &, bool connected)
            { comm_status_led_->setPixmap(QPixmap(connected ? ":/assets/zhuangtaideng1.png" : ":/assets/zhuangtaideng0.png")); }, Qt::QueuedConnection);

    connect(&comm_mgr_, &CommMgr::sign_lightStatusChanged, this, [this](bool connected)
            { light_status_led_->setPixmap(QPixmap(connected ? ":/assets/zhuangtaideng1.png" : ":/assets/zhuangtaideng0.png")); }, Qt::QueuedConnection);
}
