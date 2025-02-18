/**
 * @file I2cDevice.h
 * @brief 该头文件定义了 I2cDevice 类，用于封装 I2C 设备的基本操作。
 *
 * 该类提供了向 I2C 设备写入寄存器、从 I2C 设备读取单个寄存器以及读取多个寄存器的功能。
 *
 * @author 施华锋
 * @date 2025-2-18
 */

#ifndef I2C_DEVICE_H
#define I2C_DEVICE_H

// 引入 I2C 主设备驱动头文件
#include <driver/i2c_master.h>

/**
 * @class I2cDevice
 * @brief 该类封装了 I2C 设备的基本操作，提供了读写寄存器的方法。
 */
class I2cDevice
{
public:
    /**
     * @brief 构造函数，初始化 I2C 设备。
     *
     * 该构造函数使用给定的 I2C 总线句柄和设备地址创建一个 I2C 设备实例。
     *
     * @param i2c_bus I2C 总线句柄，用于标识使用的 I2C 总线。
     * @param addr I2C 设备的 7 位地址。
     */
    I2cDevice(i2c_master_bus_handle_t i2c_bus, uint8_t addr);

protected:
    // I2C 设备句柄，用于后续的 I2C 操作
    i2c_master_dev_handle_t i2c_device_;

    /**
     * @brief 向 I2C 设备的指定寄存器写入一个字节的数据。
     *
     * 该方法将指定的值写入到 I2C 设备的指定寄存器中。
     *
     * @param reg 要写入的寄存器地址。
     * @param value 要写入的值。
     */
    void WriteReg(uint8_t reg, uint8_t value);

    /**
     * @brief 从 I2C 设备的指定寄存器读取一个字节的数据。
     *
     * 该方法从 I2C 设备的指定寄存器中读取一个字节的数据并返回。
     *
     * @param reg 要读取的寄存器地址。
     * @return 从指定寄存器读取到的字节数据。
     */
    uint8_t ReadReg(uint8_t reg);

    /**
     * @brief 从 I2C 设备的指定寄存器开始读取多个字节的数据。
     *
     * 该方法从 I2C 设备的指定寄存器开始，连续读取指定长度的数据，并将其存储到指定的缓冲区中。
     *
     * @param reg 起始寄存器地址。
     * @param buffer 用于存储读取数据的缓冲区指针。
     * @param length 要读取的数据长度（字节数）。
     */
    void ReadRegs(uint8_t reg, uint8_t *buffer, size_t length);
};

#endif // I2C_DEVICE_H