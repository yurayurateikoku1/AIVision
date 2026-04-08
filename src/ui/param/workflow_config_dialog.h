#pragma once
#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <array>
#include "../../common.h"

QT_BEGIN_NAMESPACE
namespace Ui { class WorkflowConfigDialog; }
QT_END_NAMESPACE

/// @brief 四路工作流公共参数配置对话框（不含检测器参数）
class WorkflowConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit WorkflowConfigDialog(const std::array<WorkflowParam, 4> &params, QWidget *parent = nullptr);
    ~WorkflowConfigDialog() override;

    std::array<WorkflowParam, 4> getResult() const { return params_; }

private:
    QWidget *createTabPage(int index);
    void loadTab(int index);
    void applyAll();

    Ui::WorkflowConfigDialog *ui;
    std::array<WorkflowParam, 4> params_;

    struct TabWidgets {
        QSpinBox *spinBox_delayMs = nullptr;
        QSpinBox *spinBox_doOkAddr = nullptr;
        QSpinBox *spinBox_doNgAddr = nullptr;
        QSpinBox *spinBox_resultHoldMs = nullptr;
        QDoubleSpinBox *doubleSpinBox_exposure = nullptr;
    };
    std::array<TabWidgets, 4> tabs_;
};
