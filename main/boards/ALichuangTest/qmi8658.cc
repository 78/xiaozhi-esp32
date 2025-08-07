#include "qmi8658.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "QMI8658"

Qmi8658::Qmi8658(i2c_master_bus_handle_t i2c_bus) 
    : I2cDevice(i2c_bus, QMI8658_I2C_ADDR) {
}

bool Qmi8658::IsPresent() {
    try {
        uint8_t who_am_i = ReadReg(QMI8658_WHO_AM_I);
        ESP_LOGI(TAG, "WHO_AM_I: 0x%02X", who_am_i);
        return (who_am_i == 0x05);  // QMI8658的WHO_AM_I值
    } catch (...) {
        return false;
    }
}

esp_err_t Qmi8658::Initialize() {
    if (!IsPresent()) {
        ESP_LOGE(TAG, "QMI8658 not found");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "QMI8658 detected, initializing...");

    // 软复位
    WriteReg(QMI8658_RESET, 0xB0);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // CTRL1 设置地址自动增加
    WriteReg(QMI8658_CTRL1, 0x40);
    
    // CTRL7 使能加速度计和陀螺仪
    WriteReg(QMI8658_CTRL7, 0x03);
    
    // CTRL2 设置ACC ±4g 250Hz
    WriteReg(QMI8658_CTRL2, 0x95);
    
    // CTRL3 设置GYR ±512dps 250Hz
    WriteReg(QMI8658_CTRL3, 0xD5);

    ESP_LOGI(TAG, "QMI8658 initialized successfully");
    return ESP_OK;
}

esp_err_t Qmi8658::ReadRawData(ImuData& data) {
    // 检查数据是否准备好
    uint8_t status = ReadReg(QMI8658_STATUS0);
    if ((status & 0x03) != 0x03) {  // 检查加速度计和陀螺仪数据都准备好
        return ESP_ERR_TIMEOUT;  // 使用ESP_ERR_TIMEOUT代替ESP_ERR_NOT_READY
    }

    // 读取原始数据 (12字节: 6个轴各2字节)
    uint8_t buffer[12];
    ReadRegs(QMI8658_AX_L, buffer, 12);

    // 转换为有符号16位整数 (原始值)
    data.acc_x_raw = (int16_t)(buffer[1] << 8 | buffer[0]);
    data.acc_y_raw = (int16_t)(buffer[3] << 8 | buffer[2]);
    data.acc_z_raw = (int16_t)(buffer[5] << 8 | buffer[4]);
    data.gyro_x_raw = (int16_t)(buffer[7] << 8 | buffer[6]);
    data.gyro_y_raw = (int16_t)(buffer[9] << 8 | buffer[8]);
    data.gyro_z_raw = (int16_t)(buffer[11] << 8 | buffer[10]);

    // 转换为物理单位
    data.accel_x = data.acc_x_raw * ACCEL_SCALE;
    data.accel_y = data.acc_y_raw * ACCEL_SCALE;
    data.accel_z = data.acc_z_raw * ACCEL_SCALE;
    data.gyro_x = data.gyro_x_raw * GYRO_SCALE;
    data.gyro_y = data.gyro_y_raw * GYRO_SCALE;
    data.gyro_z = data.gyro_z_raw * GYRO_SCALE;
    
    data.timestamp_us = esp_timer_get_time();

    return ESP_OK;
}

void Qmi8658::CalculateAnglesFromAccel(ImuData& data) {
    float temp;
    
    // 根据加速度计算倾角值
    // X轴倾角
    temp = data.accel_x / std::sqrt(data.accel_y * data.accel_y + data.accel_z * data.accel_z);
    data.angle_x = std::atan(temp) * RAD_TO_DEG;
    
    // Y轴倾角
    temp = data.accel_y / std::sqrt(data.accel_x * data.accel_x + data.accel_z * data.accel_z);
    data.angle_y = std::atan(temp) * RAD_TO_DEG;
    
    // Z轴倾角 (重力矢量与Z轴的夹角)
    temp = std::sqrt(data.accel_x * data.accel_x + data.accel_y * data.accel_y) / data.accel_z;
    data.angle_z = std::atan(temp) * RAD_TO_DEG;
}

esp_err_t Qmi8658::ReadDataWithAngles(ImuData& data) {
    esp_err_t ret = ReadRawData(data);
    if (ret == ESP_OK) {
        CalculateAnglesFromAccel(data);
    }
    return ret;
}