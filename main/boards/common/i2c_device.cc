#include "i2c_device.h"

#include <cstring>
#include <esp_log.h>

#define TAG "I2cDevice"


I2cDevice::I2cDevice(i2c_master_bus_handle_t i2c_bus, uint8_t addr) {
    i2c_device_config_t i2c_device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400 * 1000,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0,
        },
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &i2c_device_cfg, &i2c_device_));
    assert(i2c_device_ != NULL);
}

void I2cDevice::WriteReg(uint8_t reg, uint8_t value) {
    uint8_t buffer[2] = {reg, value};
    auto err = i2c_master_transmit(i2c_device_, buffer, 2, 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "WriteReg 0x%02x failed: %s", reg, esp_err_to_name(err));
    }
}

uint8_t I2cDevice::ReadReg(uint8_t reg) {
    uint8_t buffer[1];
    auto err = i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, 1, 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ReadReg 0x%02x failed: %s", reg, esp_err_to_name(err));
        return 0;
    }
    return buffer[0];
}

void I2cDevice::ReadRegs(uint8_t reg, uint8_t* buffer, size_t length) {
    auto err = i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, length, 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ReadRegs 0x%02x len %u failed: %s",
            reg, (unsigned int)length, esp_err_to_name(err));
        memset(buffer, 0, length);
    }
}
