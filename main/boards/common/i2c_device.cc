#include "i2c_device.h"

#include <esp_log.h>

#define TAG "I2cDevice"  // 定义日志标签

// I2cDevice类的构造函数
I2cDevice::I2cDevice(i2c_master_bus_handle_t i2c_bus, uint8_t addr) {
    // 配置I2C设备参数
    i2c_device_config_t i2c_device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,  // 设备地址长度为7位
        .device_address = addr,                // 设备地址
        .scl_speed_hz = 100000,                // SCL时钟频率为100kHz
        .scl_wait_us = 0,                      // SCL等待时间（微秒）
        .flags = {
            .disable_ack_check = 0,            // 不禁用ACK检查
        },
    };
    
    // 将设备添加到I2C总线，并获取设备句柄
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &i2c_device_cfg, &i2c_device_));
    assert(i2c_device_ != NULL);  // 确保设备句柄不为空
}

// 向寄存器写入数据的函数
void I2cDevice::WriteReg(uint8_t reg, uint8_t value) {
    uint8_t buffer[2] = {reg, value};  // 构造写入缓冲区，包含寄存器地址和值
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_, buffer, 2, 100));  // 发送数据
}

// 从寄存器读取数据的函数
uint8_t I2cDevice::ReadReg(uint8_t reg) {
    uint8_t buffer[1];  // 定义接收缓冲区
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, 1, 100));  // 发送寄存器地址并接收数据
    return buffer[0];  // 返回读取的值
}

// 从寄存器读取多个字节数据的函数
void I2cDevice::ReadRegs(uint8_t reg, uint8_t* buffer, size_t length) {
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, length, 100));  // 发送寄存器地址并接收多个字节数据
}