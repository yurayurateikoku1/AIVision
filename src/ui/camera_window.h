#pragma once
#include <QWidget>
#include <halconcpp/HalconCpp.h>
#include <string>
#include <memory>
#include "../camera/camera_interface.h"

class CameraMgr;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class CameraWindow;
}
QT_END_NAMESPACE

class CameraWindow : public QWidget, public InterfaceCameraCallback
{
    Q_OBJECT
public:
    explicit CameraWindow(const std::string &camera_name, CameraMgr &cam_mgr, QWidget *parent = nullptr);
    ~CameraWindow() override;

    void frameReceived(const std::string &camera_name, const HalconCpp::HObject &frame) override;
    void errorMsgReceived(const std::string &camera_name, int error_code, const std::string &msg) override;

    /// @brief 显示 Halcon HObject 图像
    void updateFrame(const HalconCpp::HObject &image);

    /// @brief 显示图像并叠加检测结果图形（XLD/Region）
    void updateFrameWithOverlay(const HalconCpp::HObject &image, const HalconCpp::HObject &contours, bool pass);

    void setStatus(bool online);
    std::string getCameraName() const { return camera_name_; }

    /// @brief 获取 Halcon 窗口句柄（供外部叠加图形、画 ROI 等）
    HalconCpp::HWindow *getHalconWindow() { return hwindow_.get(); }

    /// @brief 进入 ROI 框选模式，使用 Halcon DrawRectangle1 交互
    void enterRoiMode();

signals:
    /// @brief 帧到达（已在主线程，可直接连接 WorkflowManager）
    void sign_frameArrived(const std::string &camera_name, const HalconCpp::HObject &frame);
    /// @brief 相机错误
    void sign_cameraErrorArrived(const std::string &camera_name, int error_code);
    void sign_maximizeRequested(const std::string &camera_name);
    void sign_selected(const std::string &camera_name);
    /// @brief ROI 框选完成，坐标为图像坐标系
    void sign_roiSelected(double row1, double col1, double row2, double col2);
private slots:

    void on_pushButton_workflow_clicked();
    void on_pushButton_scaleWindow_clicked();

private:
    void initHalconWindow();
    void displayImage(const HalconCpp::HObject &image);
    void resizeEvent(QResizeEvent *event) override;

    Ui::CameraWindow *ui;
    CameraMgr &cam_mgr_;
    std::string camera_name_;
    bool maximized_ = false;

    std::unique_ptr<HalconCpp::HWindow> hwindow_;
    int hwindow_w_ = 0;
    int hwindow_h_ = 0;
    HalconCpp::HObject current_image_;
};