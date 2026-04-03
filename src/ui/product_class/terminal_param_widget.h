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
/// 由 detector_param_widget_factory.h 的工厂函数创建，
/// 嵌入 WorkflowConfigDialog 的 verticalLayout_detector 区域。
///
/// 使用 terminal_param_dialog.ui 作为界面布局（虽然 ui 文件名带 Dialog，
/// 但实际作为 QWidget 嵌入，不是独立弹窗）。
///
/// 持有 TerminalParam 的引用（指向 WorkflowConfigDialog::param_.detector_param 内部），
/// applyParam() 被调用时将 UI 控件值写回该引用。
///
/// 功能：
///   - AI 推理参数：置信度阈值、NMS 阈值
///   - 形状匹配参数：缩放范围、角度范围、匹配分数、金字塔层数
///   - 模板创建：点击按钮 → 相机窗口 ROI 框选 → 裁剪 → 创建形状模型 → 保存到磁盘
///   - 模板预览：内嵌 Halcon HWindow 显示模板图
class TerminalParamWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalParamWidget(TerminalParam &param, int di_index = 0, QWidget *parent = nullptr);
    ~TerminalParamWidget() override;

    /// @brief 将 UI 控件的值写回 param_ 引用（由工厂输出的 apply_fn 调用）
    void applyParam();

    /// @brief 绑定相机窗口（用于 ROI 框选创建模板）
    void setCameraWindow(CameraWindow *cam) { camera_view_ = cam; }

private:
    /// @brief 初始化模板预览的 Halcon 窗口（延迟到 widget 尺寸确定后）
    void initTemplateWindow();
    /// @brief 在模板预览窗口中显示图像
    void displayTemplate(const HalconCpp::HObject &image);
    /// @brief 加载磁盘上已有的模板图（启动时自动调用）
    void loadExistingTemplate();

    Ui::TerminalParamDialog *ui;
    TerminalParam &param_;          // 指向 WorkflowConfigDialog::param_.detector_param 内的 TerminalParam
    int di_index_ = 0;              // DI 编号，用于模板文件命名区分
    CameraWindow *camera_view_ = nullptr;

    std::unique_ptr<HalconCpp::HWindow> template_hwindow_; // 模板预览 Halcon 窗口
    HalconCpp::HObject template_image_;                    // 当前模板图像
};
