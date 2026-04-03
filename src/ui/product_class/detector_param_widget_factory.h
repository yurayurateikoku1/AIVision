#pragma once
#include <QWidget>
#include <functional>
#include "../../common.h"
#include "terminal_param_widget.h"

class CameraWindow;

/// @brief 根据 detector_param 的 variant 类型，创建对应的检测器参数编辑 widget
///
/// 工作原理：
///   对 detector_param 做 std::visit，按持有的类型分发：
///     - TerminalParam → 创建 TerminalParamWidget（端子检测参数面板）
///     - monostate     → 返回 nullptr（无检测器，不创建 widget）
///
///   同时输出 apply_fn：绑定到子 widget 的 applyParam()，
///   供 WorkflowConfigDialog 在用户点击确定时调用，将 UI 值写回 param。
///
/// 扩展新检测器时：
///   1. 在 common.h 的 WorkflowParam::detector_param variant 中添加新类型
///   2. 创建对应的 XxxParamWidget
///   3. 在此函数中增加一个 if constexpr 分支
///
/// @param detector_param 检测器参数 variant 的引用（子 widget 直接修改它）
/// @param apply_fn       输出参数，绑定子 widget 的提交函数
/// @param di_index       DI 编号，用于模板文件命名区分
/// @param camera_view    相机窗口，用于 ROI 框选创建模板
/// @param parent         父 widget
/// @return 创建的参数 widget，或 nullptr（monostate 时）
inline QWidget *createDetectorParamWidget(
    std::variant<std::monostate, TerminalParam> &detector_param,
    std::function<void()> &apply_fn,
    int di_index = 0,
    CameraWindow *camera_view = nullptr,
    QWidget *parent = nullptr)
{
    return std::visit([&](auto &p) -> QWidget *
                      {
                          using T = std::decay_t<decltype(p)>;
                          if constexpr (std::is_same_v<T, TerminalParam>)
                          {
                              auto *w = new TerminalParamWidget(p, di_index, parent);
                              w->setCameraWindow(camera_view);
                              apply_fn = [w]() { w->applyParam(); };
                              return w;
                          }
                          else
                          {
                              apply_fn = nullptr;
                              return nullptr;
                          } },
                      detector_param);
}
