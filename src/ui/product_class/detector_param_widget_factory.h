#pragma once
#include <QWidget>
#include <functional>
#include "../../common.h"
#include "terminal_param_widget.h"

/// @brief 根据 detector_param 类型创建对应的参数 widget
/// @param apply_fn  输出参数：调用后将 UI 值写回 detector_param
/// @return nullptr 表示该工作流未绑定检测器
inline QWidget *createDetectorParamWidget(
    std::variant<std::monostate, TerminalParam> &detector_param,
    std::function<void()> &apply_fn,
    QWidget *parent = nullptr)
{
    return std::visit([&](auto &p) -> QWidget *
                      {
                          using T = std::decay_t<decltype(p)>;
                          if constexpr (std::is_same_v<T, TerminalParam>)
                          {
                              auto *w = new TerminalParamWidget(p, parent);
                              apply_fn = [w]() { w->apply(); };
                              return w;
                          }
                          else
                          {
                              apply_fn = nullptr;
                              return nullptr;
                          }
                      },
                      detector_param);
}
