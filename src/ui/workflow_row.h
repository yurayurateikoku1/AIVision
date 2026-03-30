#pragma once
#include <QWidget>
#include "../common.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class WorkflowRow;
}
QT_END_NAMESPACE

class WorkflowRow : public QWidget
{
    Q_OBJECT
public:
    explicit WorkflowRow(int workflow_index, const WorkflowParam &wp, QWidget *parent = nullptr);
    ~WorkflowRow() override;

    const int getWorkflowIndex() const { return workflow_index_; }

signals:

    void sign_configClicked(int workflow_index);
    void sign_roiClicked(int workflow_index);
    void sign_enabledChanged(int workflow_index, bool enabled);

private:
    Ui::WorkflowRow *ui;
    int workflow_index_;
};
