#pragma once
#include "interface_detector.h"
#include <nlohmann/json.hpp>
#include <functional>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

/// @brief 检测器注册表：各检测器在 .cpp 中通过静态变量自注册，
///        运行时按 type name 创建检测器实例。
///
/// @example
///   static bool _reg = (DetectorRegistry::reg("MyDetector",
///       [](const json& j) { return std::make_unique<MyDetector>(j.get<MyParam>()); }), true);
class DetectorRegistry
{
public:
    using CreatorFn = std::function<std::unique_ptr<InterfaceDetector>(const json &)>;

    static void registry(const std::string &name, CreatorFn fn)
    {
        getRegistryMap()[name] = std::move(fn);
    }

    static std::unique_ptr<InterfaceDetector> create(const std::string &name, const json &param)
    {
        auto &r = getRegistryMap();
        auto it = r.find(name);
        if (it == r.end())
        {
            SPDLOG_ERROR("DetectorRegistry: unknown type '{}'", name);
            return nullptr;
        }
        try
        {
            return it->second(param);
        }
        catch (const std::exception &e)
        {
            SPDLOG_ERROR("DetectorRegistry::create('{}') failed: {}", name, e.what());
            return nullptr;
        }
    }

    static std::vector<std::string> registeredNames()
    {
        std::vector<std::string> names;
        for (auto &[k, v] : getRegistryMap())
            names.push_back(k);
        return names;
    }

private:
    static std::unordered_map<std::string, CreatorFn> &getRegistryMap()
    {
        static std::unordered_map<std::string, CreatorFn> r;
        return r;
    }
};
