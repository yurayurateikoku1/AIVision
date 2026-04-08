#pragma once
#include <QWidget>
#include <halconcpp/HalconCpp.h>
#include <memory>
#include "../../common.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class TerminalParamDialog;
}
QT_END_NAMESPACE

class CameraWindow;

/// @brief 端子检测参数编辑 widget
///
/// 持有 json 引用（指向 WorkflowParam::detector_param），
/// 内部反序列化为 TerminalParam 工作副本，applyParam() 时序列化回 json。
class TerminalParamWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalParamWidget(json &detector_param_json, int di_index = 0, QWidget *parent = nullptr);
    ~TerminalParamWidget() override;

    /// @brief 将 UI 控件值写回内部 param_，再序列化回 json 引用
    void applyParam();

    /// @brief 设置相机窗口
    /// @param cam
    void setCameraWindow(CameraWindow *cam) { camera_view_ = cam; }

private:
    void initTemplateWindow();
    void displayTemplate(const HalconCpp::HObject &image);
    void loadExistingTemplate();
    void drawPartRoi(PartRoi &roi);
    void displayAllRois();

    /// @brief 获取或创建指定部件的 PartInspector 引用
    PartInspector &inspector(TermPartCls cls);

    Ui::TerminalParamDialog *ui;
    json &param_json_;    // 指向 WorkflowParam::detector_param
    TerminalParam param_; // 工作副本（从 json 反序列化）
    int di_index_ = 0;
    CameraWindow *camera_view_ = nullptr;

    std::unique_ptr<HalconCpp::HWindow> template_hwindow_;
    HalconCpp::HObject template_image_;
};
