#pragma once
#include <QWidget>
#include <functional>
#include "../../common.h"
#include "terminal_param_widget.h"

class CameraWindow;

/// @brief 根据 detector_param 类型创建对应的参数 widget
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
