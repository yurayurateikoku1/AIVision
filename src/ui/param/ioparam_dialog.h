#pragma once
#include <QDialog>

struct Context;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class IOParamDialog;
}
QT_END_NAMESPACE

class IOParamDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IOParamDialog(Context &ctx, QWidget *parent = nullptr);
    ~IOParamDialog();

private:
    Ui::IOParamDialog *ui;
    Context &ctx_;
};