#pragma once
#include <QDialog>

struct Context;
class CameraMgr;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class CameraParamDialog;
}
QT_END_NAMESPACE

class CameraParamDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraParamDialog(Context &ctx, CameraMgr &cam_mgr, QWidget *parent = nullptr);
    ~CameraParamDialog();

private:
    Ui::CameraParamDialog *ui;
    Context &ctx_;
    CameraMgr &cam_mgr_;
};