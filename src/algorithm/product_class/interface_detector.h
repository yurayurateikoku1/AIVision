#pragma once
#include "../../common.h"

class InterfaceDetector
{
public:
    virtual ~InterfaceDetector() = default;
    virtual void detect(NodeContext &ctx) = 0;

    /// @brief 更新运行时参数（不重建模型），各子类从 json 中反序列化自己的参数
    virtual void updateParam(const json &param) = 0;
};