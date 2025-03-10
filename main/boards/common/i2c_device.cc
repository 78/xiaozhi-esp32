#include "i2c_device.h"

#include <esp_log.h>

// 定义日志标签，方便后续打印调试信息
#define TAG "I2cDevice"

/**
 * @brief I2cDevice 类的构造函数，用于初始化 I2C 设备。
 *
 * 该构造函数接收 I2C 总线句柄和设备地址作为参数，
 * 配置 I2C 设备的相关参数，并将设备添加到指定的 I2C 总线上。
 *
 * @param i2c_bus I2C 总线句柄，标识要使用的 I2C 总线。
 * @param addr I2C 设备的 7 位地址。
 */
I2cDevice::I2cDevice(i2c_master_bus_handle_t i2c_bus, uint8_t addr)
{
    // 定义 I2C 设备的配置结构体
    i2c_device_config_t i2c_device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400 * 1000,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0, // 不禁用 ACK 检查
        },
    };

    // 将 I2C 设备添加到指定的总线上，并检查操作是否成功
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &i2c_device_cfg, &i2c_device_));

    // 断言 I2C 设备句柄不为空，确保设备添加成功
    assert(i2c_device_ != NULL);
}

/**
 * @brief 向 I2C 设备的指定寄存器写入一个字节的数据。
 *
 * 该方法将指定的寄存器地址和要写入的值封装在一个缓冲区中，
 * 然后通过 I2C 总线将数据发送到设备。
 *
 * @param reg 要写入的寄存器地址。
 * @param value 要写入寄存器的值。
 */
void I2cDevice::WriteReg(uint8_t reg, uint8_t value)
{
    // 定义一个包含寄存器地址和要写入值的缓冲区
    uint8_t buffer[2] = {reg, value};

    // 通过 I2C 总线向设备发送数据，并检查操作是否成功
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_, buffer, 2, 100));
}

/**
 * @brief 从 I2C 设备的指定寄存器读取一个字节的数据。
 *
 * 该方法先向设备发送要读取的寄存器地址，然后接收设备返回的一个字节数据。
 *
 * @param reg 要读取的寄存器地址。
 * @return 从指定寄存器读取到的一个字节数据。
 */
uint8_t I2cDevice::ReadReg(uint8_t reg)
{
    // 定义一个用于存储读取数据的缓冲区
    uint8_t buffer[1];

    // 通过 I2C 总线先发送要读取的寄存器地址，再接收一个字节的数据，并检查操作是否成功
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, 1, 100));

    // 返回读取到的数据
    return buffer[0];
}

/**
 * @brief 从 I2C 设备的指定寄存器开始连续读取多个字节的数据。
 *
 * 该方法先向设备发送起始寄存器地址，然后接收指定长度的数据并存储到缓冲区中。
 *
 * @param reg 起始寄存器地址。
 * @param buffer 用于存储读取数据的缓冲区指针。
 * @param length 要读取的数据长度（字节数）。
 */
void I2cDevice::ReadRegs(uint8_t reg, uint8_t *buffer, size_t length)
{
    // 通过 I2C 总线先发送起始寄存器地址，再接收指定长度的数据，并检查操作是否成功
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, length, 100));
}