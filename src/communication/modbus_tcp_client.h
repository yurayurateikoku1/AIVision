#pragma once

#include "modbus_client_base.h"
#include <QModbusTcpClient>
#include <spdlog/spdlog.h>
#include <QVariant>

/// @brief Modbus TCP 客户端（网口 → PLC）
///        config.protocol = "modbus_tcp" 时由 CommManager 创建
class ModbusTcpClient : public ModbusClientBase
{
    Q_OBJECT
public:
    explicit ModbusTcpClient(const std::string &name, QObject *parent = nullptr)
        : ModbusClientBase(name, parent)
    {
        client_ = new QModbusTcpClient(this);
        client_->setTimeout(timeout_ms_);
        client_->setNumberOfRetries(3);
    }

    bool connectDevice(const CommunicationParam &param) override
    {
        if (isConnected())
        {
            SPDLOG_WARN("Modbus TCP {} already connected", id_);
            return true;
        }

        server_address_ = param.slave_address;

        client_->setConnectionParameter(
            QModbusDevice::NetworkPortParameter, QVariant(param.port));
        client_->setConnectionParameter(
            QModbusDevice::NetworkAddressParameter, QVariant(QString::fromStdString(param.ip)));

        if (!client_->connectDevice())
        {
            SPDLOG_ERROR("Modbus TCP connect failed: {} {}:{} - {}",
                         id_, param.ip, param.port, client_->errorString().toStdString());
            return false;
        }

        // SPDLOG_INFO("Modbus TCP connecting: {} {}:{}...", id_, param.ip, param.port);
        return true;
    }
};