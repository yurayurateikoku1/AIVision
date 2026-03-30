#include "hik_camera.h"
#include <spdlog/spdlog.h>
#include <unordered_map>

HikCamera::HikCamera()
{
}

HikCamera::~HikCamera()
{
    closeCamera();
}

bool HikCamera::openCamera(const CameraControlParam &cam_param)
{
    if (flag_opened_)
    {
        SPDLOG_WARN("Camera:{} has been opened", cam_param.name);
    }

    // 枚举设备并按 IP 匹配
    MV_CC_DEVICE_INFO_LIST device_list{};
    int ret = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &device_list);
    if (ret != MV_OK || device_list.nDeviceNum == 0)
    {
        SPDLOG_ERROR("No devices found for camera {}", cam_param.name);
        return false;
    }

    MV_CC_DEVICE_INFO *target = nullptr;
    for (unsigned int i = 0; i < device_list.nDeviceNum; ++i)
    {
        auto *info = device_list.pDeviceInfo[i];
        if (!info)
            continue;
        if (info->nTLayerType == MV_GIGE_DEVICE)
        {
            auto &gige = info->SpecialInfo.stGigEInfo;
            char ip[32];
            snprintf(ip, sizeof(ip), "%d.%d.%d.%d",
                     (gige.nCurrentIp >> 24) & 0xFF,
                     (gige.nCurrentIp >> 16) & 0xFF,
                     (gige.nCurrentIp >> 8) & 0xFF,
                     gige.nCurrentIp & 0xFF);
            if (cam_param.ip == ip)
            {
                target = info;
                break;
            }
        }
    }

    if (!target)
    {
        SPDLOG_ERROR("Camera {} with IP {} not found", cam_param.name, cam_param.ip);
        return false;
    }

    return openCamera(cam_param, target);
}

bool HikCamera::openCamera(const CameraControlParam &cam_param, MV_CC_DEVICE_INFO *dev_info)
{
    if (flag_opened_)
    {
        SPDLOG_WARN("Camera {} already opened", cam_param.name);
        return true;
    }
    camera_control_param_ = cam_param;

    int ret = MV_CC_CreateHandle(&handler_, dev_info);
    if (ret != MV_OK)
    {
        SPDLOG_ERROR("CreateHandle failed: 0x{:08X}", ret);
        return false;
    }

    ret = MV_CC_OpenDevice(handler_);
    if (ret != MV_OK)
    {
        SPDLOG_ERROR("OpenDevice failed: 0x{:08X}", ret);
        MV_CC_DestroyHandle(handler_);
        handler_ = nullptr;
        return false;
    }

    // 注册异常回调（掉线检测）
    MV_CC_RegisterExceptionCallBack(handler_, exceptionCallback, this);

    setExposureTime(cam_param.exposure_time);
    setGain(cam_param.gain);
    setTriggerMode(cam_param.trigger_mode);

    flag_opened_ = true;
    SPDLOG_INFO("Camera {} opened", cam_param.name);
    return true;
}

void HikCamera::closeCamera()
{
    void *h = handler_;
    if (!h)
        return;

    // 标记关闭 — imageCallback 检查后直接 return，不再持锁
    flag_opened_ = false;
    flag_is_grabbing_ = false;

    // 停止抓取（SDK 内部会等待回调退出）
    MV_CC_StopGrabbing(h);

    // 回调已全部退出，安全释放资源
    std::lock_guard lock(handler_mutex_);
    if (handler_)
    {
        MV_CC_CloseDevice(handler_);
        MV_CC_DestroyHandle(handler_);
        handler_ = nullptr;
    }
}

bool HikCamera::isOpened() const
{
    return flag_opened_;
}

bool HikCamera::startGrabbing()
{
    if (!flag_opened_ || flag_is_grabbing_)
        return false;

    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;

    int ret = MV_CC_RegisterImageCallBackEx(handler_, frameCallback, this);
    if (ret != MV_OK)
    {
        SPDLOG_ERROR("RegisterImageCallback failed: 0x{:08X}", ret);
        return false;
    }

    ret = MV_CC_StartGrabbing(handler_);
    if (ret != MV_OK)
    {
        SPDLOG_ERROR("StartGrabbing failed: 0x{:08X}", ret);
        return false;
    }
    flag_is_grabbing_ = true;
    return true;
}

void HikCamera::stopGrabbing()
{
    if (!flag_is_grabbing_ || !handler_)
        return;
    flag_is_grabbing_ = false;
    MV_CC_StopGrabbing(handler_);
}

bool HikCamera::isGrabbing() const
{
    return flag_is_grabbing_;
}

bool HikCamera::grabOne(HalconCpp::HObject &frame, int timeout_ms)
{
    if (!flag_opened_)
        return false;
    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;

    MV_FRAME_OUT frame_out{};
    int ret = MV_CC_GetImageBuffer(handler_, &frame_out, timeout_ms);
    if (ret != MV_OK)
    {
        spdlog::error("GetImageBuffer failed: 0x{:08X}", ret);
        return false;
    }

    frame = convert2Hobject(frame_out.pBufAddr, &frame_out.stFrameInfo);
    MV_CC_FreeImageBuffer(handler_, &frame_out);
    return frame.IsInitialized();
}

bool HikCamera::setExposureTime(float us)
{
    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;
    MV_CC_SetEnumValue(handler_, "ExposureAuto", 0);
    int ret = MV_CC_SetFloatValue(handler_, "ExposureTime", us);
    return ret == MV_OK;
}

bool HikCamera::getExposureTime(float &us)
{
    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;
    MVCC_FLOATVALUE val{};
    int ret = MV_CC_GetFloatValue(handler_, "ExposureTime", &val);
    if (ret != MV_OK)
        return false;
    us = val.fCurValue;
    return true;
}

bool HikCamera::setGain(float gain)
{
    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;
    MV_CC_SetEnumValue(handler_, "GainAuto", 0);
    int ret = MV_CC_SetFloatValue(handler_, "Gain", gain);
    if (ret != MV_OK)
    {
        // 部分型号使用 GainRaw 节点
        ret = MV_CC_SetFloatValue(handler_, "GainRaw", gain);
    }
    return ret == MV_OK;
}

bool HikCamera::getGain(float &gain)
{
    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;
    MVCC_FLOATVALUE val{};
    int ret = MV_CC_GetFloatValue(handler_, "Gain", &val);
    if (ret != MV_OK)
    {
        // 部分型号使用 GainRaw 节点
        ret = MV_CC_GetFloatValue(handler_, "GainRaw", &val);
    }
    if (ret != MV_OK)
    {
        SPDLOG_WARN("getGain failed: 0x{:08X}", ret);
        return false;
    }
    gain = val.fCurValue;
    return true;
}

bool HikCamera::setTriggerMode(TriggerMode mode)
{
    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;
    if (mode == TriggerMode::Continuous)
    {
        MV_CC_SetEnumValue(handler_, "TriggerMode", 0);
    }
    else
    {
        MV_CC_SetEnumValue(handler_, "TriggerMode", 1);
        MV_CC_SetEnumValue(handler_, "TriggerSource",
                           mode == TriggerMode::SoftTrigger ? 7 /*Software*/ : 0 /*Line0*/);
    }
    return true;
}

bool HikCamera::getTriggerMode(TriggerMode &mode)
{
    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;
    MVCC_ENUMVALUE val{};
    int ret = MV_CC_GetEnumValue(handler_, "TriggerMode", &val);
    if (ret != MV_OK)
        return false;
    if (val.nCurValue == 0)
    {
        mode = TriggerMode::Continuous; // 连续采集
    }
    else
    {
        MVCC_ENUMVALUE src{};
        MV_CC_GetEnumValue(handler_, "TriggerSource", &src);
        mode = (src.nCurValue == 7) ? TriggerMode::SoftTrigger : TriggerMode::HardTrigger; // 7=Software, 0=Line0
    }
    return true;
}

bool HikCamera::setFrameRate(float fps)
{
    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;
    MV_CC_SetBoolValue(handler_, "AcquisitionFrameRateEnable", true);
    int ret = MV_CC_SetFloatValue(handler_, "AcquisitionFrameRate", fps);
    return ret == MV_OK;
}

bool HikCamera::getFrameRate(float &fps)
{
    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;
    MVCC_FLOATVALUE val{};
    int ret = MV_CC_GetFloatValue(handler_, "AcquisitionFrameRate", &val);
    if (ret != MV_OK)
        return false;
    fps = val.fCurValue;
    return true;
}

bool HikCamera::setPixelFormat(const std::string &format)
{
    static const std::unordered_map<std::string, unsigned int> fmt_map = {
        {"Mono8", PixelType_Gvsp_Mono8},
        {"Mono10", PixelType_Gvsp_Mono10},
        {"Mono12", PixelType_Gvsp_Mono12},
        {"BayerRG8", PixelType_Gvsp_BayerRG8},
        {"BayerGB8", PixelType_Gvsp_BayerGB8},
        {"BayerGR8", PixelType_Gvsp_BayerGR8},
        {"BayerBG8", PixelType_Gvsp_BayerBG8},
        {"RGB8Packed", PixelType_Gvsp_RGB8_Packed},
        {"YUV422_8", PixelType_Gvsp_YUV422_Packed},
    };
    auto it = fmt_map.find(format);
    if (it == fmt_map.end())
    {
        SPDLOG_ERROR("Unknown pixel format: {}", format);
        return false;
    }
    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;
    int ret = MV_CC_SetEnumValue(handler_, "PixelFormat", it->second);
    return ret == MV_OK;
}

bool HikCamera::getPixelFormat(std::string &format)
{
    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;
    MVCC_ENUMVALUE val{};
    int ret = MV_CC_GetEnumValue(handler_, "PixelFormat", &val);
    if (ret != MV_OK)
        return false;
    // 反向映射枚举值到字符串
    static const std::unordered_map<unsigned int, std::string> fmt_map = {
        {PixelType_Gvsp_Mono8, "Mono8"},
        {PixelType_Gvsp_Mono10, "Mono10"},
        {PixelType_Gvsp_Mono12, "Mono12"},
        {PixelType_Gvsp_BayerRG8, "BayerRG8"},
        {PixelType_Gvsp_BayerGB8, "BayerGB8"},
        {PixelType_Gvsp_BayerGR8, "BayerGR8"},
        {PixelType_Gvsp_BayerBG8, "BayerBG8"},
        {PixelType_Gvsp_RGB8_Packed, "RGB8Packed"},
        {PixelType_Gvsp_YUV422_Packed, "YUV422_8"},
    };
    auto it = fmt_map.find(val.nCurValue);
    if (it != fmt_map.end())
        format = it->second;
    else
        format = "Unknown(" + std::to_string(val.nCurValue) + ")";
    return true;
}

bool HikCamera::softTrigger()
{
    std::lock_guard lock(handler_mutex_);
    if (!handler_)
        return false;
    return MV_CC_SetCommandValue(handler_, "TriggerSoftware") == MV_OK;
}

std::string HikCamera::getName() const
{
    return camera_control_param_.name;
}

void HikCamera::setCallback(InterfaceCameraCallback *callback)
{
    callback_ = callback;
}

bool HikCamera::reconnect()
{
    SPDLOG_INFO("Camera {} attempting reconnect...", camera_control_param_.name);

    // 强制清理旧连接（无论状态标记如何，确保 SDK 资源释放）
    forceCloseCamera();

    // 重新枚举，查找匹配 IP 的设备
    auto devices = enumDevices();
    MV_CC_DEVICE_INFO *target = nullptr;
    for (auto &dev : devices)
    {
        if (dev.nTLayerType == MV_GIGE_DEVICE)
        {
            auto &gige = dev.SpecialInfo.stGigEInfo;
            char ip[32];
            snprintf(ip, sizeof(ip), "%d.%d.%d.%d",
                     (gige.nCurrentIp >> 24) & 0xFF,
                     (gige.nCurrentIp >> 16) & 0xFF,
                     (gige.nCurrentIp >> 8) & 0xFF,
                     gige.nCurrentIp & 0xFF);
            if (camera_control_param_.ip == ip)
            {
                target = &dev;
                break;
            }
        }
    }

    if (!target)
    {
        SPDLOG_WARN("Camera {} (IP: {}) not found during reconnect", camera_control_param_.name, camera_control_param_.ip);
        return false;
    }

    // 重新打开
    if (!openCamera(camera_control_param_, target))
        return false;

    // 重新开始采集
    if (!startGrabbing())
    {
        closeCamera();
        return false;
    }

    SPDLOG_INFO("Camera {} reconnected successfully", camera_control_param_.name);
    return true;
}

bool HikCamera::heartbeat() const
{
    if (!handler_ || !flag_opened_)
        return false;

    // 通过 Control Channel 读取只读寄存器判断相机是否在线
    // DeviceTemperature 是 GigE Vision 标准只读属性，读取走 GVCP ReadReg
    MVCC_FLOATVALUE val{};
    auto *self = const_cast<HikCamera *>(this);
    std::lock_guard lock(self->handler_mutex_);
    if (!self->handler_)
        return false;

    int ret = MV_CC_GetFloatValue(self->handler_, "DeviceTemperature", &val);
    if (ret == MV_OK)
        return true;

    // 部分型号不支持 DeviceTemperature，回退读 ExposureTime（同样走 Control Channel）
    ret = MV_CC_GetFloatValue(self->handler_, "ExposureTime", &val);
    return ret == MV_OK;
}

void HikCamera::forceCloseCamera()
{
    void *h = handler_;
    if (!h)
    {
        flag_opened_ = false;
        flag_is_grabbing_ = false;
        return;
    }

    flag_opened_ = false;
    flag_is_grabbing_ = false;

    MV_CC_StopGrabbing(h);

    std::lock_guard lock(handler_mutex_);
    if (handler_)
    {
        MV_CC_CloseDevice(handler_);
        MV_CC_DestroyHandle(handler_);
        handler_ = nullptr;
    }
}

std::vector<MV_CC_DEVICE_INFO> HikCamera::enumDevices()
{
    std::vector<MV_CC_DEVICE_INFO> result;
    MV_CC_DEVICE_INFO_LIST device_list{};
    int ret = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &device_list);
    if (ret != MV_OK)
    {
        SPDLOG_ERROR("EnumDevices failed: 0x{:08X}", ret);
        return result;
    }
    for (unsigned int i = 0; i < device_list.nDeviceNum; ++i)
    {
        if (device_list.pDeviceInfo[i])
        {
            result.push_back(*device_list.pDeviceInfo[i]);
        }
    }
    SPDLOG_INFO("Found {} HIK devices", result.size());
    return result;
}

void __stdcall HikCamera::frameCallback(unsigned char *p_data, MV_FRAME_OUT_INFO_EX *p_frameinfo, void *p_user)
{
    auto *self = static_cast<HikCamera *>(p_user);
    if (!self->flag_opened_ || !self->callback_ || !p_data || !p_frameinfo)
        return;

    try
    {
        HalconCpp::HObject frame;
        {
            std::lock_guard lock(self->handler_mutex_);
            if (!self->handler_)
                return;
            frame = self->convert2Hobject(p_data, p_frameinfo);
        }
        if (frame.IsInitialized())
        {
            self->callback_->frameReceived(self->camera_control_param_.name, frame);
        }
    }
    catch (HalconCpp::HException &e)
    {
        SPDLOG_ERROR("imageCallback HException: {}", e.ErrorMessage().Text());
    }
    catch (std::exception &e)
    {
        SPDLOG_ERROR("imageCallback exception: {}", e.what());
    }
}

void __stdcall HikCamera::exceptionCallback(unsigned int msg_type, void *p_user)
{
    auto *self = static_cast<HikCamera *>(p_user);
    SPDLOG_ERROR("Camera {} exception: 0x{:08X}", self->camera_control_param_.name, msg_type);
    // 不在 SDK 回调线程中直接操作 handle_ 或修改状态，
    // 仅通知上层（MainWindow::onCameraError 会投递到主线程调 markOffline）
    if (self->callback_)
    {
        self->callback_->errorMsgReceived(self->camera_control_param_.name, static_cast<int>(msg_type), "device exception/disconnected");
    }
}

HalconCpp::HObject HikCamera::convert2Hobject(unsigned char *p_data, MV_FRAME_OUT_INFO_EX *p_frameinfo)
{
    int width = p_frameinfo->nWidth;
    int height = p_frameinfo->nHeight;
    auto pixel_type = p_frameinfo->enPixelType;
    HalconCpp::HObject image;

    // Mono8：直接生成单通道图像
    if (pixel_type == PixelType_Gvsp_Mono8)
    {
        HalconCpp::GenImage1(&image, "byte", width, height, reinterpret_cast<Hlong>(p_data));
        return image;
    }

    // Bayer 格式：先生成单通道原始图，再用 Halcon CfaToRgb 转彩色
    if (pixel_type == PixelType_Gvsp_BayerRG8 ||
        pixel_type == PixelType_Gvsp_BayerGB8 ||
        pixel_type == PixelType_Gvsp_BayerGR8 ||
        pixel_type == PixelType_Gvsp_BayerBG8)
    {
        try
        {
            HalconCpp::HObject bayer_image;
            HalconCpp::GenImage1(&bayer_image, "byte", width, height, reinterpret_cast<Hlong>(p_data));

            // Halcon Bayer 模式字符串
            const char *cfa = "bayer_rg"; // 默认
            if (pixel_type == PixelType_Gvsp_BayerGB8)
                cfa = "bayer_gb";
            else if (pixel_type == PixelType_Gvsp_BayerGR8)
                cfa = "bayer_gr";
            else if (pixel_type == PixelType_Gvsp_BayerBG8)
                cfa = "bayer_bg";

            HalconCpp::CfaToRgb(bayer_image, &image, cfa, "bilinear");
            return image;
        }
        catch (HalconCpp::HException &e)
        {
            SPDLOG_ERROR("CfaToRgb failed: {}", e.ErrorMessage().Text());
            return {};
        }
    }

    // RGB8Packed：直接拆分 R/G/B 平面
    if (pixel_type == PixelType_Gvsp_RGB8_Packed)
    {
        std::vector<unsigned char> r(width * height), g(width * height), b(width * height);
        for (int i = 0; i < width * height; ++i)
        {
            r[i] = p_data[i * 3];
            g[i] = p_data[i * 3 + 1];
            b[i] = p_data[i * 3 + 2];
        }
        HalconCpp::GenImage3(&image, "byte", width, height,
                             reinterpret_cast<Hlong>(r.data()),
                             reinterpret_cast<Hlong>(g.data()),
                             reinterpret_cast<Hlong>(b.data()));
        return image;
    }

    // 其他格式：尝试 SDK 转 RGB8
    MV_CC_PIXEL_CONVERT_PARAM param{};
    param.nWidth = width;
    param.nHeight = height;
    param.enSrcPixelType = pixel_type;
    param.pSrcData = p_data;
    param.nSrcDataLen = p_frameinfo->nFrameLen;
    param.enDstPixelType = PixelType_Gvsp_RGB8_Packed;

    std::vector<unsigned char> rgb_buf(width * height * 3);
    param.pDstBuffer = rgb_buf.data();
    param.nDstBufferSize = static_cast<unsigned int>(rgb_buf.size());

    int ret = MV_CC_ConvertPixelType(handler_, &param);
    if (ret != MV_OK)
    {
        SPDLOG_ERROR("ConvertPixelType failed: pixelType=0x{:08X}, ret=0x{:08X}",
                     static_cast<unsigned int>(pixel_type), ret);
        return {};
    }

    std::vector<unsigned char> r(width * height), g(width * height), b(width * height);
    for (int i = 0; i < width * height; ++i)
    {
        r[i] = rgb_buf[i * 3];
        g[i] = rgb_buf[i * 3 + 1];
        b[i] = rgb_buf[i * 3 + 2];
    }

    HalconCpp::GenImage3(&image, "byte", width, height,
                         reinterpret_cast<Hlong>(r.data()),
                         reinterpret_cast<Hlong>(g.data()),
                         reinterpret_cast<Hlong>(b.data()));
    return image;
}
