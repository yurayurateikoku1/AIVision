#pragma once
#include <QWidget>
#include <halconcpp/HalconCpp.h>
#include <memory>
#include "../../common.h"

QT_BEGIN_NAMESPACE
namespace Ui { class TerminalParamDialog; }
QT_END_NAMESPACE

class CameraWindow;

class TerminalParamWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalParamWidget(TerminalParam &param, int di_index = 0, QWidget *parent = nullptr);
    ~TerminalParamWidget() override;

    void apply();
    void setCameraWindow(CameraWindow *cam) { camera_view_ = cam; }

private:
    void initTemplateWindow();
    void displayTemplate(const HalconCpp::HObject &image);
    void loadExistingTemplate();

    Ui::TerminalParamDialog *ui;
    TerminalParam &param_;
    int di_index_ = 0;
    CameraWindow *camera_view_ = nullptr;

    std::unique_ptr<HalconCpp::HWindow> template_hwindow_;
    HalconCpp::HObject template_image_;
};
