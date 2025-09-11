#ifndef MPU6050_SENSOR_H
#define MPU6050_SENSOR_H

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>

// MPU6050相关定义
#define MPU6050_I2C_ADDRESS 0x68
#define MPU6050_WHO_AM_I_REG 0x75
#define MPU6050_WHO_AM_I_VAL 0x68

// 加速度计量程
typedef enum {
    ACCE_FS_2G = 0,   // ±2g
    ACCE_FS_4G = 1,   // ±4g
    ACCE_FS_8G = 2,   // ±8g
    ACCE_FS_16G = 3   // ±16g
} mpu6050_acce_fs_t;

// 陀螺仪量程
typedef enum {
    GYRO_FS_250DPS = 0,  // ±250°/s
    GYRO_FS_500DPS = 1,  // ±500°/s
    GYRO_FS_1000DPS = 2, // ±1000°/s
    GYRO_FS_2000DPS = 3  // ±2000°/s
} mpu6050_gyro_fs_t;

// 传感器数据结构
typedef struct {
    float acce_x;
    float acce_y;
    float acce_z;
} mpu6050_acce_value_t;

typedef struct {
    float gyro_x;
    float gyro_y;
    float gyro_z;
} mpu6050_gyro_value_t;

typedef struct {
    float temp;
} mpu6050_temp_value_t;

typedef struct {
    float pitch;
    float roll;
    float yaw;
} complimentary_angle_t;

/**
 * @brief MPU6050传感器封装类
 * 
 * 提供现代化的C++接口来操作MPU6050六轴传感器
 * 支持加速度计、陀螺仪、温度传感器和互补滤波
 */
class Mpu6050Sensor {
public:
    /**
     * @brief 构造函数
     * @param i2c_bus I2C总线句柄
     * @param device_addr 设备地址，默认为0x68
     */
    explicit Mpu6050Sensor(i2c_master_bus_handle_t i2c_bus, uint8_t device_addr = MPU6050_I2C_ADDRESS);
    
    /**
     * @brief 析构函数
     */
    ~Mpu6050Sensor();

    /**
     * @brief 初始化传感器
     * @param acce_fs 加速度计量程
     * @param gyro_fs 陀螺仪量程
     * @return true表示初始化成功，false表示失败
     */
    bool Initialize(mpu6050_acce_fs_t acce_fs = ACCE_FS_4G, mpu6050_gyro_fs_t gyro_fs = GYRO_FS_500DPS);

    /**
     * @brief 唤醒传感器
     * @return true表示成功，false表示失败
     */
    bool WakeUp();

    /**
     * @brief 获取设备ID
     * @param device_id 输出设备ID
     * @return true表示成功，false表示失败
     */
    bool GetDeviceId(uint8_t* device_id);

    /**
     * @brief 获取加速度计数据
     * @param acce 输出加速度计数据
     * @return true表示成功，false表示失败
     */
    bool GetAccelerometer(mpu6050_acce_value_t* acce);

    /**
     * @brief 获取陀螺仪数据
     * @param gyro 输出陀螺仪数据
     * @return true表示成功，false表示失败
     */
    bool GetGyroscope(mpu6050_gyro_value_t* gyro);

    /**
     * @brief 获取温度数据
     * @param temp 输出温度数据
     * @return true表示成功，false表示失败
     */
    bool GetTemperature(mpu6050_temp_value_t* temp);

    /**
     * @brief 互补滤波计算姿态角
     * @param acce 加速度计数据
     * @param gyro 陀螺仪数据
     * @param angle 输出姿态角
     * @return true表示成功，false表示失败
     */
    bool ComplimentaryFilter(const mpu6050_acce_value_t* acce, 
                           const mpu6050_gyro_value_t* gyro, 
                           complimentary_angle_t* angle);

    /**
     * @brief 检查传感器是否已初始化
     * @return true表示已初始化，false表示未初始化
     */
    bool IsInitialized() const { return initialized_; }

    /**
     * @brief 获取传感器状态信息
     * @return JSON格式的状态信息
     */
    std::string GetStatusJson() const;

private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t device_handle_;
    uint8_t device_addr_;
    bool initialized_;
    mpu6050_acce_fs_t acce_fs_;
    mpu6050_gyro_fs_t gyro_fs_;
    
    // 互补滤波相关
    float dt_;
    float alpha_;
    complimentary_angle_t last_angle_;
    uint64_t last_time_;
    
    static const char* TAG;

    /**
     * @brief 写入寄存器
     * @param reg_addr 寄存器地址
     * @param data 数据
     * @return true表示成功，false表示失败
     */
    bool WriteRegister(uint8_t reg_addr, uint8_t data);

    /**
     * @brief 读取寄存器
     * @param reg_addr 寄存器地址
     * @param data 输出数据
     * @param len 数据长度
     * @return true表示成功，false表示失败
     */
    bool ReadRegister(uint8_t reg_addr, uint8_t* data, size_t len);

    /**
     * @brief 获取当前时间戳（微秒）
     * @return 时间戳
     */
    uint64_t GetCurrentTimeUs();

    /**
     * @brief 初始化互补滤波
     */
    void InitializeComplimentaryFilter();
};

#endif // MPU6050_SENSOR_H
