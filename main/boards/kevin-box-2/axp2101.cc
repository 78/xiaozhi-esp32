#include "axp2101.h"

#include <esp_log.h>

static const char *TAG = "AXP2101";

bool Axp2101::Initialize(i2c_master_bus_handle_t i2c_bus, int i2c_device_address) {
    i2c_device_config_t axp2101_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = (uint16_t)i2c_device_address,
        .scl_speed_hz = 100000,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0,
        },
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &axp2101_cfg, &i2c_device_));

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
    return true;
}

void Axp2101::WriteReg(uint8_t reg, uint8_t value) {
    uint8_t buffer[2];
    buffer[0] = reg;
    buffer[1] = value;
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_, buffer, 2, 100));
}

uint8_t Axp2101::ReadReg(uint8_t reg) {
    uint8_t buffer[1];
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, 1, 100));
    return buffer[0];
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
