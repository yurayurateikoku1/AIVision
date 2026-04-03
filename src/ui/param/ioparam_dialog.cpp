#include "ioparam_dialog.h"
#include "ui_ioparam_dialog.h"

IOParamDialog::IOParamDialog(Context &ctx, QWidget *parent)
    : QDialog(parent), ui(new Ui::IOParamDialog), ctx_(ctx)
{
    ui->setupUi(this);
}

IOParamDialog::~IOParamDialog() { delete ui; }