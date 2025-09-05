#include "mpu6050_sensor.h"
#include <esp_timer.h>
#include <cmath>

const char* Mpu6050Sensor::TAG = "MPU6050";

Mpu6050Sensor::Mpu6050Sensor(i2c_master_bus_handle_t i2c_bus, uint8_t device_addr)
    : i2c_bus_(i2c_bus), device_addr_(device_addr), initialized_(false),
      acce_fs_(ACCE_FS_4G), gyro_fs_(GYRO_FS_500DPS), dt_(0.0f), alpha_(0.98f) {
    
    // 初始化设备句柄
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = device_addr_,
        .scl_speed_hz = 100000,
    };
    
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &device_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add MPU6050 device to I2C bus: %s", esp_err_to_name(ret));
        device_handle_ = nullptr;
    }
    
    // 初始化互补滤波
    InitializeComplimentaryFilter();
}

Mpu6050Sensor::~Mpu6050Sensor() {
    if (device_handle_) {
        i2c_master_bus_rm_device(i2c_bus_, device_handle_);
    }
}

bool Mpu6050Sensor::Initialize(mpu6050_acce_fs_t acce_fs, mpu6050_gyro_fs_t gyro_fs) {
    if (!device_handle_) {
        ESP_LOGE(TAG, "Device handle is null");
        return false;
    }
    
    acce_fs_ = acce_fs;
    gyro_fs_ = gyro_fs;
    
    // 配置加速度计量程
    uint8_t acce_config = (acce_fs << 3) & 0x18;
    if (!WriteRegister(0x1C, acce_config)) {
        ESP_LOGE(TAG, "Failed to configure accelerometer");
        return false;
    }
    
    // 配置陀螺仪量程
    uint8_t gyro_config = (gyro_fs << 3) & 0x18;
    if (!WriteRegister(0x1B, gyro_config)) {
        ESP_LOGE(TAG, "Failed to configure gyroscope");
        return false;
    }
    
    // 配置数字低通滤波器
    if (!WriteRegister(0x1A, 0x06)) { // DLPF_CFG = 6 (5Hz)
        ESP_LOGE(TAG, "Failed to configure DLPF");
        return false;
    }
    
    // 配置采样率 (1kHz / (1 + 7) = 125Hz)
    if (!WriteRegister(0x19, 0x07)) {
        ESP_LOGE(TAG, "Failed to configure sample rate");
        return false;
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "MPU6050 initialized successfully");
    ESP_LOGI(TAG, "Accelerometer range: %d, Gyroscope range: %d", acce_fs, gyro_fs);
    
    return true;
}

bool Mpu6050Sensor::WakeUp() {
    if (!device_handle_) {
        ESP_LOGE(TAG, "Device handle is null");
        return false;
    }
    
    // 清除睡眠模式位
    if (!WriteRegister(0x6B, 0x00)) {
        ESP_LOGE(TAG, "Failed to wake up MPU6050");
        return false;
    }
    
    // 等待传感器稳定
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "MPU6050 woken up");
    return true;
}

bool Mpu6050Sensor::GetDeviceId(uint8_t* device_id) {
    if (!device_id) {
        ESP_LOGE(TAG, "Device ID pointer is null");
        return false;
    }
    
    return ReadRegister(MPU6050_WHO_AM_I_REG, device_id, 1);
}

bool Mpu6050Sensor::GetAccelerometer(mpu6050_acce_value_t* acce) {
    if (!acce) {
        ESP_LOGE(TAG, "Accelerometer data pointer is null");
        return false;
    }
    
    uint8_t data[6];
    if (!ReadRegister(0x3B, data, 6)) {
        ESP_LOGE(TAG, "Failed to read accelerometer data");
        return false;
    }
    
    // 组合16位数据
    int16_t raw_x = (data[0] << 8) | data[1];
    int16_t raw_y = (data[2] << 8) | data[3];
    int16_t raw_z = (data[4] << 8) | data[5];
    
    // 根据量程转换为g值
    float scale_factor;
    switch (acce_fs_) {
        case ACCE_FS_2G:  scale_factor = 16384.0f; break;
        case ACCE_FS_4G:  scale_factor = 8192.0f; break;
        case ACCE_FS_8G:  scale_factor = 4096.0f; break;
        case ACCE_FS_16G: scale_factor = 2048.0f; break;
        default: scale_factor = 8192.0f; break;
    }
    
    acce->acce_x = raw_x / scale_factor;
    acce->acce_y = raw_y / scale_factor;
    acce->acce_z = raw_z / scale_factor;
    
    return true;
}

bool Mpu6050Sensor::GetGyroscope(mpu6050_gyro_value_t* gyro) {
    if (!gyro) {
        ESP_LOGE(TAG, "Gyroscope data pointer is null");
        return false;
    }
    
    uint8_t data[6];
    if (!ReadRegister(0x43, data, 6)) {
        ESP_LOGE(TAG, "Failed to read gyroscope data");
        return false;
    }
    
    // 组合16位数据
    int16_t raw_x = (data[0] << 8) | data[1];
    int16_t raw_y = (data[2] << 8) | data[3];
    int16_t raw_z = (data[4] << 8) | data[5];
    
    // 根据量程转换为度/秒
    float scale_factor;
    switch (gyro_fs_) {
        case GYRO_FS_250DPS:  scale_factor = 131.0f; break;
        case GYRO_FS_500DPS:  scale_factor = 65.5f; break;
        case GYRO_FS_1000DPS: scale_factor = 32.8f; break;
        case GYRO_FS_2000DPS: scale_factor = 16.4f; break;
        default: scale_factor = 65.5f; break;
    }
    
    gyro->gyro_x = raw_x / scale_factor;
    gyro->gyro_y = raw_y / scale_factor;
    gyro->gyro_z = raw_z / scale_factor;
    
    return true;
}

bool Mpu6050Sensor::GetTemperature(mpu6050_temp_value_t* temp) {
    if (!temp) {
        ESP_LOGE(TAG, "Temperature data pointer is null");
        return false;
    }
    
    uint8_t data[2];
    if (!ReadRegister(0x41, data, 2)) {
        ESP_LOGE(TAG, "Failed to read temperature data");
        return false;
    }
    
    // 组合16位数据
    int16_t raw_temp = (data[0] << 8) | data[1];
    
    // 转换为摄氏度: T = (TEMP_OUT / 340) + 36.53
    temp->temp = raw_temp / 340.0f + 36.53f;
    
    return true;
}

bool Mpu6050Sensor::ComplimentaryFilter(const mpu6050_acce_value_t* acce, 
                                       const mpu6050_gyro_value_t* gyro, 
                                       complimentary_angle_t* angle) {
    if (!acce || !gyro || !angle) {
        ESP_LOGE(TAG, "Input pointers are null");
        return false;
    }
    
    uint64_t current_time = GetCurrentTimeUs();
    
    // 计算时间间隔
    if (last_time_ > 0) {
        dt_ = (current_time - last_time_) / 1000000.0f; // 转换为秒
    } else {
        dt_ = 0.01f; // 默认10ms
    }
    
    // 从加速度计计算俯仰角和横滚角
    float accel_pitch = atan2f(acce->acce_y, sqrtf(acce->acce_x * acce->acce_x + acce->acce_z * acce->acce_z)) * 180.0f / M_PI;
    float accel_roll = atan2f(-acce->acce_x, acce->acce_z) * 180.0f / M_PI;
    
    // 互补滤波
    angle->pitch = alpha_ * (last_angle_.pitch + gyro->gyro_x * dt_) + (1.0f - alpha_) * accel_pitch;
    angle->roll = alpha_ * (last_angle_.roll + gyro->gyro_y * dt_) + (1.0f - alpha_) * accel_roll;
    angle->yaw = last_angle_.yaw + gyro->gyro_z * dt_; // 偏航角只能通过陀螺仪积分
    
    // 更新上次的角度和时间
    last_angle_ = *angle;
    last_time_ = current_time;
    
    return true;
}

std::string Mpu6050Sensor::GetStatusJson() const {
    std::string json = "{";
    json += "\"initialized\":" + std::string(initialized_ ? "true" : "false") + ",";
    json += "\"device_address\":" + std::to_string(device_addr_) + ",";
    json += "\"accelerometer_range\":" + std::to_string(static_cast<int>(acce_fs_)) + ",";
    json += "\"gyroscope_range\":" + std::to_string(static_cast<int>(gyro_fs_)) + ",";
    json += "\"filter_alpha\":" + std::to_string(alpha_) + ",";
    json += "\"sample_rate\":125";
    json += "}";
    return json;
}

bool Mpu6050Sensor::WriteRegister(uint8_t reg_addr, uint8_t data) {
    if (!device_handle_) {
        return false;
    }
    
    uint8_t write_buf[2] = {reg_addr, data};
    esp_err_t ret = i2c_master_transmit(device_handle_, write_buf, 2, 1000);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write register 0x%02X: %s", reg_addr, esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

bool Mpu6050Sensor::ReadRegister(uint8_t reg_addr, uint8_t* data, size_t len) {
    if (!device_handle_ || !data) {
        return false;
    }
    
    esp_err_t ret = i2c_master_transmit_receive(device_handle_, &reg_addr, 1, data, len, 1000);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read register 0x%02X: %s", reg_addr, esp_err_to_name(ret));
        return false;
    }
    
    return true;
}

uint64_t Mpu6050Sensor::GetCurrentTimeUs() {
    return esp_timer_get_time();
}

void Mpu6050Sensor::InitializeComplimentaryFilter() {
    last_angle_.pitch = 0.0f;
    last_angle_.roll = 0.0f;
    last_angle_.yaw = 0.0f;
    last_time_ = 0;
    dt_ = 0.01f;
    alpha_ = 0.98f; // 互补滤波系数，0.98表示更信任陀螺仪
}
