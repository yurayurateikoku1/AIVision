#pragma once
#include <QWidget>
#include <string>

class CameraWindow;
class WorkflowMgr;
struct Context;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class WorkflowView;
}
QT_END_NAMESPACE

class WorkflowView : public QWidget
{
    Q_OBJECT

public:
    explicit WorkflowView(Context &ctx, WorkflowMgr &wf_mgr, QWidget *parent = nullptr);
    ~WorkflowView() override;

    void setCameraView(CameraWindow *view) { camera_view_ = view; }

    /// @brief 刷新 4 路 workflow 列表
    void initPage();

    /// @brief 设置选中行
    void setSelectedRow(int di_index);

signals:
    void sign_workflowSelected(int di_index);

private:
    Ui::WorkflowView *ui;
    Context &ctx_;
    WorkflowMgr &wf_mgr_;
    CameraWindow *camera_view_ = nullptr;
};
