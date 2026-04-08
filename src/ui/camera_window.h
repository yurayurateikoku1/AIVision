#pragma once
#include <QWidget>
#include <halconcpp/HalconCpp.h>
#include <string>
#include <memory>
#include <taskflow/taskflow.hpp>
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
    explicit CameraWindow(const std::string &camera_name, CameraMgr &cam_mgr, tf::Executor &executor, QWidget *parent = nullptr);
    ~CameraWindow() override;

    void frameReceived(const std::string &camera_name, const HalconCpp::HObject &frame) override;
    void errorMsgReceived(const std::string &camera_name, int error_code, const std::string &msg) override;

    /// @brief 显示 Halcon HObject 图像
    void updateFrame(const HalconCpp::HObject &image);

    /// @brief 显示图像并叠加检测结果图形（XLD/Region）
    /// @param show_all true=绿色显示正常+红色显示缺陷, false=仅红色显示缺陷
    void updateFrameWithOverlay(const HalconCpp::HObject &image,
                                const HalconCpp::HObject &ok_contours,
                                const HalconCpp::HObject &ng_contours,
                                bool show_all = true);

    void setStatus(bool online);

    void setResult(bool pass);

    std::string getCameraName() const { return camera_name_; }

    /// @brief 获取 Halcon 窗口句柄（供外部叠加图形、画 ROI 等）
    HalconCpp::HWindow *getHalconWindow() { return hwindow_.get(); }

    /// @brief 获取当前显示的图像
    const HalconCpp::HObject &getCurrentImage() const { return current_image_; }

    /// @brief 进入 ROI 框选模式，使用 Halcon DrawRectangle1 交互
    void enterRoiMode();

signals:
    /// @brief 帧到达（已在主线程，可直接连接 WorkflowManager）
    void sign_frameArrived(const std::string &camera_name, const HalconCpp::HObject &frame);

    /// @brief 相机错误
    void sign_cameraErrorArrived(const std::string &camera_name, int error_code);

    /// @brief 最大化
    void sign_maximizeRequested(const std::string &camera_name);

    /// @brief 选中相机
    void sign_selected(const std::string &camera_name);

    /// @brief ROI 框选完成，坐标为图像坐标系
    void sign_roiSelected(double row1, double col1, double row2, double col2);
private slots:

    void on_pushButton_workflow_clicked();
    void on_pushButton_scaleWindow_clicked();

private:
    void initHalconWindow();
    void displayImage(const HalconCpp::HObject &image);
    void redisplay();
    void dispOverlay();
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

    void resetView();

    Ui::CameraWindow *ui;
    CameraMgr &cam_mgr_;
    tf::Executor &executor_;
    std::string camera_name_;
    bool maximized_ = false;

    std::unique_ptr<HalconCpp::HWindow> hwindow_;
    int hwindow_w_ = 0;
    int hwindow_h_ = 0;
    HalconCpp::HObject current_image_;
    HalconCpp::HObject ok_contours_;   // 正常部件轮廓
    HalconCpp::HObject ng_contours_;   // 缺陷部件轮廓
    bool show_all_ = true;             // true=显示全部, false=仅显示缺陷

    // 初始 fit-to-window 的 part（用于右键复原）
    HalconCpp::HTuple fit_row1_, fit_col1_, fit_row2_, fit_col2_;
    // 拖拽平移
    bool dragging_ = false;
    QPoint drag_start_;
    HalconCpp::HTuple drag_row1_, drag_col1_, drag_row2_, drag_col2_;

    // 节流：避免高频 redisplay
    bool redisplay_pending_ = false;

};