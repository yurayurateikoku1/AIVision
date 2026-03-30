#include "camera_mgr.h"
#include "hik_camera.h"
#include "../context.h"
#include <spdlog/spdlog.h>

CameraMgr::CameraMgr(QObject *parent)
    : QObject(nullptr)
{

    // 心跳定时器：周期性读寄存器
    heartbeat_timer_ = new QTimer(this);
    heartbeat_timer_->setInterval(HEARTBEAT_INTERVAL_MS);
    connect(heartbeat_timer_, &QTimer::timeout, this, &CameraMgr::slot_heartbeatTimer);

    // 枚举定时器单次触发，去抖合并多个枚举请求
    discovery_timer_ = new QTimer(this);
    discovery_timer_->setSingleShot(true);
    discovery_timer_->setInterval(DISCOVERY_DEBOUNCE_MS);
    connect(discovery_timer_, &QTimer::timeout, this, &CameraMgr::slot_doDiscovery);

    // 程序启动时：第一次枚举（唯一允许的启动时枚举）

    enumAndOpenAll();

    // 启动心跳
    heartbeat_timer_->start();
}

CameraMgr::~CameraMgr()
{
}

void CameraMgr::enumAndOpenAll()
{
    auto devices = HikCamera::enumDevices();
    if (devices.empty())
    {
        SPDLOG_WARN("No HIK camera devices found");
        return;
    }
    SPDLOG_INFO("Found {} HIK devices", devices.size());

    // 构建枚举设备的 name → (dev_info, ip) 映射
    struct DevEntry
    {
        MV_CC_DEVICE_INFO *info;
        std::string ip;
    };
    std::map<std::string, DevEntry> name_dev_map;
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
            std::string name = gige.chUserDefinedName[0]
                                   ? std::string(reinterpret_cast<const char *>(gige.chUserDefinedName))
                                   : "";
            name_dev_map[name] = {&dev, ip};
            SPDLOG_INFO("Enumerated GigE device: Name={}, IP={}", name, ip);
        }
    }

    // 按配置中的相机参数，根据 name 匹配枚举到的设备
    for (auto &[cam_name, cfg] : Context::getInstance().camera_params)
    {
        auto it = name_dev_map.find(cam_name);
        if (it == name_dev_map.end())
        {
            SPDLOG_WARN("Camera {} not found in enumeration", cam_name);
            continue;
        }

        // 用枚举到的实际 IP 更新配置
        cfg.ip = it->second.ip;

        if (addCamera(cfg, it->second.info))
        {
            auto *cam = getCamera(cam_name);
            cam->startGrabbing();
            SPDLOG_INFO("Camera {} (IP: {}) started", cam_name, cfg.ip);
        }
    }
}

bool CameraMgr::addCamera(const CameraControlParam &camera_param)
{
    std::lock_guard lock(mutex_);
    if (cameras_.count(camera_param.name))
    {
        SPDLOG_WARN("Camera {} already exists", camera_param.name);
        return false;
    }
    auto cam = std::make_unique<HikCamera>();
    if (!cam->openCamera(camera_param))
    {
        return false;
    }
    cameras_[camera_param.name] = std::move(cam);
    return true;
}

bool CameraMgr::addCamera(const CameraControlParam &camera_param, MV_CC_DEVICE_INFO *dev_info)
{
    std::lock_guard lock(mutex_);
    if (cameras_.count(camera_param.name))
    {
        SPDLOG_WARN("Camera {} already exists", camera_param.name);
        return false;
    }
    auto cam = std::make_unique<HikCamera>();
    if (!cam->openCamera(camera_param, dev_info))
    {
        return false;
    }
    cameras_[camera_param.name] = std::move(cam);
    return true;
}

void CameraMgr::removeCamera(const std::string &name)
{
    std::lock_guard lock(mutex_);
    if (auto it = cameras_.find(name); it != cameras_.end())
    {
        it->second->closeCamera();
        cameras_.erase(it);
    }
    offline_cameras_.erase(name);
}

InterfaceCamera *CameraMgr::getCamera(const std::string &name)
{
    std::lock_guard lock(mutex_);
    auto it = cameras_.find(name);
    return it != cameras_.end() ? it->second.get() : nullptr;
}

std::vector<std::string> CameraMgr::getCameraNames() const
{
    std::lock_guard lock(mutex_);
    std::vector<std::string> names;
    names.reserve(cameras_.size());
    for (auto &[name, _] : cameras_)
    {
        names.push_back(name);
    }
    return names;
}

void CameraMgr::openAll()
{
    std::lock_guard lock(mutex_);
    for (auto &[name, cam] : cameras_)
    {
        if (!cam->isOpened())
        {
            SPDLOG_WARN("Camera {} is not opened", name);
        }
    }
}

void CameraMgr::closeAll()
{
    heartbeat_timer_->stop();
    discovery_timer_->stop();
    offline_cameras_.clear();

    std::lock_guard lock(mutex_);
    for (auto &[name, cam] : cameras_)
    {
        cam->closeCamera();
    }
    cameras_.clear();
}

void CameraMgr::requestDiscovery()
{
    // 去抖：多次请求合并为一次枚举
    if (!discovery_timer_->isActive())
    {
        SPDLOG_INFO("Discovery requested, will execute in {}ms", DISCOVERY_DEBOUNCE_MS);
        discovery_timer_->start();
    }
}

void CameraMgr::markOffline(const std::string &camera_name)
{
    {
        std::lock_guard lock(mutex_);
        auto it = cameras_.find(camera_name);
        if (it != cameras_.end())
        {
            auto *hik_cam = dynamic_cast<HikCamera *>(it->second.get());
            if (hik_cam)
                hik_cam->forceCloseCamera();
        }
    }
    offline_cameras_.insert(camera_name);
    emit sign_cameraStatusChanged(camera_name, false);
    SPDLOG_INFO("Camera {} marked offline, scheduling discovery", camera_name);

    // 心跳失败/异常回调 → 触发一次枚举（去抖合并）
    requestDiscovery();
}

void CameraMgr::slot_heartbeatTimer()
{
    std::vector<std::string> failed_cameras;

    {
        std::lock_guard lock(mutex_);
        for (auto &[cam_name, cam] : cameras_)
        {
            // 跳过已知离线的相机
            if (offline_cameras_.count(cam_name))
                continue;

            auto *hik_cam = dynamic_cast<HikCamera *>(cam.get());
            if (!hik_cam || !hik_cam->isOpened())
                continue;

            // 心跳：读只读寄存器，走 Control Channel，不产生广播
            if (!hik_cam->heartbeat())
            {
                SPDLOG_WARN("Camera {} heartbeat failed", cam_name);
                failed_cameras.push_back(cam_name);
            }
        }
    }

    // 心跳失败的相机：标记离线（会自动触发枚举重连）
    for (const auto &name : failed_cameras)
    {
        markOffline(name);
    }
}

void CameraMgr::slot_doDiscovery()
{
    SPDLOG_INFO("Executing device discovery...");
    auto devices = HikCamera::enumDevices();

    // 构建 name → (dev_info*, ip) 映射
    struct DevEntry
    {
        MV_CC_DEVICE_INFO *info;
        std::string ip;
    };
    std::map<std::string, DevEntry> online_devs;
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
            std::string name = gige.chUserDefinedName[0]
                                   ? std::string(reinterpret_cast<const char *>(gige.chUserDefinedName))
                                   : "";
            online_devs[name] = {&dev, ip};
        }
    }

    std::vector<std::pair<std::string, bool>> status_changes;

    // 尝试重连离线相机,name 出现在枚举结果中才尝试
    if (!offline_cameras_.empty())
    {
        auto to_reconnect = offline_cameras_;
        for (const auto &cam_name : to_reconnect)
        {
            std::lock_guard lock(mutex_);
            auto it = cameras_.find(cam_name);
            if (it == cameras_.end())
                continue;

            auto *hik_cam = dynamic_cast<HikCamera *>(it->second.get());
            if (!hik_cam)
                continue;

            if (online_devs.find(hik_cam->getCameraControlParam().name) == online_devs.end())
                continue;

            if (hik_cam->reconnect())
            {
                offline_cameras_.erase(cam_name);
                status_changes.emplace_back(cam_name, true);
            }
            else
            {
                SPDLOG_WARN("Camera {} reconnect failed, will retry on next discovery", cam_name);
            }
        }
    }

    // 尝试打开配置中尚未管理的相机，可能是启动时未连接，现在上线了
    for (auto &[cam_name, cfg] : Context::getInstance().camera_params)
    {
        {
            std::lock_guard lock(mutex_);
            if (cameras_.count(cam_name))
                continue;
        }
        if (offline_cameras_.count(cam_name))
            continue;

        auto it = online_devs.find(cam_name);
        if (it == online_devs.end())
            continue;

        cfg.ip = it->second.ip;

        if (addCamera(cfg, it->second.info))
        {
            auto *cam = getCamera(cam_name);
            if (cam)
            {
                cam->startGrabbing();
                status_changes.emplace_back(cam_name, true);
                SPDLOG_INFO("Camera {} (IP: {}) connected via discovery", cam_name, cfg.ip);
            }
        }
    }

    // 安全发送信号
    for (auto &[cam_name, online] : status_changes)
    {
        emit sign_cameraStatusChanged(cam_name, online);
    }

    // 如果还有离线相机未重连成功，定时重试枚举
    if (!offline_cameras_.empty())
    {
        SPDLOG_INFO("{} camera(s) still offline, scheduling retry discovery in {}ms",
                    offline_cameras_.size(), HEARTBEAT_INTERVAL_MS);
        // 使用心跳间隔作为重试间隔
        QTimer::singleShot(HEARTBEAT_INTERVAL_MS, this, &CameraMgr::requestDiscovery);
    }
}
