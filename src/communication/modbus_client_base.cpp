#include "modbus_client_base.h"
#include <spdlog/spdlog.h>
#include <QModbusReply>
ModbusClientBase::ModbusClientBase(const std::string &name, QObject *parent)
    : QObject(parent), id_(name)
{
}

ModbusClientBase::~ModbusClientBase()
{
    disconnectDevice();
    delete client_;
}

void ModbusClientBase::disconnectDevice()
{
    if (client_ && client_->state() != QModbusDevice::UnconnectedState)
    {
        client_->disconnectDevice();
    }
}

bool ModbusClientBase::isConnected() const
{
    return client_ && client_->state() == QModbusDevice::ConnectedState;
}

void ModbusClientBase::asyncRead(const QModbusDataUnit &unit, readRegistersCallback reg_cb, readCoilsCallback coil_cb)
{
    if (!isConnected())
    {
        if (reg_cb)
            reg_cb(false, {});
        if (coil_cb)
            coil_cb(false, {});
        return;
    }

    auto *reply = client_->sendReadRequest(unit, server_address_);
    if (!reply)
    {
        SPDLOG_ERROR("Modbus {} read request failed", id_);
        if (reg_cb)
            reg_cb(false, {});
        if (coil_cb)
            coil_cb(false, {});
        return;
    }

    if (reply->isFinished())
    {
        // 同步返回（broadcast 等场景）
        if (reply->error() == QModbusDevice::NoError)
        {
            auto result = reply->result();
            if (reg_cb)
            {
                std::vector<uint16_t> values(result.valueCount());
                for (uint i = 0; i < result.valueCount(); ++i)
                    values[i] = result.value(i);
                reg_cb(true, values);
            }
            if (coil_cb)
            {
                std::vector<bool> values(result.valueCount());
                for (uint i = 0; i < result.valueCount(); ++i)
                    values[i] = result.value(i) != 0;
                coil_cb(true, values);
            }
        }
        else
        {
            SPDLOG_DEBUG("Modbus {} read error: {}", id_, reply->errorString().toStdString());
            if (reg_cb)
                reg_cb(false, {});
            if (coil_cb)
                coil_cb(false, {});
        }
        reply->deleteLater();
        return;
    }

    // 异步等待应答
    connect(reply, &QModbusReply::finished, this, [this, reply, reg_cb, coil_cb]()
            {
        if (reply->error() == QModbusDevice::NoError)
        {
            auto result = reply->result();
            if (reg_cb)
            {
                std::vector<uint16_t> values(result.valueCount());
                for (uint i = 0; i < result.valueCount(); ++i)
                    values[i] = result.value(i);
                reg_cb(true, values);
            }
            if (coil_cb)
            {
                std::vector<bool> values(result.valueCount());
                for (uint i = 0; i < result.valueCount(); ++i)
                    values[i] = result.value(i) != 0;
                coil_cb(true, values);
            }
        }
        else
        {
            SPDLOG_DEBUG("Modbus {} read error: {}", id_, reply->errorString().toStdString());
            if (reg_cb) reg_cb(false, {});
            if (coil_cb) coil_cb(false, {});
        }
        reply->deleteLater(); });
}

void ModbusClientBase::asyncWrite(const QModbusDataUnit &unit, writeCallback cb)
{
    if (!isConnected())
    {
        if (cb)
            cb(false);
        return;
    }

    auto *reply = client_->sendWriteRequest(unit, server_address_);
    if (!reply)
    {
        SPDLOG_ERROR("Modbus {} write request failed", id_);
        if (cb)
            cb(false);
        return;
    }

    if (reply->isFinished())
    {
        bool ok = reply->error() == QModbusDevice::NoError;
        if (!ok)
            SPDLOG_ERROR("Modbus {} write error: {}", id_, reply->errorString().toStdString());
        if (cb)
            cb(ok);
        reply->deleteLater();
        return;
    }

    connect(reply, &QModbusReply::finished, this, [this, reply, cb]()
            {
        bool ok = reply->error() == QModbusDevice::NoError;
        if (!ok)
            SPDLOG_ERROR("Modbus {} write error: {}", id_, reply->errorString().toStdString());
        if (cb) cb(ok);
        reply->deleteLater(); });
}

void ModbusClientBase::readHoldingRegisters(uint16_t addr, uint16_t count, readRegistersCallback cb)
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, addr, count);
    asyncRead(unit, cb, nullptr);
}

void ModbusClientBase::writeSingleRegister(uint16_t addr, uint16_t value, writeCallback cb)
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, addr, 1);
    unit.setValue(0, value);
    asyncWrite(unit, cb);
}

void ModbusClientBase::writeMultipleRegisters(uint16_t addr, const std::vector<uint16_t> &values, writeCallback cb)
{
    QModbusDataUnit unit(QModbusDataUnit::HoldingRegisters, addr, static_cast<uint16_t>(values.size()));
    for (size_t i = 0; i < values.size(); ++i)
        unit.setValue(static_cast<int>(i), values[i]);
    asyncWrite(unit, cb);
}

void ModbusClientBase::readCoils(uint16_t addr, uint16_t count, readCoilsCallback cb)
{
    QModbusDataUnit unit(QModbusDataUnit::Coils, addr, count);
    asyncRead(unit, nullptr, cb);
}

void ModbusClientBase::writeSingleCoil(uint16_t addr, bool value, writeCallback cb)
{
    QModbusDataUnit unit(QModbusDataUnit::Coils, addr, 1);
    unit.setValue(0, value ? 1 : 0);
    asyncWrite(unit, cb);
}

void ModbusClientBase::sendResult(uint16_t addr, bool pass, writeCallback cb)
{
    writeSingleRegister(addr, pass ? 1 : 0, cb);
}
