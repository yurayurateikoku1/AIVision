#pragma once
#include <QWidget>
#include <QString>
#include <QStringList>
#include <vector>
#include <halconcpp/HalconCpp.h>

class CameraWindow;
class WorkflowMgr;
struct Context;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class OperationView;
}
QT_END_NAMESPACE

class OperationView : public QWidget
{
    Q_OBJECT

public:
    explicit OperationView(Context &ctx, WorkflowMgr &wf_mgr, QWidget *parent = nullptr);
    ~OperationView();

    /// @brief 设置离线检测图像目录
    /// @param dir_path
    /// @param filters
    void setImageDir(const QString &dir_path,
                     const QStringList &filters = {"*.bmp", "*.png", "*.jpg", "*.jpeg", "*.tif", "*.tiff"});

    /// @brief 获取图像数量
    /// @return
    int getImageCount() const { return static_cast<int>(image_paths_.size()); }

    /// @brief 设置相机窗口
    /// @param view
    void setCameraView(CameraWindow *view) { camera_view_ = view; }

private slots:
    void on_pushButton_testImage_clicked();
    void on_pushButton_previousImage_clicked();
    void on_pushButton_nextImage_clicked();

private:
    HalconCpp::HObject loadCurrentImage() const;
    void showCurrentImage();

    Ui::OperationView *ui;
    Context &ctx_;
    WorkflowMgr &wf_mgr_;
    std::vector<QString> image_paths_; // 离线图像路径数组
    int image_index_ = 0;              // 当前图像索引
    CameraWindow *camera_view_ = nullptr;
};
