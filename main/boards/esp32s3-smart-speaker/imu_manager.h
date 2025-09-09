#ifndef IMU_MANAGER_H
#define IMU_MANAGER_H

#include "config.h"
#include "mpu6050_sensor.h"
#include <driver/i2c_master.h>
#include <memory>

class ImuManager {
public:
    static ImuManager& GetInstance();
    
    // 初始化IMU系统
    bool Initialize();
    
    // 启动/停止IMU任务
    void StartImuTask();
    void StopImuTask();
    
    // 获取IMU传感器实例
    Mpu6050Sensor* GetImuSensor() const { return mpu6050_sensor_.get(); }
    
    // 检查是否已初始化
    bool IsInitialized() const { return initialized_; }

private:
    ImuManager() = default;
    ~ImuManager() = default;
    ImuManager(const ImuManager&) = delete;
    ImuManager& operator=(const ImuManager&) = delete;
    
    void InitializeImu();
    static void ImuDataTask(void *pvParameters);
    
    bool initialized_ = false;
    i2c_master_bus_handle_t imu_i2c_bus_;
    std::unique_ptr<Mpu6050Sensor> mpu6050_sensor_;
    
    // 任务句柄
    TaskHandle_t imu_task_handle_ = nullptr;
};

#endif // IMU_MANAGER_H
