#include "k10_i2c_device.h"

#include <esp_log.h>

#define TAG "K10I2cDevice"


K10I2cDevice::K10I2cDevice(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
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

void K10I2cDevice::WriteValue(uint8_t value) {
    uint8_t buffer[1] = {value};
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_, buffer, 1, 100));
}

void K10I2cDevice::WriteValues(uint8_t* buffer, size_t length) {
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_, buffer, length, 100));
}

uint8_t K10I2cDevice::ReadValue() {
    uint8_t buffer[1];
    ESP_ERROR_CHECK(i2c_master_receive(i2c_device_, buffer, 1, 100));
    return buffer[0];
}

void K10I2cDevice::ReadValues(uint8_t* buffer, size_t length) {
    ESP_ERROR_CHECK(i2c_master_receive(i2c_device_, buffer, length, 100));
}
