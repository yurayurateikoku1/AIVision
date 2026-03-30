#pragma once
#include <QWidget>
#include "../../common.h"

QT_BEGIN_NAMESPACE
namespace Ui { class TerminalParamDialog; }
QT_END_NAMESPACE

class TerminalParamWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalParamWidget(TerminalParam &param, QWidget *parent = nullptr);
    ~TerminalParamWidget() override;

    /// @brief 将 UI 当前值写回 param_
    void apply();

private:
    Ui::TerminalParamDialog *ui;
    TerminalParam &param_;
};
