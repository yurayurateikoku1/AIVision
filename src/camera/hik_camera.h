#pragma once

#include "camera_interface.h"
#include "MvCameraControl.h"
#include <halconcpp/HalconCpp.h>
#include <atomic>
#include <mutex>

class HikCamera : public InterfaceCamera
{
public:
    HikCamera();
    ~HikCamera() override;

    /// @brief 打开相机，IP枚举匹配
    /// @param cam_param
    /// @return
    bool openCamera(const CameraControlParam &cam_param) override;

    /// @brief 直接用设备信息打开相机（免二次枚举）
    /// @param cam_param 相机配置（id/name 等）
    /// @param dev_info 枚举得到的设备信息
    /// @return
    bool openCamera(const CameraControlParam &cam_param, MV_CC_DEVICE_INFO *dev_info);

    /// @brief 关闭相机
    void closeCamera() override;

    /// @brief 判断相机是否打开
    /// @return
    bool isOpened() const override;

    /// @brief 开始抓图
    /// @return
    bool startGrabbing() override;

    /// @brief 停止抓图
    void stopGrabbing() override;

    /// @brief 判断是否在抓图
    /// @return
    bool isGrabbing() const override;

    /// @brief 抓一张图
    /// @param frame
    /// @param timeout_ms
    /// @return
    bool grabOne(HalconCpp::HObject &frame, int timeout_ms = 3000) override;

    /// @brief 设置曝光
    /// @param us
    /// @return
    bool setExposureTime(float us) override;

    /// @brief 获取曝光
    /// @param us
    /// @return
    bool getExposureTime(float &us) override;

    /// @brief 设置增益
    /// @param gain
    /// @return
    bool setGain(float gain) override;

    /// @brief 获取增益
    /// @param gain
    /// @return
    bool getGain(float &gain) override;

    /// @brief 设置触发模式
    /// @param mode
    /// @return
    bool setTriggerMode(TriggerMode mode) override;

    /// @brief 获取触发模式
    /// @param mode
    /// @return
    bool getTriggerMode(TriggerMode &mode) override;

    /// @brief 设置帧率
    /// @param fps
    /// @return
    bool setFrameRate(float fps) override;

    /// @brief 获取帧率
    /// @param fps
    /// @return
    bool getFrameRate(float &fps) override;

    /// @brief 设置图像格式
    /// @param format
    /// @return
    bool setPixelFormat(const std::string &format) override;

    /// @brief 获取图像格式
    /// @param format
    /// @return
    bool getPixelFormat(std::string &format) override;

    /// @brief 软触发
    /// @return
    bool softTrigger() override;

    /// @brief 获取相机名称
    /// @return
    std::string getName() const override;

    /// @brief 设置回调
    /// @param callback
    void setCallback(InterfaceCameraCallback *callback) override;

    /// @brief 获取相机参数
    /// @return
    const CameraControlParam &getCameraControlParam() const { return camera_control_param_; }

    /// @brief 重连（关闭旧连接 → 重新枚举 → 打开 → 开始采集）
    /// @return
    bool reconnect();

    /// @brief 心跳检测：通过 Control Channel 读取只读寄存器判断相机是否在线
    /// @return true 在线（寄存器读取成功）
    bool heartbeat() const;

    /// @brief 强制关闭相机
    void forceCloseCamera();

    /// @brief 枚举所有海康设备
    /// @return
    static std::vector<MV_CC_DEVICE_INFO> enumDevices();

private:
    /// @brief 一帧图像回调
    /// @param p_data
    /// @param p_frameinfo
    /// @param p_user
    /// @return
    static void __stdcall frameCallback(unsigned char *p_data,
                                        MV_FRAME_OUT_INFO_EX *p_frameinfo,
                                        void *p_user);
    /// @brief 异常回调
    /// @param msg_type
    /// @param p_user
    /// @return
    static void __stdcall exceptionCallback(unsigned int msg_type, void *p_user);

    /// @brief 转换为 Halcon HObject 图像
    HalconCpp::HObject convert2Hobject(unsigned char *p_data, MV_FRAME_OUT_INFO_EX *p_frameinfo);

    void *handler_ = nullptr;                     // 相机句柄
    CameraControlParam camera_control_param_;     // 相机参数
    InterfaceCameraCallback *callback_ = nullptr; // 回调
    std::atomic<bool> flag_is_grabbing_ = false;
    std::atomic<bool> flag_opened_ = false;
    std::mutex handler_mutex_;
};