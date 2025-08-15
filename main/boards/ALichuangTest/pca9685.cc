#include "pca9685.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>

static const char* TAG = "PCA9685";

Pca9685::Pca9685(i2c_master_bus_handle_t i2c_bus, uint8_t addr)
    : I2cDevice(i2c_bus, addr), i2c_bus_(i2c_bus), pwm_frequency_(1000) {
}

esp_err_t Pca9685::Initialize(uint16_t pwm_frequency) {
    pwm_frequency_ = pwm_frequency;
    esp_err_t ret;
    
    // 确保设备完全唤醒
    WriteReg(PCA9685_MODE1, 0x00);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // 复位设备
    WriteReg(PCA9685_MODE1, PCA9685_RESTART);
    vTaskDelay(pdMS_TO_TICKS(10));  // 等待复位完成
    
    // 配置MODE1寄存器 - 清除睡眠位，启用自动增量
    WriteReg(PCA9685_MODE1, PCA9685_ALLCALL);
    
    // 配置MODE2寄存器 - 推挽输出，非反相
    WriteReg(PCA9685_MODE2, PCA9685_OUTDRV);
    
    vTaskDelay(pdMS_TO_TICKS(10));  // 增加延迟确保配置生效
    
    // 设置PWM频率
    ret = SetFrequency(pwm_frequency);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set PWM frequency");
        return ret;
    }
    
    // 初始化时关闭所有通道
    TurnOffAll();
    
    // 验证配置
    uint8_t mode1_val = ReadReg(PCA9685_MODE1);
    ESP_LOGI(TAG, "PCA9685 initialized successfully, frequency: %d Hz, MODE1: 0x%02X", 
            pwm_frequency_, mode1_val);
    
    return ESP_OK;
}

void Pca9685::SetPwm(uint8_t channel, uint16_t duty_cycle) {
    if (channel > 15) {
        return;
    }
    
    if (duty_cycle > PCA9685_PWM_RESOLUTION - 1) {
        duty_cycle = PCA9685_PWM_RESOLUTION - 1;
    }
    
    SetPwmTiming(channel, 0, duty_cycle);
}

void Pca9685::SetPwmTiming(uint8_t channel, uint16_t on_time, uint16_t off_time) {
    if (channel > 15) {
        ESP_LOGW(TAG, "Invalid channel: %d (max 15)", channel);
        return;
    }
    
    on_time &= 0x0FFF;
    off_time &= 0x0FFF;
    
    uint8_t base_reg = PCA9685_LED0_ON_L + (channel * 4);
    
    // 直接写入PWM寄存器 (使用I2cDevice的WriteReg)
    WriteReg(base_reg, on_time & 0xFF);
    WriteReg(base_reg + 1, (on_time >> 8) & 0xFF);
    WriteReg(base_reg + 2, off_time & 0xFF);
    WriteReg(base_reg + 3, (off_time >> 8) & 0xFF);
}

void Pca9685::TurnOff(uint8_t channel) {
    if (channel > 15) {
        return;
    }
    
    SetPwmTiming(channel, 0, PCA9685_PWM_RESOLUTION);
}

void Pca9685::TurnOn(uint8_t channel) {
    if (channel > 15) {
        return;
    }
    
    SetPwmTiming(channel, PCA9685_PWM_RESOLUTION, 0);
}

void Pca9685::TurnOffAll() {
    try {
        WriteReg(PCA9685_ALL_LED_ON_L, 0);
        WriteReg(PCA9685_ALL_LED_ON_H, 0);
        WriteReg(PCA9685_ALL_LED_OFF_L, 0);
        WriteReg(PCA9685_ALL_LED_OFF_H, 0x10);
    } catch (...) {
        // Silent failure
    }
}

esp_err_t Pca9685::SetFrequency(uint16_t frequency) {
    pwm_frequency_ = frequency;
    uint8_t prescale = CalculatePrescale(frequency);
    
    uint8_t old_mode = ReadReg(PCA9685_MODE1);
    
    // 进入睡眠模式
    WriteReg(PCA9685_MODE1, (old_mode & 0x7F) | PCA9685_SLEEP);
    vTaskDelay(pdMS_TO_TICKS(5)); // 增加延时确保进入睡眠模式
    
    // 设置预分频值
    WriteReg(PCA9685_PRESCALE, prescale);
    
    // 恢复原模式
    WriteReg(PCA9685_MODE1, old_mode);
    vTaskDelay(pdMS_TO_TICKS(10)); // 增加延时让时钟稳定
    
    // 重启振荡器
    WriteReg(PCA9685_MODE1, old_mode | PCA9685_RESTART);
    vTaskDelay(pdMS_TO_TICKS(10)); // 重启后等待振荡器稳定
    
    return ESP_OK;
}

void Pca9685::Sleep() {
    try {
        uint8_t mode = ReadReg(PCA9685_MODE1);
        WriteReg(PCA9685_MODE1, mode | PCA9685_SLEEP);
    } catch (...) {
        // Silent failure
    }
}

void Pca9685::Wakeup() {
    try {
        uint8_t mode = ReadReg(PCA9685_MODE1);
        WriteReg(PCA9685_MODE1, mode & ~PCA9685_SLEEP);
        vTaskDelay(pdMS_TO_TICKS(5));
    } catch (...) {
        // Silent failure
    }
}

uint8_t Pca9685::CalculatePrescale(uint16_t frequency) {
    // PCA9685内部时钟频率约为25MHz
    // 预分频值 = (25MHz / (4096 * frequency)) - 1
    float prescale_float = (25000000.0f / (4096.0f * frequency)) - 1.0f;
    
    // 先限制浮点数范围，再转换为整数
    if (prescale_float < 3.0f) prescale_float = 3.0f;
    if (prescale_float > 255.0f) prescale_float = 255.0f;
    
    uint8_t prescale = (uint8_t)(prescale_float + 0.5f);  // 四舍五入
    
    return prescale;
}


bool Pca9685::IsDevicePresent() {
    // 创建设备句柄
    i2c_master_dev_handle_t dev_handle;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCA9685_DEFAULT_ADDR,
        .scl_speed_hz = 100000,
    };
    
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) {
        return false;
    }
    
    // 发送一个字节的数据来检测设备
    uint8_t test_data = 0x00;
    ret = i2c_master_transmit(dev_handle, &test_data, 1, 100);
    
    // 删除设备句柄
    i2c_master_bus_rm_device(dev_handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "PCA9685 device detected");
        return true;
    } else {
        ESP_LOGE(TAG, "PCA9685 not detected: %s", esp_err_to_name(ret));
        return false;
    }
}
