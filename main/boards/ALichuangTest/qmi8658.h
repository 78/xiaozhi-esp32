#ifndef ALICHUANGTEST_QMI8658_H
#define ALICHUANGTEST_QMI8658_H

#include "i2c_device.h"
#include <esp_err.h>
#include <cmath>

// QMI8658 I2C地址 (立创开发板使用0x6A)
#define QMI8658_I2C_ADDR        0x6A

// QMI8658寄存器定义
enum qmi8658_reg {
    QMI8658_WHO_AM_I = 0x00,
    QMI8658_REVISION_ID,
    QMI8658_CTRL1,
    QMI8658_CTRL2,
    QMI8658_CTRL3,
    QMI8658_CTRL4,
    QMI8658_CTRL5,
    QMI8658_CTRL6,
    QMI8658_CTRL7,
    QMI8658_CTRL8,
    QMI8658_CTRL9,
    QMI8658_CATL1_L,
    QMI8658_CATL1_H,
    QMI8658_CATL2_L,
    QMI8658_CATL2_H,
    QMI8658_CATL3_L,
    QMI8658_CATL3_H,
    QMI8658_CATL4_L,
    QMI8658_CATL4_H,
    QMI8658_FIFO_WTM_TH,
    QMI8658_FIFO_CTRL,
    QMI8658_FIFO_SMPL_CNT,
    QMI8658_FIFO_STATUS,
    QMI8658_FIFO_DATA,
    QMI8658_I2CM_STATUS = 44,
    QMI8658_STATUSINT,
    QMI8658_STATUS0,
    QMI8658_STATUS1,
    QMI8658_TIMESTAMP_LOW,
    QMI8658_TIMESTAMP_MID,
    QMI8658_TIMESTAMP_HIGH,
    QMI8658_TEMP_L,
    QMI8658_TEMP_H,
    QMI8658_AX_L,
    QMI8658_AX_H,
    QMI8658_AY_L,
    QMI8658_AY_H,
    QMI8658_AZ_L,
    QMI8658_AZ_H,
    QMI8658_GX_L,
    QMI8658_GX_H,
    QMI8658_GY_L,
    QMI8658_GY_H,
    QMI8658_GZ_L,
    QMI8658_GZ_H,
    QMI8658_MX_L,
    QMI8658_MX_H,
    QMI8658_MY_L,
    QMI8658_MY_H,
    QMI8658_MZ_L,
    QMI8658_MZ_H,
    QMI8658_dQW_L = 73,
    QMI8658_dQW_H,
    QMI8658_dQX_L,
    QMI8658_dQX_H,
    QMI8658_dQY_L,
    QMI8658_dQY_H,
    QMI8658_dQZ_L,
    QMI8658_dQZ_H,
    QMI8658_dVX_L,
    QMI8658_dVX_H,
    QMI8658_dVY_L,
    QMI8658_dVY_H,
    QMI8658_dVZ_L,
    QMI8658_dVZ_H,
    QMI8658_AE_REG1,
    QMI8658_AE_REG2,
    QMI8658_RESET = 96
};

// IMU数据结构
struct ImuData {
    // 原始数据
    int16_t acc_x_raw, acc_y_raw, acc_z_raw;
    int16_t gyro_x_raw, gyro_y_raw, gyro_z_raw;
    
    // 物理单位数据
    float accel_x, accel_y, accel_z;  // 单位: g
    float gyro_x, gyro_y, gyro_z;     // 单位: deg/s
    
    // 计算的角度
    float angle_x, angle_y, angle_z;  // 单位: degrees
    
    int64_t timestamp_us;
};

class Qmi8658 : public I2cDevice {
public:
    Qmi8658(i2c_master_bus_handle_t i2c_bus);
    esp_err_t Initialize();
    esp_err_t ReadRawData(ImuData& data);
    esp_err_t ReadDataWithAngles(ImuData& data);
    bool IsPresent();
    
    // 获取计算的角度
    void CalculateAnglesFromAccel(ImuData& data);

private:
    static constexpr float ACCEL_SCALE = 4.0f / 32768.0f;   // ±4g范围
    static constexpr float GYRO_SCALE = 512.0f / 32768.0f;  // ±512 deg/s范围
    static constexpr float RAD_TO_DEG = 57.29578f;          // 180/π
};

#endif // ALICHUANGTEST_QMI8658_H