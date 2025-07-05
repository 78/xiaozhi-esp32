#include "sy6970.h"
#include "board.h"
#include "display.h"

#include <esp_log.h>

#define TAG "Sy6970"

Sy6970::Sy6970(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
}

int Sy6970::GetChangingStatus() {
    return (ReadReg(0x0B) >> 3) & 0x03;
}

bool Sy6970::IsCharging() {
    return GetChangingStatus() != 0;
}

bool Sy6970::IsPowerGood() {
    return (ReadReg(0x0B) & 0x04) != 0;
}

bool Sy6970::IsChargingDone() {
    return GetChangingStatus() == 3;
}

int Sy6970::GetBatteryVoltage() {
    uint8_t value = ReadReg(0x0E);
    value &= 0x7F;
    if (value == 0) {
        return 0;
    }
    return value * 20 + 2304;
}

int Sy6970::GetChargeTargetVoltage() {
    uint8_t value = ReadReg(0x06);
    value = (value & 0xFC) >> 2;
    if (value > 0x30) {
        return 4608;
    }
    return value * 16 + 3840;
}

int Sy6970::GetBatteryLevel() {
    int level = 0;
    // 电池所能掉电的最低电压
    int battery_minimum_voltage = 3200;
    int battery_voltage = GetBatteryVoltage();
    int charge_voltage_limit = GetChargeTargetVoltage();
    // ESP_LOGI(TAG, "battery_voltage: %d, charge_voltage_limit: %d", battery_voltage, charge_voltage_limit);
    if (battery_voltage > battery_minimum_voltage && charge_voltage_limit > battery_minimum_voltage) {
        level = (((float) battery_voltage - (float) battery_minimum_voltage) / ((float) charge_voltage_limit - (float) battery_minimum_voltage)) * 100.0;
    }
    // 不连接电池时读取的充电状态不稳定且battery_voltage有时会超过charge_voltage_limit
    if (level > 100) {
        level = 100;
    }
    return level;
}

void Sy6970::PowerOff() {
    WriteReg(0x09, 0B01100100);
}
