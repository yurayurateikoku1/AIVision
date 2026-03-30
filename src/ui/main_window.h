#pragma once
#include <QMainWindow>
#include <QLabel>
#include <map>

struct Context;
class CameraMgr;
class CommMgr;
class WorkflowMgr;
class CameraWindow;
class WorkflowView;
class OperationView;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Context &ctx, CameraMgr &cam_mgr, CommMgr &comm_mgr, WorkflowMgr &wf_mgr,
               QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_action_parameter_triggered();
    void on_action_camera_triggered();
    void on_action_open_folder_triggered();

private:
    void initCamera();
    void initCommunication();

    Ui::MainWindow *ui;

    Context &ctx_;
    CameraMgr &cam_mgr_;
    CommMgr &comm_mgr_;
    WorkflowMgr &wf_mgr_;

    CameraWindow *camera_view_ = nullptr;
    QLabel *comm_status_led_ = nullptr;
    QLabel *light_status_led_ = nullptr;

    WorkflowView *workflow_view_ = nullptr;
    OperationView *operation_view_ = nullptr;
};
