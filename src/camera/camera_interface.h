#pragma once

#include <string>
#include <functional>
#include <halconcpp/HalconCpp.h>
#include "../common.h"

struct InterfaceCameraCallback
{
    virtual ~InterfaceCameraCallback() = default;
    virtual void frameReceived(const std::string &camera_name, const HalconCpp::HObject &frame) = 0;
    virtual void errorMsgReceived(const std::string &camera_name, int error_code, const std::string &msg) = 0;
};

class InterfaceCamera
{
public:
    virtual ~InterfaceCamera() = default;

    virtual bool openCamera(const CameraControlParam &cam_param) = 0;

    virtual void closeCamera() = 0;

    virtual bool isOpened() const = 0;

    virtual bool startGrabbing() = 0;

    virtual void stopGrabbing() = 0;

    virtual bool isGrabbing() const = 0;

    virtual bool grabOne(HalconCpp::HObject &frame, int timeout_ms = 3000) = 0;

    virtual bool setExposureTime(float us) = 0;

    virtual bool getExposureTime(float &us) = 0;

    virtual bool setGain(float gain) = 0;

    virtual bool getGain(float &gain) = 0;

    virtual bool setTriggerMode(TriggerMode mode) = 0;

    virtual bool getTriggerMode(TriggerMode &mode) = 0;

    virtual bool setFrameRate(float fps) = 0;

    virtual bool getFrameRate(float &fps) = 0;

    virtual bool setPixelFormat(const std::string &format) = 0;

    virtual bool getPixelFormat(std::string &format) = 0;

    virtual bool softTrigger() = 0;

    virtual std::string getName() const = 0;

    virtual void setCallback(InterfaceCameraCallback *callback) = 0;
};
