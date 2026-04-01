#pragma once
#include <QDialog>
#include <functional>
#include "../common.h"

QT_BEGIN_NAMESPACE
namespace Ui { class WorkflowConfigDialog; }
QT_END_NAMESPACE

class CameraWindow;

class WorkflowConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WorkflowConfigDialog(const WorkflowParam &param, CameraWindow *camera_view = nullptr, QWidget *parent = nullptr);
    ~WorkflowConfigDialog() override;

    /// @brief 返回编辑后的参数副本
    WorkflowParam getResult() const { return param_; }

private:
    void loadCommon();
    void applyCommon();

    Ui::WorkflowConfigDialog *ui;
    WorkflowParam param_;  // 本地副本
    std::function<void()> detector_apply_fn_;
};
