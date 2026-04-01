#include "config_mgr.h"
#include "context.h"
#include <fstream>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

// ── enum 序列化（宏必须在全局或对应命名空间） ────────────────

NLOHMANN_JSON_SERIALIZE_ENUM(CommProtocol, {
    {CommProtocol::ModbusRTU, "RTU"},
    {CommProtocol::ModbusTCP, "TCP"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(TriggerMode, {
    {TriggerMode::Continuous, "Continuous"},
    {TriggerMode::SoftTrigger, "SoftTrigger"},
    {TriggerMode::HardTrigger, "HardTrigger"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(DetectMethod, {
    {DetectMethod::AI, "AI"},
    {DetectMethod::MatchTemplate, "MatchTemplate"},
})

namespace AIInfer
{
NLOHMANN_JSON_SERIALIZE_ENUM(TaskType, {
    {TaskType::YOLO_DET, "YOLO_DET"},
    {TaskType::YOLO_OBB, "YOLO_OBB"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(EngineType, {
    {EngineType::OPENVINO, "OPENVINO"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(InputDimensionType, {
    {InputDimensionType::DYNAMIC, "DYNAMIC"},
})
}

// ── 结构体序列化 ─────────────────────────────────────────────

// LightControlParam
void to_json(json &j, const LightControlParam &p)
{
    j = json{{"serial_port", p.serial_port}, {"baud_rate", p.baud_rate},
             {"luminance", {p.luminance[0], p.luminance[1], p.luminance[2], p.luminance[3]}},
             {"use_modbus", p.use_modbus}};
}
void from_json(const json &j, LightControlParam &p)
{
    j.at("serial_port").get_to(p.serial_port);
    j.at("baud_rate").get_to(p.baud_rate);
    auto lum = j.at("luminance");
    for (int i = 0; i < 4; i++) p.luminance[i] = lum[i].get<int>();
    j.at("use_modbus").get_to(p.use_modbus);
}

// CommunicationParam
void to_json(json &j, const CommunicationParam &p)
{
    j = json{{"name", p.name}, {"protocol", p.protocol},
             {"ip", p.ip}, {"port", p.port},
             {"serial_port", p.serial_port}, {"baud_rate", p.baud_rate},
             {"data_bits", p.data_bits}, {"stop_bits", p.stop_bits},
             {"parity", p.parity}, {"slave_address", p.slave_address}};
}
void from_json(const json &j, CommunicationParam &p)
{
    j.at("name").get_to(p.name);
    j.at("protocol").get_to(p.protocol);
    j.at("ip").get_to(p.ip);
    j.at("port").get_to(p.port);
    j.at("serial_port").get_to(p.serial_port);
    j.at("baud_rate").get_to(p.baud_rate);
    j.at("data_bits").get_to(p.data_bits);
    j.at("stop_bits").get_to(p.stop_bits);
    j.at("parity").get_to(p.parity);
    j.at("slave_address").get_to(p.slave_address);
}

// CameraControlParam
void to_json(json &j, const CameraControlParam &p)
{
    j = json{{"name", p.name}, {"ip", p.ip},
             {"exposure_time", p.exposure_time}, {"gain", p.gain},
             {"trigger_mode", p.trigger_mode}, {"rotation_deg", p.rotation_deg}};
}
void from_json(const json &j, CameraControlParam &p)
{
    j.at("name").get_to(p.name);
    j.at("ip").get_to(p.ip);
    j.at("exposure_time").get_to(p.exposure_time);
    j.at("gain").get_to(p.gain);
    j.at("trigger_mode").get_to(p.trigger_mode);
    j.at("rotation_deg").get_to(p.rotation_deg);
}

// RoiParam
void to_json(json &j, const RoiParam &p)
{
    j = json{{"enabled", p.enabled}, {"row1", p.row1}, {"col1", p.col1},
             {"row2", p.row2}, {"col2", p.col2}};
}
void from_json(const json &j, RoiParam &p)
{
    j.at("enabled").get_to(p.enabled);
    j.at("row1").get_to(p.row1);
    j.at("col1").get_to(p.col1);
    j.at("row2").get_to(p.row2);
    j.at("col2").get_to(p.col2);
}

// ShapeMatchParam
void to_json(json &j, const ShapeMatchParam &p)
{
    j = json{{"angle_start", p.angle_start}, {"angle_extent", p.angle_extent},
             {"angle_step", p.angle_step}, {"scale_min", p.scale_min},
             {"scale_max", p.scale_max}, {"scale_step", p.scale_step},
             {"min_score", p.min_score}, {"num_matches", p.num_matches},
             {"max_overlap", p.max_overlap}, {"greediness", p.greediness},
             {"num_levels", p.num_levels}, {"metric", p.metric},
             {"template_image_path", p.template_image_path},
             {"model_path", p.model_path}};
}
void from_json(const json &j, ShapeMatchParam &p)
{
    j.at("angle_start").get_to(p.angle_start);
    j.at("angle_extent").get_to(p.angle_extent);
    j.at("angle_step").get_to(p.angle_step);
    j.at("scale_min").get_to(p.scale_min);
    j.at("scale_max").get_to(p.scale_max);
    j.at("scale_step").get_to(p.scale_step);
    j.at("min_score").get_to(p.min_score);
    j.at("num_matches").get_to(p.num_matches);
    j.at("max_overlap").get_to(p.max_overlap);
    j.at("greediness").get_to(p.greediness);
    j.at("num_levels").get_to(p.num_levels);
    j.at("metric").get_to(p.metric);
    j.at("template_image_path").get_to(p.template_image_path);
    j.at("model_path").get_to(p.model_path);
}

// AIInfer::YOLOSettings（必须在 AIInfer 命名空间内）
namespace AIInfer
{
void to_json(json &j, const YOLOSettings &p)
{
    j = json{{"model_path", p.model_path}, {"score_threshold", p.score_threshold},
             {"nms_threshold", p.nms_threshold}, {"image_stride", p.image_stride},
             {"infer_size", p.infer_size}, {"task_type", p.task_type},
             {"input_type", p.input_type}, {"engine_type", p.engine_type},
             {"end2end", p.end2end}};
}
void from_json(const json &j, YOLOSettings &p)
{
    j.at("model_path").get_to(p.model_path);
    j.at("score_threshold").get_to(p.score_threshold);
    j.at("nms_threshold").get_to(p.nms_threshold);
    j.at("image_stride").get_to(p.image_stride);
    j.at("infer_size").get_to(p.infer_size);
    j.at("task_type").get_to(p.task_type);
    j.at("input_type").get_to(p.input_type);
    j.at("engine_type").get_to(p.engine_type);
    j.at("end2end").get_to(p.end2end);
}
}

// TerminalParam
void to_json(json &j, const TerminalParam &p)
{
    j = json{{"method", p.method}, {"yolo_settings", p.yolo_settings},
             {"shape_match_param", p.shape_match_param},
             {"count", p.count}, {"parts_per_terminal", p.parts_per_terminal}};
}
void from_json(const json &j, TerminalParam &p)
{
    j.at("method").get_to(p.method);
    j.at("yolo_settings").get_to(p.yolo_settings);
    j.at("shape_match_param").get_to(p.shape_match_param);
    j.at("count").get_to(p.count);
    j.at("parts_per_terminal").get_to(p.parts_per_terminal);
}

// WorkflowParam（variant 手动处理）
void to_json(json &j, const WorkflowParam &p)
{
    j = json{{"di_index", p.di_index}, {"enabled", p.enabled},
             {"do_ok_addr", p.do_ok_addr}, {"do_ng_addr", p.do_ng_addr},
             {"result_hold_ms", p.result_hold_ms}, {"delay_ms", p.delay_ms},
             {"exposure_override", p.exposure_override}, {"roi", p.roi}};

    if (auto *tp = std::get_if<TerminalParam>(&p.detector_param))
    {
        j["detector_type"] = "Terminal";
        j["detector_param"] = *tp;
    }
    else
    {
        j["detector_type"] = "none";
    }
}
void from_json(const json &j, WorkflowParam &p)
{
    j.at("di_index").get_to(p.di_index);
    j.at("enabled").get_to(p.enabled);
    j.at("do_ok_addr").get_to(p.do_ok_addr);
    j.at("do_ng_addr").get_to(p.do_ng_addr);
    j.at("result_hold_ms").get_to(p.result_hold_ms);
    j.at("delay_ms").get_to(p.delay_ms);
    j.at("exposure_override").get_to(p.exposure_override);
    j.at("roi").get_to(p.roi);

    std::string det_type = j.value("detector_type", "none");
    if (det_type == "Terminal")
        p.detector_param = j.at("detector_param").get<TerminalParam>();
    else
        p.detector_param = std::monostate{};
}

// ── ConfigMgr 实现 ───────────────────────────────────────────

ConfigMgr &ConfigMgr::getInstance()
{
    static ConfigMgr inst;
    return inst;
}

ConfigMgr::ConfigMgr()
{
    loadConfig();
}

ConfigMgr::~ConfigMgr()
{
    saveConfig();
}

bool ConfigMgr::loadConfig(const std::string &path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open())
    {
        SPDLOG_WARN("Config file not found: {}, using defaults", path);
        return false;
    }

    try
    {
        json root = json::parse(ifs);
        auto &ctx = Context::getInstance();

        if (root.contains("cameras"))
        {
            ctx.camera_params.clear();
            for (auto &[key, val] : root["cameras"].items())
            {
                CameraControlParam cam;
                from_json(val, cam);
                ctx.camera_params[key] = cam;
            }
        }

        if (root.contains("comm"))
            from_json(root["comm"], ctx.comm_param);

        if (root.contains("light"))
            from_json(root["light"], ctx.light_param);

        if (root.contains("workflows"))
        {
            auto &wfs = root["workflows"];
            for (size_t i = 0; i < wfs.size() && i < ctx.workflows.size(); i++)
                from_json(wfs[i], ctx.workflows[i]);
        }

        SPDLOG_INFO("Config loaded from {}", path);
        return true;
    }
    catch (const json::exception &e)
    {
        SPDLOG_ERROR("Config parse error: {}", e.what());
        return false;
    }
}

bool ConfigMgr::saveConfig(const std::string &path)
{
    try
    {
        auto &ctx = Context::getInstance();
        json root;

        json cameras;
        for (auto &[name, param] : ctx.camera_params)
        {
            json cam_j;
            to_json(cam_j, param);
            cameras[name] = cam_j;
        }
        root["cameras"] = cameras;

        json comm_j;
        to_json(comm_j, ctx.comm_param);
        root["comm"] = comm_j;

        json light_j;
        to_json(light_j, ctx.light_param);
        root["light"] = light_j;

        json wfs = json::array();
        for (auto &wf : ctx.workflows)
        {
            json wf_j;
            to_json(wf_j, wf);
            wfs.push_back(wf_j);
        }
        root["workflows"] = wfs;

        std::ofstream ofs(path);
        if (!ofs.is_open())
        {
            SPDLOG_ERROR("Cannot open config file for writing: {}", path);
            return false;
        }
        ofs << root.dump(4);

        SPDLOG_INFO("Config saved to {}", path);
        return true;
    }
    catch (const std::exception &e)
    {
        SPDLOG_ERROR("Config save error: {}", e.what());
        return false;
    }
}
