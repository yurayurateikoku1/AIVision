#pragma once
#include "modbus_client_base.h"
#include <QModbusRtuSerialClient>
#include <spdlog/spdlog.h>
#include <QVariant>

/// @brief Modbus RTU 客户端（串口 → PLC/外设）
///        config.protocol = "modbus_rtu" 时由 CommManager 创建
class ModbusRtuClient : public ModbusClientBase
{
    Q_OBJECT
public:
    explicit ModbusRtuClient(const std::string &name, QObject *parent = nullptr)
        : ModbusClientBase(name, parent)
    {
        client_ = new QModbusRtuSerialClient(this);
        client_->setTimeout(timeout_ms_);
        client_->setNumberOfRetries(3);
    }

    bool connectDevice(const CommunicationParam &param) override
    {
        if (isConnected())
        {
            SPDLOG_WARN("Modbus RTU {} already connected", id_);
            return true;
        }

        server_address_ = param.slave_address;

        client_->setConnectionParameter(
            QModbusDevice::SerialPortNameParameter, QVariant(QString::number(param.serial_port)));
        client_->setConnectionParameter(
            QModbusDevice::SerialBaudRateParameter, QVariant(param.baud_rate));
        client_->setConnectionParameter(
            QModbusDevice::SerialDataBitsParameter, QVariant(param.data_bits));
        client_->setConnectionParameter(
            QModbusDevice::SerialStopBitsParameter, QVariant(param.stop_bits));
        client_->setConnectionParameter(
            QModbusDevice::SerialParityParameter, QVariant(param.parity));

        if (!client_->connectDevice())
        {
            SPDLOG_ERROR("Modbus RTU connect failed: {} {} - {}",
                         id_, param.serial_port, client_->errorString().toStdString());
            return false;
        }

        SPDLOG_INFO("Modbus RTU connecting: {} {} @{}...", id_, param.serial_port, param.baud_rate);
        return true;
    }
};