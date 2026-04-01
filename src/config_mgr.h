#pragma once

#include "common.h"
#include <string>

// ── JSON 序列化声明 ──────────────────────────────────────────

void to_json(json &j, const LightControlParam &p);
void from_json(const json &j, LightControlParam &p);

void to_json(json &j, const CommunicationParam &p);
void from_json(const json &j, CommunicationParam &p);

void to_json(json &j, const CameraControlParam &p);
void from_json(const json &j, CameraControlParam &p);

void to_json(json &j, const RoiParam &p);
void from_json(const json &j, RoiParam &p);

void to_json(json &j, const ShapeMatchParam &p);
void from_json(const json &j, ShapeMatchParam &p);

namespace AIInfer
{
void to_json(json &j, const YOLOSettings &p);
void from_json(const json &j, YOLOSettings &p);
}

void to_json(json &j, const TerminalParam &p);
void from_json(const json &j, TerminalParam &p);

void to_json(json &j, const WorkflowParam &p);
void from_json(const json &j, WorkflowParam &p);

// ── 配置管理器 ───────────────────────────────────────────────

class ConfigMgr
{
public:
    static ConfigMgr &getInstance();
    ~ConfigMgr();

    bool loadConfig(const std::string &path = "data/config.json");
    bool saveConfig(const std::string &path = "data/config.json");

private:
    ConfigMgr();
};
