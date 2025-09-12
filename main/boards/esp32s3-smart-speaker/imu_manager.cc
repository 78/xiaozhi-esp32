#include "imu_manager.h"
#include <esp_log.h>

#define TAG "ImuManager"

ImuManager& ImuManager::GetInstance() {
    static ImuManager instance;
    return instance;
}

bool ImuManager::Initialize() {
    if (initialized_) {
        ESP_LOGW(TAG, "ImuManager already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing ImuManager...");
    
    InitializeImu();
    
    // 启动IMU任务
    StartImuTask();
    
    initialized_ = true;
    ESP_LOGI(TAG, "ImuManager initialized successfully");
    return true;
}

void ImuManager::InitializeImu() {
    ESP_LOGI(TAG, "Initializing MPU6050 IMU sensor...");

    // IMU传感器I2C总线
    i2c_master_bus_config_t imu_i2c_cfg = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = IMU_I2C_SDA_PIN,
        .scl_io_num = IMU_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
            .allow_pd = false,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&imu_i2c_cfg, &imu_i2c_bus_));

    // 初始化MPU6050传感器
    mpu6050_sensor_ = std::make_unique<Mpu6050Sensor>(imu_i2c_bus_);

    if (mpu6050_sensor_) {
        uint8_t device_id;
        if (mpu6050_sensor_->GetDeviceId(&device_id)) {
            ESP_LOGI(TAG, "MPU6050 device ID: 0x%02X", device_id);
            if (device_id == MPU6050_WHO_AM_I_VAL) {
                if (mpu6050_sensor_->Initialize(ACCE_FS_4G, GYRO_FS_500DPS)) {
                    if (mpu6050_sensor_->WakeUp()) {
                        initialized_ = true;
                        ESP_LOGI(TAG, "MPU6050 sensor initialized successfully");
                    } else {
                        ESP_LOGE(TAG, "Failed to wake up MPU6050");
                    }
                } else {
                    ESP_LOGE(TAG, "Failed to initialize MPU6050");
                }
            } else {
                ESP_LOGE(TAG, "MPU6050 device ID mismatch: expected 0x%02X, got 0x%02X",
                         MPU6050_WHO_AM_I_VAL, device_id);
            }
        } else {
            ESP_LOGE(TAG, "Failed to read MPU6050 device ID");
        }
    } else {
        ESP_LOGE(TAG, "Failed to create MPU6050 sensor instance");
    }

    if (!initialized_) {
        ESP_LOGW(TAG, "IMU sensor initialization failed - continuing without IMU");
    }
}

void ImuManager::StartImuTask() {
    if (!initialized_) {
        ESP_LOGW(TAG, "ImuManager not initialized, skipping IMU task creation");
        return;
    }
    
    BaseType_t ret = xTaskCreate(ImuDataTask, "imu_data_task", 4096, this, 5, &imu_task_handle_);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create IMU data task");
    } else {
        ESP_LOGI(TAG, "IMU data task created successfully");
    }
}

void ImuManager::StopImuTask() {
    if (imu_task_handle_) {
        vTaskDelete(imu_task_handle_);
        imu_task_handle_ = nullptr;
    }
}

void ImuManager::ImuDataTask(void *pvParameters) {
    ImuManager *manager = static_cast<ImuManager *>(pvParameters);
    ESP_LOGI(TAG, "IMU data task started");

    mpu6050_acce_value_t acce;
    mpu6050_gyro_value_t gyro;
    mpu6050_temp_value_t temp;
    complimentary_angle_t angle;

    while (true) {
        if (manager->mpu6050_sensor_ && manager->initialized_) {
            // 读取加速度计数据
            if (manager->mpu6050_sensor_->GetAccelerometer(&acce)) {
                ESP_LOGI(TAG, "Accelerometer - X:%.2f, Y:%.2f, Z:%.2f", acce.acce_x,
                         acce.acce_y, acce.acce_z);
            }

            // 读取陀螺仪数据
            if (manager->mpu6050_sensor_->GetGyroscope(&gyro)) {
                ESP_LOGI(TAG, "Gyroscope - X:%.2f, Y:%.2f, Z:%.2f", gyro.gyro_x,
                         gyro.gyro_y, gyro.gyro_z);
            }

            // 读取温度数据
            if (manager->mpu6050_sensor_->GetTemperature(&temp)) {
                ESP_LOGI(TAG, "Temperature: %.2f°C", temp.temp);
            }

            // 计算姿态角
            if (manager->mpu6050_sensor_->ComplimentaryFilter(&acce, &gyro, &angle)) {
                ESP_LOGI(TAG, "Attitude - Pitch:%.2f°, Roll:%.2f°, Yaw:%.2f°",
                         angle.pitch, angle.roll, angle.yaw);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
