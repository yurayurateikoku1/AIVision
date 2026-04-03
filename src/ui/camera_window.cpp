#include "camera_window.h"
#include "ui_camera_window.h"
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
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
    ui->widget_window->setMouseTracking(true);
    ui->widget_window->installEventFilter(this);

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
    setResult(pass);

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

void CameraWindow::setResult(bool pass)
{
    ui->label_result->setText(pass ? "OK" : "NG");
    ui->label_result->setStyleSheet(pass
                                        ? "color: #00ff00; font-weight: bold; font-size: 16px;"
                                        : "color: #ff0000; font-weight: bold; font-size: 16px;");
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
        hwindow_.reset();

    if (!hwindow_)
        initHalconWindow();
    if (!hwindow_)
        return;

    try
    {
        HalconCpp::HTuple img_w, img_h;
        HalconCpp::GetImageSize(image, &img_w, &img_h);

        // 等比缩放：计算 SetPart 范围，保持图像宽高比不拉伸
        double iw = img_w.D(), ih = img_h.D();
        double ww = static_cast<double>(w), wh = static_cast<double>(h);
        double scale_w = iw / ww, scale_h = ih / wh;

        HalconCpp::HTuple r1, c1, r2, c2;
        if (scale_w > scale_h)
        {
            double pad = (iw * wh / ww - ih) / 2.0;
            r1 = -pad;  c1 = 0.0;
            r2 = ih - 1 + pad;  c2 = iw - 1;
        }
        else
        {
            double pad = (ih * ww / wh - iw) / 2.0;
            r1 = 0.0;  c1 = -pad;
            r2 = ih - 1;  c2 = iw - 1 + pad;
        }

        // 保存 fit 值，右键可复原
        fit_row1_ = r1;  fit_col1_ = c1;
        fit_row2_ = r2;  fit_col2_ = c2;

        hwindow_->ClearWindow();
        hwindow_->SetPart(r1, c1, r2, c2);
        hwindow_->DispObj(image);
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("DispObj failed: {}", e.ErrorMessage().Text());
    }
}

void CameraWindow::resetView()
{
    if (!hwindow_) return;
    hwindow_->SetPart(fit_row1_, fit_col1_, fit_row2_, fit_col2_);
    redisplay();
}

void CameraWindow::redisplay()
{
    if (!hwindow_ || !current_image_.IsInitialized())
        return;

    if (redisplay_pending_)
        return;
    redisplay_pending_ = true;
    QTimer::singleShot(200, this, [this]()
                       {
        redisplay_pending_ = false;
        if (!hwindow_ || !current_image_.IsInitialized())
            return;
        try
        {
            // 从 HWindow 读取当前 part，直接用
            HalconCpp::HTuple r1, c1, r2, c2;
            hwindow_->GetPart(&r1, &c1, &r2, &c2);
            hwindow_->ClearWindow();
            hwindow_->SetPart(r1, c1, r2, c2);
            hwindow_->DispObj(current_image_);
        }
        catch (HalconCpp::HException &e)
        {
            SPDLOG_ERROR("Redisplay failed: {}", e.ErrorMessage().Text());
        } });
}

bool CameraWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != ui->widget_window || !hwindow_ || !current_image_.IsInitialized())
        return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::MouseMove)
    {
        auto *me = static_cast<QMouseEvent *>(event);
        try
        {
            // Halcon 坐标转换：窗口像素 → 图像坐标
            HalconCpp::HTuple img_row, img_col;
            hwindow_->ConvertCoordinatesWindowToImage(
                me->pos().y(), me->pos().x(), &img_row, &img_col);

            double r = img_row.D(), c = img_col.D();

            HalconCpp::HTuple img_w, img_h;
            HalconCpp::GetImageSize(current_image_, &img_w, &img_h);

            if (r >= 0 && r < img_h.D() && c >= 0 && c < img_w.D())
            {
                ui->label_coordinates->setText(
                    QStringLiteral("(%1, %2)").arg(static_cast<int>(c)).arg(static_cast<int>(r)));

                // 灰度值（支持单通道和三通道）
                HalconCpp::HTuple channels;
                HalconCpp::CountChannels(current_image_, &channels);
                int row_i = static_cast<int>(r), col_i = static_cast<int>(c);

                if (channels.I() == 1)
                {
                    HalconCpp::HTuple val;
                    HalconCpp::GetGrayval(current_image_, row_i, col_i, &val);
                    ui->label_grayscale->setText(QString::number(val.I()));
                }
                else
                {
                    HalconCpp::HObject ch1, ch2, ch3;
                    HalconCpp::Decompose3(current_image_, &ch1, &ch2, &ch3);
                    HalconCpp::HTuple v1, v2, v3;
                    HalconCpp::GetGrayval(ch1, row_i, col_i, &v1);
                    HalconCpp::GetGrayval(ch2, row_i, col_i, &v2);
                    HalconCpp::GetGrayval(ch3, row_i, col_i, &v3);
                    ui->label_grayscale->setText(
                        QStringLiteral("[%1,%2,%3]").arg(v1.I()).arg(v2.I()).arg(v3.I()));
                }
            }
            else
            {
                ui->label_coordinates->setText("");
                ui->label_grayscale->setText("");
            }
        }
        catch (...)
        {
        }
        return true;
    }

    if (event->type() == QEvent::Wheel)
    {
        auto *we = static_cast<QWheelEvent *>(event);
        try
        {
            HalconCpp::HTuple img_row, img_col;
            QPointF pos = we->position();
            hwindow_->ConvertCoordinatesWindowToImage(
                pos.y(), pos.x(), &img_row, &img_col);

            double cr = img_row.D(), cc = img_col.D();
            double factor = (we->angleDelta().y() > 0) ? 0.8 : 1.25;

            HalconCpp::HTuple r1, c1, r2, c2;
            hwindow_->GetPart(&r1, &c1, &r2, &c2);

            HalconCpp::HTuple nr1 = cr + (r1.D() - cr) * factor;
            HalconCpp::HTuple nc1 = cc + (c1.D() - cc) * factor;
            HalconCpp::HTuple nr2 = cr + (r2.D() - cr) * factor;
            HalconCpp::HTuple nc2 = cc + (c2.D() - cc) * factor;

            hwindow_->SetPart(nr1, nc1, nr2, nc2);
            redisplay();
        }
        catch (...) {}
        return true;
    }

    // 中键拖拽平移
    if (event->type() == QEvent::MouseButtonPress)
    {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::MiddleButton)
        {
            dragging_ = true;
            drag_start_ = me->pos();
            hwindow_->GetPart(&drag_row1_, &drag_col1_, &drag_row2_, &drag_col2_);
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonRelease)
    {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::MiddleButton && dragging_)
        {
            dragging_ = false;
            return true;
        }
        // 右键松开 → 复原 fit-to-window
        if (me->button() == Qt::RightButton)
        {
            resetView();
            return true;
        }
    }

    if (event->type() == QEvent::MouseMove && dragging_)
    {
        auto *me = static_cast<QMouseEvent *>(event);
        double dx = me->pos().x() - drag_start_.x();
        double dy = me->pos().y() - drag_start_.y();
        double part_w = drag_col2_.D() - drag_col1_.D();
        double part_h = drag_row2_.D() - drag_row1_.D();
        int ww = ui->widget_window->width();
        int wh = ui->widget_window->height();

        double dr = -dy * part_h / wh;
        double dc = -dx * part_w / ww;

        hwindow_->SetPart(drag_row1_.D() + dr, drag_col1_.D() + dc,
                          drag_row2_.D() + dr, drag_col2_.D() + dc);
        redisplay();
        return true;
    }

    return QWidget::eventFilter(obj, event);
}

void CameraWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (hwindow_)
    {
        hwindow_.reset();
        initHalconWindow();

        // resize 后重置为 fit-to-window
        if (current_image_.IsInitialized())
            displayImage(current_image_);
    }
}

void CameraWindow::enterRoiMode()
{
    if (!hwindow_)
        return;

    // DrawRectangle1 是阻塞调用，放到独立线程执行
    std::thread([this]()
                {
        try
        {
            // 刷新显示，清除旧的 ROI 框（保持当前缩放）
            QMetaObject::invokeMethod(this, [this]()
            {
                if (current_image_.IsInitialized())
                    redisplay();
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
        catch (...) {} })
        .detach();
}
