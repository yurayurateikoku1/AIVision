#pragma once
#include <QWidget>
#include <functional>
#include "../../common.h"
#include "terminal_param_widget.h"

class CameraWindow;

/// @brief 根据 detector_type 创建对应的检测器参数编辑 widget
///
/// 扩展新检测器时：
///   1. 创建对应的 XxxParamWidget
///   2. 在此函数中增加一个 if 分支
inline QWidget *createDetectorParamWidget(
    const std::string &detector_type,
    json &detector_param,
    std::function<void()> &apply_fn,
    int di_index = 0,
    CameraWindow *camera_view = nullptr,
    QWidget *parent = nullptr)
{
    if (detector_type == "Terminal")
    {
        auto *w = new TerminalParamWidget(detector_param, di_index, parent);
        w->setCameraWindow(camera_view);
        apply_fn = [w]() { w->applyParam(); };
        return w;
    }
    // 新检测器在此扩展: if (detector_type == "Screw") { ... }

    apply_fn = nullptr;
    return nullptr;
}
