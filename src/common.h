#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <any>
#include <map>
#include <variant>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <halconcpp/HalconCpp.h>
#include "ai_infer/yolo.h"
using json = nlohmann::json;

struct Defer
{
public:
    Defer(std::function<void()> func) : func_(func) {}
    ~Defer() { func_(); }

private:
    std::function<void()> func_;
};

/// @brief 通信协议类
enum class CommProtocol
{
    ModbusRTU, // 串口 → PLC/外设
    ModbusTCP, // 网口 → PLC
};

/// @brief Modbus 地址定义（对应从站内存布局）
///        0X 区 = 线圈 (FC01 读 / FC05 写单个)，地址范围 0~4095
///        4X 区 = 保持寄存器 (FC03 读 / FC06 写单个 / FC10 写多个)，地址范围 0~4095
namespace ModbusAddr
{
    // ── 0X 线圈区：DI 输入信号（从 PLC 读） ──
    constexpr uint16_t DI0_STRIP_VISION = 0; // DI0 剥皮视觉信号
    constexpr uint16_t DI1_SHELL_VISION = 1; // DI1 胶壳视觉信号
    constexpr uint16_t DI2 = 2;              // DI2 预留
    constexpr uint16_t DI3 = 3;              // DI3 预留
    constexpr uint16_t DI4_BACKLIGHT1 = 4;   // DI4 启动背光源1
    constexpr uint16_t DI5_SIDELIGHT2 = 5;   // DI5 启动侧光源2
    constexpr uint16_t DI6_LIGHT3 = 6;       // DI6 启动光源3
    constexpr uint16_t DI7_LIGHT4 = 7;       // DI7 启动光源4

    // ── 0X 线圈区：DO 输出信号（写到 PLC，地址 500~507，对应 Modbus_0x[500+i]） ──
    constexpr uint16_t DO0_VISION_OK = 500;       // DO0 视觉OK信号
    constexpr uint16_t DO1_VISION_NG = 501;       // DO1 视觉NG信号
    constexpr uint16_t DO2 = 502;                 // DO2 预留
    constexpr uint16_t DO3 = 503;                 // DO3 预留
    constexpr uint16_t DO4_TRIG_BACKLIGHT1 = 504; // DO4 触发背光源1
    constexpr uint16_t DO5_TRIG_SIDELIGHT2 = 505; // DO5 触发侧光源2
    constexpr uint16_t DO6_TRIG_LIGHT3 = 506;     // DO6 触发光源3
    constexpr uint16_t DO7_TRIG_LIGHT4 = 507;     // DO7 触发光源4

    // ── 4X 保持寄存器区 ──
    constexpr uint16_t REG_RESULT = 0; // 检测结果: 1=OK, 0=NG
    constexpr uint16_t REG_STATUS = 1; // 系统状态
}

/// @brief 光源控制参数
struct LightControlParam
{
    int serial_port = 3;             // 光源控制器串口
    int baud_rate = 38400;           // 波特率（光源控制器固定38400）
    int luminance[4] = {0, 0, 0, 0}; // 4路通道亮度 0~255
    bool use_modbus = false;         // true=通过Modbus/PLC控制, false=串口直接控制
};

/// @brief 通信参数
struct CommunicationParam
{
    std::string name;

    CommProtocol protocol = CommProtocol::ModbusRTU;

    // TCP
    std::string ip;
    int port = 502;

    // RTU
    int serial_port = 3;
    int baud_rate = 9600;
    int data_bits = 8;
    int stop_bits = 1; // 1 或 2
    int parity = 0;    // 0=None, 1=Even, 2=Odd
    int slave_address = 1;
};

/// @brief 触发模式
enum class TriggerMode
{
    // 0=连续采集, 1=软触发, 2=硬触发
    Continuous = 0,
    SoftTrigger = 1,
    HardTrigger = 2,
};

/// @brief 相机控制参数
struct CameraControlParam
{
    std::string name = "ccd1";
    std::string ip;
    float exposure_time = 10000.0f;
    float gain = 0.0f;
    TriggerMode trigger_mode = TriggerMode::Continuous;
    int rotation_deg = 0;
};

/// @brief 单个缺陷（检测器负责填充）
struct Defect
{
    std::string label; // 缺陷类型，如 "missing", "offset"
    float confidence = 0.0f;
    HalconCpp::HObject contour; // 缺陷区域轮廓（XLD/Region，任意形状）
};

/// @brief 形状匹配参数
struct ShapeMatchParam
{
    double angle_start = -0.39;          // 起始角度 (rad)
    double angle_extent = 0.78;          // 角度范围 (rad)
    double angle_step = 0.0;             // 角度步长，0=自动
    double scale_min = 0.9;              // 最小缩放
    double scale_max = 1.1;              // 最大缩放
    double scale_step = 0.0;             // 缩放步长，0=自动
    double min_score = 0.5;              // 最低匹配分数
    int num_matches = 1;                 // 最大匹配数量
    double max_overlap = 0.5;            // 最大重叠比
    double greediness = 0.9;             // 贪心度 (0~1)
    int num_levels = 0;                  // 金字塔层数，0=自动
    std::string metric = "use_polarity"; // 匹配极性
    std::string template_image_path;     // 模板图路径
    std::string model_path;              // 形状模型文件路径 (.shm)
};

/// @brief 形状匹配结果
struct ShapeMatchResult
{
    HalconCpp::HTuple row;
    HalconCpp::HTuple col;
    HalconCpp::HTuple angle;
    HalconCpp::HTuple scale;
    HalconCpp::HTuple score;

    int count() const { return static_cast<int>(score.Length()); }
};

/// @brief 检测方法
enum class DetectMethod
{
    AI,
    MatchTemplate,
};

/// @brief 端子检测参数
struct TerminalParam
{
    DetectMethod method = DetectMethod::AI;
    AIInfer::YOLOSettings yolo_settings{
        .model_path = "data/model/DZ.xml",
        .score_threshold = 0.5f,
        .nms_threshold = 0.5f,
        .image_stride = 32,
        .task_type = AIInfer::TaskType::YOLO_OBB,
        .engine_type = AIInfer::EngineType::OPENVINO};
    ShapeMatchParam shape_match_param;
    int count = 1;              // 预期数量
    int parts_per_terminal = 0; // 预期部件数
};

/// @brief 端子单个部件的检测结果
struct TerminalPart
{
    int part_id;                 // 部件名称，如 "pin", "housing", "lock" 等
    float confidence = 0.0f;     // 检测置信度
    float x = 0, y = 0;          // 左上角像素坐标
    float width = 0, height = 0; // 像素尺寸（宽、高）
};

/// @brief 单个端子的检测结果
struct TerminalResult
{
    int id = 0;                      // 端子编号
    std::vector<TerminalPart> parts; // 部件列表
    /// @brief 部件数量是否达标（expected<=0 表示不检查）
    bool is_complete(int expected) const { return expected <= 0 || static_cast<int>(parts.size()) >= expected; }
};

/// @brief 检测器结果（与 WorkflowParam::detector_param 的 variant 对应）
using DetectorResult = std::variant<std::monostate, std::vector<TerminalResult>, ShapeMatchResult>;

/// @brief 检测结果
struct InspectionResult
{
    bool pass = true;
    std::vector<Defect> defects;
    DetectorResult detector_result; // 检测器专属结果
    int64_t timestamp_ms = 0;
};

/// @brief 检测节点上下文
struct NodeContext
{
    InspectionResult result;
    std::string camera_name;
    HalconCpp::HObject src_image;         // 检测用图（可能是 ROI 裁剪后的）
    HalconCpp::HObject display_image;     // 显示用原图（始终为全图）
    HalconCpp::HObject result_contours;   // 检测器输出的可视化图形（XLD/Region，坐标基于 image）
    std::map<std::string, std::any> data; // 扩展数据
};

/// @brief ROI 裁剪（通用前处理）
struct RoiParam
{
    bool enabled = false;
    int row1 = 0, col1 = 0, row2 = 0, col2 = 0;
};

/// @brief 工作流参数（每路 DI 对应一个，下标 = DI 编号 0~3）
struct WorkflowParam
{
    int di_index = 0;
    bool enabled = false;

    // DO 输出
    uint16_t do_ok_addr = 500;
    uint16_t do_ng_addr = 501;
    int result_hold_ms = 100;

    // 检测前处理
    int delay_ms = 0;
    float exposure_override = -1.0f; // <=0 不覆盖
    RoiParam roi;

    /// 检测器参数：monostate = 不检测，
    /// TerminalParam = 端子检测
    /// ConductorParam = 电缆检测
    /// PlasticCasingParam =检测
    std::variant<std::monostate, TerminalParam> detector_param;
};
