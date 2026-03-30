#include "context.h"

Context &Context::getInstance()
{
    static Context instance;
    return instance;
}

Context::Context()
    : executor_(8)
{
    // 相机
    CameraControlParam camera_param;
    camera_param.name = "ccd1";
    camera_param.ip = "192.168.2.100";
    camera_param.exposure_time = 10000.0f;
    camera_param.trigger_mode = TriggerMode::Continuous;
    camera_params.emplace(camera_param.name, camera_param);

    // 通信
    comm_param.name = "plc_1";
    comm_param.protocol = CommProtocol::ModbusTCP;
    comm_param.ip = "192.168.1.200";
    comm_param.port = 502;
    comm_param.slave_address = 1;

    // 光源
    light_param.serial_port = 3;
    light_param.baud_rate = 38400;

    // 4 路工作流
    for (int i = 0; i < 4; i++)
    {
        workflows[i].di_index = i;
        workflows[i].do_ok_addr = 500;
        workflows[i].do_ng_addr = 501;
    }

    // DI0 默认启用端子检测
    workflows[0].enabled = true;
    workflows[0].detector_param = TerminalParam{};
}
