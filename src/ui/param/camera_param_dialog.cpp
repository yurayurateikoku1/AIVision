#include "camera_param_dialog.h"
#include "ui_camera_param_dialog.h"
#include "../../camera/camera_mgr.h"
#include "../../context.h"
#include <spdlog/spdlog.h>

CameraParamDialog::CameraParamDialog(Context &ctx, CameraMgr &cam_mgr, QWidget *parent)
    : QDialog(parent), ui(new Ui::CameraParamDialog), ctx_(ctx), cam_mgr_(cam_mgr)
{
    ui->setupUi(this);

    // 从 Context 加载当前参数到 UI
    auto it = ctx_.camera_params.begin();
    if (it != ctx_.camera_params.end())
    {
        ui->doubleSpinBox_cameraGain->setValue(it->second.gain);
        ui->doubleSpinBox_imageRotaAngle->setValue(it->second.rotation_deg);
    }

    // 获取相机参数 — 从相机 SDK 读取实际值
    connect(ui->pushButton_getCameraParam, &QPushButton::clicked, this, [this]()
            {
        auto it = ctx_.camera_params.begin();
        if (it == ctx_.camera_params.end()) return;

        auto *cam = cam_mgr_.getCamera(it->second.name);
        if (!cam || !cam->isOpened())
        {
            SPDLOG_WARN("Camera not connected, cannot get params");
            return;
        }

        float gain = 0;
        if (cam->getGain(gain))
            ui->doubleSpinBox_cameraGain->setValue(gain);

        SPDLOG_INFO("Camera params read from device"); });

    // 设置相机参数 — 写入相机 SDK + 存回 Context
    connect(ui->pushButton_setCameraParam, &QPushButton::clicked, this, [this]()
            {
        auto it = ctx_.camera_params.begin();
        if (it == ctx_.camera_params.end()) return;

        auto *cam = cam_mgr_.getCamera(it->second.name);

        float gain = static_cast<float>(ui->doubleSpinBox_cameraGain->value());
        int rotation = static_cast<int>(ui->doubleSpinBox_imageRotaAngle->value());

        // 写入相机
        if (cam && cam->isOpened())
        {
            cam->setGain(gain);
            SPDLOG_INFO("Camera params set to device");
        }

        // 存回 Context
        it->second.gain = gain;
        it->second.rotation_deg = rotation;

        SPDLOG_INFO("Camera params saved: gain={}, rotation={}", gain, rotation); });
}

CameraParamDialog::~CameraParamDialog()
{
    delete ui;
}
