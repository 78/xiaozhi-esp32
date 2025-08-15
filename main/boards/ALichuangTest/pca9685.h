#ifndef PCA9685_H
#define PCA9685_H

#include "i2c_device.h"
#include <driver/i2c_master.h>
#include <stdint.h>

// PCA9685 寄存器地址
#define PCA9685_MODE1          0x00
#define PCA9685_MODE2          0x01
#define PCA9685_SUBADR1        0x02
#define PCA9685_SUBADR2        0x03
#define PCA9685_SUBADR3        0x04
#define PCA9685_PRESCALE       0xFE
#define PCA9685_LED0_ON_L      0x06
#define PCA9685_LED0_ON_H      0x07
#define PCA9685_LED0_OFF_L     0x08
#define PCA9685_LED0_OFF_H     0x09
#define PCA9685_ALL_LED_ON_L   0xFA
#define PCA9685_ALL_LED_ON_H   0xFB
#define PCA9685_ALL_LED_OFF_L  0xFC
#define PCA9685_ALL_LED_OFF_H  0xFD

// MODE1 寄存器位
#define PCA9685_RESTART        0x80
#define PCA9685_SLEEP          0x10
#define PCA9685_ALLCALL        0x01

// MODE2 寄存器位
#define PCA9685_INVRT          0x10
#define PCA9685_OUTDRV         0x04

// PCA9685 默认地址
#define PCA9685_DEFAULT_ADDR   0x40

// PWM分辨率
#define PCA9685_PWM_RESOLUTION 4096  // 12位分辨率

/**
 * @brief PCA9685 16路PWM控制器驱动类
 *        支持通过I2C控制多路PWM输出
 */
class Pca9685 : public I2cDevice {
public:
    /**
     * @brief 构造函数
     * @param i2c_bus I2C总线句柄
     * @param addr PCA9685的I2C地址（默认0x80，所有地址引脚接地）
     */
    Pca9685(i2c_master_bus_handle_t i2c_bus, uint8_t addr = PCA9685_DEFAULT_ADDR);

    /**
     * @brief 初始化PCA9685
     * @param pwm_frequency PWM频率（Hz），默认1000Hz
     * @return ESP_OK 如果成功，其他错误码如果失败
     */
    esp_err_t Initialize(uint16_t pwm_frequency = 1000);

    /**
     * @brief 设置指定通道的PWM占空比
     * @param channel PWM通道（0-15）
     * @param duty_cycle 占空比（0-4095，对应0-100%）
     */
    void SetPwm(uint8_t channel, uint16_t duty_cycle);

    /**
     * @brief 设置指定通道的PWM值（更精确控制）
     * @param channel PWM通道（0-15）
     * @param on_time 开启时间点（0-4095）
     * @param off_time 关闭时间点（0-4095）
     */
    void SetPwmTiming(uint8_t channel, uint16_t on_time, uint16_t off_time);

    /**
     * @brief 关闭指定通道的PWM输出
     * @param channel PWM通道（0-15）
     */
    void TurnOff(uint8_t channel);

    /**
     * @brief 开启指定通道的PWM输出（全开）
     * @param channel PWM通道（0-15）
     */
    void TurnOn(uint8_t channel);

    /**
     * @brief 关闭所有PWM输出
     */
    void TurnOffAll();

    /**
     * @brief 设置PWM频率
     * @param frequency 频率（Hz）
     * @return ESP_OK 如果成功，其他错误码如果失败
     */
    esp_err_t SetFrequency(uint16_t frequency);

    /**
     * @brief 进入睡眠模式
     */
    void Sleep();

    /**
     * @brief 从睡眠模式唤醒
     */
    void Wakeup();
    
    /**
     * @brief 检测PCA9685是否存在
     * @return true如果设备存在并响应
     */
    bool IsDevicePresent();

private:
    i2c_master_bus_handle_t i2c_bus_;
    uint16_t pwm_frequency_;

    /**
     * @brief 计算预分频值
     * @param frequency 目标频率
     * @return 预分频值
     */
    uint8_t CalculatePrescale(uint16_t frequency);
    
};

#endif // PCA9685_H