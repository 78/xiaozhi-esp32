#include "axp2101.h"

#include <esp_log.h>

static const char *TAG = "AXP2101";

Axp2101::Axp2101(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {

    WriteReg(0x93, 0x1c); // 配置aldo2输出为3.3v

    uint8_t value = ReadReg(0x90);                 // XPOWERS_AXP2101_LDO_ONOFF_CTRL0
    value = value | 0x02; // set bit 1 (ALDO2)
    WriteReg(0x90, value);  // and power channels now enabled

    WriteReg(0x64, 0x03); // CV charger voltage setting to 42V
    value = ReadReg(0x62);
    ESP_LOGI(TAG, "axp2101 read 0x62 get: 0x%X", value);
    
    WriteReg(0x61, 0x05); // set Main battery precharge current to 125mA
    WriteReg(0x62, 0x10); // set Main battery charger current to 900mA ( 0x08-200mA, 0x09-300mA, 0x0A-400mA )
    WriteReg(0x63, 0x15); // set Main battery term charge current to 125mA
    value = ReadReg(0x62);
    ESP_LOGI(TAG, "axp2101 read 0x62 get: 0x%X", value);

    value = ReadReg(0x18);
    ESP_LOGI(TAG, "axp2101 read 0x18 get: 0x%X", value);
    value = value & 0b11100000;
    value = value | 0b00001110;
    WriteReg(0x18, value);
    value = ReadReg(0x18);
    ESP_LOGI(TAG, "axp2101 read 0x18 get: 0x%X", value);

    WriteReg(0x14, 0x00); // set minimum system voltage to 4.1V (default 4.7V), for poor USB cables
    WriteReg(0x15, 0x00); // set input voltage limit to 3.88v, for poor USB cables
    WriteReg(0x16, 0x05); // set input voltage limit to 3.88v, for poor USB cables

    WriteReg(0x24, 0x01); // set Vsys for PWROFF threshold to 3.2V (default - 2.6V and kill battery)
    WriteReg(0x50, 0x14); // set TS pin to EXTERNAL input (not temperature)
}

bool Axp2101::IsCharging() {
    uint8_t value = ReadReg(0x01);
    return (value & 0b01100000) == 0b00100000;
}

bool Axp2101::IsChargingDone() {
    uint8_t value = ReadReg(0x01);
    return (value & 0b00000111) == 0b00000100;
}

int Axp2101::GetBatteryLevel() {
    uint8_t value = ReadReg(0xA4);
    return value;
}

void Axp2101::PowerOff() {
    uint8_t value = ReadReg(0x10);
    value = value | 0x01;
    WriteReg(0x10, value);
}
