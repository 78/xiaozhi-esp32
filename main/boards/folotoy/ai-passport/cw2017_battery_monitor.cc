#include "cw2017_battery_monitor.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "Cw2017Battery"

// Register map (see CW2017 datasheet / FoloToy AI Passport BSP).
#define CW_REG_VERSION   0x00  // version byte; answering means the chip is present
#define CW_REG_VCELL_H   0x02  // 14-bit voltage, V(uV) = raw * 312.5
#define CW_REG_SOC_H     0x04  // high byte = integer percent; 0x05 = 1/256 %
#define CW_REG_CONFIG    0x08  // 0xF0 sleep / 0x30 reset / 0x00 normal

Cw2017BatteryMonitor::Cw2017BatteryMonitor(i2c_master_bus_handle_t i2c_bus, uint8_t addr)
    : i2c_bus_(i2c_bus), i2c_device_(nullptr), device_address_(addr) {
    i2c_device_config_t i2c_device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        // CW2017 is a slow 100 kHz part on the Passport shared bus.
        .scl_speed_hz = 100 * 1000,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0,
        },
    };
    if (i2c_master_bus_add_device(i2c_bus, &i2c_device_cfg, &i2c_device_) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register CW2017 device 0x%02X", addr);
        i2c_device_ = nullptr;
    }
}

Cw2017BatteryMonitor::~Cw2017BatteryMonitor() {
    if (i2c_device_) {
        i2c_master_bus_rm_device(i2c_device_);
        i2c_device_ = nullptr;
    }
}

int Cw2017BatteryMonitor::ReadReg16(uint8_t reg, uint16_t* value) {
    if (!i2c_device_) {
        return -1;
    }
    uint8_t buf[2] = {0, 0};
    // transmit_receive: write the register address, then read two bytes back.
    if (i2c_master_transmit_receive(i2c_device_, &reg, 1, buf, sizeof(buf), 100) != ESP_OK) {
        return -1;
    }
    *value = ((uint16_t)buf[0] << 8) | buf[1];
    return 0;
}

bool Cw2017BatteryMonitor::Initialize() {
    if (present_) {
        return true;
    }
    if (!i2c_device_) {
        return false;
    }

    uint8_t version = 0;
    if (i2c_master_transmit_receive(i2c_device_, (const uint8_t[]){CW_REG_VERSION}, 1,
                                    &version, 1, 100) != ESP_OK) {
        ESP_LOGW(TAG, "CW2017 did not answer at 0x%02X - battery disabled", device_address_);
        return false;
    }
    ESP_LOGI(TAG, "CW2017 found (version 0x%02X)", version);

    // Leave the chip in normal operating mode using its built-in Li-Poly
    // profile; do not write a custom cell profile.
    uint8_t wake[2] = {CW_REG_CONFIG, 0x00};
    i2c_master_transmit(i2c_device_, wake, sizeof(wake), 100);

    present_ = true;
    vTaskDelay(pdMS_TO_TICKS(100));  // let the first SOC sample settle
    return true;
}

int Cw2017BatteryMonitor::GetBatteryLevel() {
    uint16_t soc;
    if (!present_ || ReadReg16(CW_REG_SOC_H, &soc) != 0) {
        return -1;
    }
    int percent = soc >> 8;  // high byte is the integer percentage
    if (percent > 100) {
        // Chip not ready yet may report 0xFF.
        return -1;
    }
    return percent;
}

int Cw2017BatteryMonitor::GetBatteryVoltageMv() {
    uint16_t raw;
    if (!present_ || ReadReg16(CW_REG_VCELL_H, &raw) != 0) {
        return -1;
    }
    raw &= 0x3FFF;  // 14-bit voltage
    return (int)(((uint32_t)raw * 3125) / 10000);  // raw * 312.5 uV -> mV
}
