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

/// @brief 通信协议类型
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
    double model_origin_row = 0;         // 模板区域中心 row（创建模型时记录）
    double model_origin_col = 0;         // 模板区域中心 col（创建模型时记录）
    bool display_rect = true;            // 显示外接矩形框
    bool display_contour = true;         // 显示匹配轮廓
    bool display_center = false;         // 显示匹配中心十字

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
    AIInfer,
    MatchTemplate,
};

/// @brief 部件检测 ROI（相对于模板区域中心的坐标，用于模板匹配方式）
struct PartRoi
{
    int row1 = 0, col1 = 0, row2 = 0, col2 = 0;
    bool valid() const { return row2 > row1 && col2 > col1; }
};

/// @brief Blob 检查参数
struct BlobInspectParam
{
    int threshold = 128;
    int area_min = 0;
    int area_max = 999999;
};

/// @brief 边缘测量检查参数
struct MeasureInspectParam
{
    int contrast_min = 20;
    int contrast_max = 255;
    int feature_limit = 2;
    bool check_ge = true; // true: edge_count >= limit 为 OK; false: edge_count <= limit 为 OK
};

/// @brief 部件检查器参数
struct PartInspector
{
    bool enabled = false;
    PartRoi roi;
    std::variant<BlobInspectParam, MeasureInspectParam> param;
};

/// @brief 部件类别 ID（端子检测器使用）
enum class TermPartCls : int
{
    TERM = 0, // 整个端子（锚框）
    IB = 1,   // 外卯
    INS = 2,  // 胶皮
    BS = 3,   // 下铜丝
    WB = 4,   // 内贸
    TS = 5,   // 上铜丝
    HEAD = 6, // 端子头
};

/// @brief 端子检测参数
struct TerminalParam
{
    DetectMethod method = DetectMethod::AIInfer;
    AIInfer::YOLOSettings yolo_settings{
        .model_path = "data/model/DZ.xml",
        .score_threshold = 0.5f,
        .nms_threshold = 0.5f,
        .image_stride = 32,
        .task_type = AIInfer::TaskType::YOLO_OBB,
        .engine_type = AIInfer::EngineType::OPENVINO};
    ShapeMatchParam shape_match_param;
    int count_TERM = 1;         // 预期端子数量
    int parts_per_terminal = 0; // 预期部件数

    /// 部件检查器表：key = PartCls 的 int 值
    std::map<int, PartInspector> part_inspectors;
};

/// @brief 检测器结果
using DetectorResult = std::variant<std::monostate, ShapeMatchResult>;

/// @brief 单个缺陷（检测器负责填充）
struct Defect
{
    int defect_id = 0;          // 缺陷部件类别 ID
    HalconCpp::HObject contour; // 缺陷区域轮廓
};

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
    HalconCpp::HObject ok_contours;       // 正常部件轮廓（绿色显示）
    HalconCpp::HObject ng_contours;       // 缺陷部件轮廓（红色显示）
    std::map<std::string, std::any> data; // 扩展数据
};

/// @brief ROI 裁剪（通用前处理）
struct RoiParam
{
    bool enable_roi = false;
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

    /// 检测器类型名称（"Terminal" 等），空 = 不检测
    std::string detector_type;
    /// 检测器参数（JSON 格式，由各检测器自行序列化/反序列化）
    json detector_param;
};
