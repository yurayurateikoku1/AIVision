#include "camera_window.h"
#include "ui_camera_window.h"
#include <QResizeEvent>
#include <QTimer>
#include <thread>
#include <spdlog/spdlog.h>
#include "../camera/camera_mgr.h"
#include "../context.h"

CameraWindow::CameraWindow(const std::string &camera_name, CameraMgr &cam_mgr, QWidget *parent)
    : QWidget(parent), ui(new Ui::CameraWindow), cam_mgr_(cam_mgr), camera_name_(camera_name)
{
    ui->setupUi(this);

    ui->label_cameraId->setText(QString::fromStdString(camera_name));
    ui->label_status->setFixedSize(24, 24);
    ui->label_status->setPixmap(QPixmap(":/assets/zhuangtaideng0.png"));
    ui->pushButton_scaleWindow->setIcon(QIcon(":/assets/fangdachuangkou2x.png"));

    // Halcon 渲染画布
    ui->widget_window->setMinimumSize(320, 240);
    ui->widget_window->setStyleSheet("background-color: #1e1e1e;");
    ui->widget_window->setAttribute(Qt::WA_NativeWindow);

    // 监听相机在线状态，自行更新状态显示和回调注册
    connect(&cam_mgr_, &CameraMgr::sign_cameraStatusChanged,
            this, [this](const std::string &name, bool online)
            {
                if (name != camera_name_) return;
                setStatus(online);
                if (online)
                {
                    auto *cam = cam_mgr_.getCamera(name);
                    if (cam) cam->setCallback(this);
                } });
}

CameraWindow::~CameraWindow()
{
    hwindow_.reset();
    delete ui;
}

void CameraWindow::frameReceived(const std::string &camera_name, const HalconCpp::HObject &frame)
{
    // SDK 回调线程 → 投递到主线程
    // HObject 内部引用计数，按值捕获仅增加引用，不会深拷贝像素
    QMetaObject::invokeMethod(this, [this, cam_name = camera_name, img = frame]()
                              {
        updateFrame(img);
        emit sign_frameArrived(cam_name, img); }, Qt::QueuedConnection);
}

void CameraWindow::errorMsgReceived(const std::string &camera_name, int error_code, const std::string &msg)
{
    SPDLOG_ERROR("Camera {} error 0x{:08X}: {}", camera_name, error_code, msg);
    QMetaObject::invokeMethod(this, [this, cam_name = camera_name, code = error_code]()
                              {
        setStatus(false);
        emit sign_cameraErrorArrived(cam_name, code); }, Qt::QueuedConnection);
}

void CameraWindow::updateFrame(const HalconCpp::HObject &image)
{
    current_image_ = image;
    displayImage(current_image_);
}

void CameraWindow::updateFrameWithOverlay(const HalconCpp::HObject &image, const HalconCpp::HObject &contours, bool pass)
{
    current_image_ = image;
    displayImage(current_image_);

    if (!hwindow_ || !contours.IsInitialized())
        return;

    try
    {
        hwindow_->SetColor(pass ? "green" : "red");
        hwindow_->SetDraw("margin");
        hwindow_->SetLineWidth(2);
        hwindow_->DispObj(contours);
    }
    catch (const HalconCpp::HException &e)
    {
        SPDLOG_ERROR("Overlay display failed: {}", e.ErrorMessage().Text());
    }
}

void CameraWindow::setStatus(bool online)
{
    ui->label_status->setPixmap(QPixmap(online ? ":/assets/zhuangtaideng1.png" : ":/assets/zhuangtaideng0.png"));
}

void CameraWindow::on_pushButton_workflow_clicked()
{
    emit sign_selected(camera_name_);
}

void CameraWindow::on_pushButton_scaleWindow_clicked()
{
    maximized_ = !maximized_;
    ui->pushButton_scaleWindow->setIcon(maximized_ ? QIcon(":/assets/fangdachuangkou2x.png") : QIcon(":/assets/suoxiaochuangkou2x.png"));
    emit sign_maximizeRequested(camera_name_);
}

void CameraWindow::initHalconWindow()
{
    if (hwindow_)
        return;

    // 使用物理像素尺寸（高DPI屏幕下逻辑像素 ≠ 物理像素）
    qreal dpr = ui->widget_window->devicePixelRatioF();
    int w = static_cast<int>(ui->widget_window->width() * dpr);
    int h = static_cast<int>(ui->widget_window->height() * dpr);
    if (w <= 0 || h <= 0)
        return;

    try
    {
        hwindow_ = std::make_unique<HalconCpp::HWindow>(
            0, 0, w - 1, h - 1,
            static_cast<Hlong>(ui->widget_window->winId()),
            "visible", "");
        hwindow_w_ = ui->widget_window->width();
        hwindow_h_ = ui->widget_window->height();
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("HWindow create failed: {}", e.ErrorMessage().Text());
    }
}

void CameraWindow::displayImage(const HalconCpp::HObject &image)
{
    int w = ui->widget_window->width();
    int h = ui->widget_window->height();

    // 窗口尺寸变化时重建 HWindow
    if (hwindow_ && (w != hwindow_w_ || h != hwindow_h_))
    {
        hwindow_.reset();
    }

    if (!hwindow_)
        initHalconWindow();
    if (!hwindow_)
        return;

    try
    {
        HalconCpp::HTuple img_w, img_h;
        HalconCpp::GetImageSize(image, &img_w, &img_h);
        hwindow_->SetPart(0, 0, img_h.I() - 1, img_w.I() - 1);
        hwindow_->DispObj(image);
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("DispObj failed: {}", e.ErrorMessage().Text());
    }
}

void CameraWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (hwindow_)
    {
        hwindow_.reset();
        initHalconWindow();

        if (current_image_.IsInitialized())
            displayImage(current_image_);
    }
}

void CameraWindow::enterRoiMode()
{
    if (!hwindow_) return;

    // DrawRectangle1 是阻塞调用，放到独立线程执行
    std::thread([this]()
    {
        try
        {
            // 画之前先刷新显示，清除旧的 ROI 框
            QMetaObject::invokeMethod(this, [this]()
            {
                if (current_image_.IsInitialized())
                    displayImage(current_image_);
            }, Qt::BlockingQueuedConnection);

            double r1, c1, r2, c2;
            hwindow_->DrawRectangle1(&r1, &c1, &r2, &c2);
            QMetaObject::invokeMethod(this, [this, r1, c1, r2, c2]()
            {
                if (current_image_.IsInitialized())
                {
                    hwindow_->SetColor("blue");
                    hwindow_->SetDraw("margin");
                    hwindow_->SetLineWidth(2);
                    HalconCpp::DispRectangle1(*hwindow_, r1, c1, r2, c2);
                }
                emit sign_roiSelected(r1, c1, r2, c2);
            }, Qt::QueuedConnection);
        }
        catch (...) {}
    }).detach();
}
