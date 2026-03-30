#pragma once
#include <QWidget>
#include <QString>
#include <vector>
#include <halconcpp/HalconCpp.h>

class CameraWindow;
class WorkflowMgr;
struct Context;

QT_BEGIN_NAMESPACE
namespace Ui { class OperationView; }
QT_END_NAMESPACE

class OperationView : public QWidget
{
    Q_OBJECT

public:
    explicit OperationView(Context &ctx, WorkflowMgr &wf_mgr, QWidget *parent = nullptr);
    ~OperationView();

    void setImagePaths(const std::vector<QString> &paths);
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
    std::vector<QString> image_paths_;
    int image_index_ = 0;
    CameraWindow *camera_view_ = nullptr;
};
