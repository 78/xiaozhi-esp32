# MPU6050传感器集成说明

## 概述

本项目为ESP32-S3智能音箱开发板集成了MPU6050六轴传感器支持，提供了现代化的C++封装接口。

## 文件结构

- `mpu6050_sensor.h` - MPU6050传感器封装类的头文件
- `mpu6050_sensor.cc` - MPU6050传感器封装类的实现文件
- `mpu6050_test.cc` - MPU6050传感器测试程序（可选）
- `esp32s3_smart_speaker.cc` - 已集成MPU6050支持的板子实现

## 功能特性

### 1. 传感器支持
- **加速度计**: 支持±2g, ±4g, ±8g, ±16g量程
- **陀螺仪**: 支持±250°/s, ±500°/s, ±1000°/s, ±2000°/s量程
- **温度传感器**: 内置温度传感器
- **姿态角计算**: 使用互补滤波算法计算俯仰角、横滚角和偏航角

### 2. 技术特点
- 使用ESP-IDF的现代I2C Master API
- 支持多量程配置
- 内置数字低通滤波器
- 可配置采样率
- 互补滤波姿态解算
- 完整的错误处理机制

## 硬件连接

根据`config.h`中的定义：

```c
// IMU传感器 (I2C接口)
#define IMU_I2C_SDA_PIN         GPIO_NUM_21
#define IMU_I2C_SCL_PIN         GPIO_NUM_20
#define IMU_INT_PIN             GPIO_NUM_19
```

### 连接方式
- **VCC**: 3.3V
- **GND**: 地
- **SDA**: GPIO21
- **SCL**: GPIO20
- **INT**: GPIO19（中断引脚，可选）

## 使用方法

### 1. 基本使用

```cpp
#include "mpu6050_sensor.h"

// 创建传感器实例
auto sensor = std::make_unique<Mpu6050Sensor>(i2c_bus_handle);

// 初始化传感器
if (sensor->Initialize(ACCE_FS_4G, GYRO_FS_500DPS)) {
    // 唤醒传感器
    if (sensor->WakeUp()) {
        // 验证设备ID
        uint8_t device_id;
        if (sensor->GetDeviceId(&device_id)) {
            ESP_LOGI(TAG, "MPU6050 initialized, ID: 0x%02X", device_id);
        }
    }
}
```

### 2. 读取传感器数据

```cpp
mpu6050_acce_value_t acce;
mpu6050_gyro_value_t gyro;
mpu6050_temp_value_t temp;
complimentary_angle_t angle;

// 读取加速度计数据
if (sensor->GetAccelerometer(&acce)) {
    ESP_LOGI(TAG, "Accelerometer - X:%.2f, Y:%.2f, Z:%.2f", 
             acce.acce_x, acce.acce_y, acce.acce_z);
}

// 读取陀螺仪数据
if (sensor->GetGyroscope(&gyro)) {
    ESP_LOGI(TAG, "Gyroscope - X:%.2f, Y:%.2f, Z:%.2f", 
             gyro.gyro_x, gyro.gyro_y, gyro.gyro_z);
}

// 读取温度数据
if (sensor->GetTemperature(&temp)) {
    ESP_LOGI(TAG, "Temperature: %.2f°C", temp.temp);
}

// 计算姿态角
if (sensor->ComplimentaryFilter(&acce, &gyro, &angle)) {
    ESP_LOGI(TAG, "Attitude - Pitch:%.2f°, Roll:%.2f°, Yaw:%.2f°", 
             angle.pitch, angle.roll, angle.yaw);
}
```

### 3. 获取传感器状态

```cpp
// 检查是否已初始化
if (sensor->IsInitialized()) {
    // 获取状态信息
    std::string status = sensor->GetStatusJson();
    ESP_LOGI(TAG, "Sensor status: %s", status.c_str());
}
```

## 配置参数

### 加速度计量程
- `ACCE_FS_2G`: ±2g (16384 LSB/g)
- `ACCE_FS_4G`: ±4g (8192 LSB/g)
- `ACCE_FS_8G`: ±8g (4096 LSB/g)
- `ACCE_FS_16G`: ±16g (2048 LSB/g)

### 陀螺仪量程
- `GYRO_FS_250DPS`: ±250°/s (131 LSB/°/s)
- `GYRO_FS_500DPS`: ±500°/s (65.5 LSB/°/s)
- `GYRO_FS_1000DPS`: ±1000°/s (32.8 LSB/°/s)
- `GYRO_FS_2000DPS`: ±2000°/s (16.4 LSB/°/s)

### 互补滤波参数
- **alpha**: 0.98 (默认值，表示更信任陀螺仪)
- **采样率**: 125Hz
- **数字低通滤波器**: 5Hz

## 集成到板子

MPU6050已经集成到`Esp32s3SmartSpeaker`类中：

1. **自动初始化**: 在板子构造函数中自动初始化MPU6050
2. **后台任务**: 自动创建后台任务持续读取传感器数据
3. **状态报告**: 在`GetBoardJson()`中报告传感器状态

## 日志输出

传感器会输出以下日志信息：

```
I (1234) SmartSpeaker: MPU6050 sensor initialized successfully (ID: 0x68)
I (1235) SmartSpeaker: IMU data task created successfully
I (1236) SmartSpeaker: IMU data task started
I (1237) SmartSpeaker: Accelerometer - X:0.12, Y:-0.05, Z:0.98
I (1238) SmartSpeaker: Gyroscope - X:0.15, Y:-0.02, Z:0.08
I (1239) SmartSpeaker: Temperature: 25.3°C
I (1240) SmartSpeaker: Attitude - Pitch:2.1°, Roll:-1.5°, Yaw:0.3°
```

## 故障排除

### 常见问题

1. **设备ID不匹配**
   - 检查I2C连接
   - 确认设备地址是否正确
   - 检查电源供应

2. **初始化失败**
   - 检查I2C总线配置
   - 确认GPIO引脚配置正确
   - 检查上拉电阻

3. **数据读取失败**
   - 检查I2C通信
   - 确认传感器已唤醒
   - 检查采样率配置

### 调试建议

1. 启用I2C调试日志
2. 检查硬件连接
3. 使用示波器检查I2C信号
4. 验证电源电压稳定性

## 技术细节

### I2C配置
- **时钟频率**: 100kHz
- **地址**: 0x68 (7位地址)
- **上拉电阻**: 内部使能

### 寄存器配置
- **加速度计量程**: 寄存器0x1C
- **陀螺仪量程**: 寄存器0x1B
- **数字低通滤波器**: 寄存器0x1A
- **采样率**: 寄存器0x19
- **电源管理**: 寄存器0x6B

### 数据格式
- **加速度计**: 16位有符号整数，转换为g值
- **陀螺仪**: 16位有符号整数，转换为度/秒
- **温度**: 16位有符号整数，转换为摄氏度

## 扩展功能

### 可扩展的功能
1. **中断支持**: 可以配置数据就绪中断
2. **运动检测**: 可以配置运动检测中断
3. **自由落体检测**: 可以配置自由落体检测
4. **FIFO支持**: 可以使用FIFO缓冲区
5. **DMP支持**: 可以使用数字运动处理器

### 性能优化
1. **降低采样率**: 减少功耗
2. **使用中断**: 避免轮询
3. **FIFO缓冲**: 批量读取数据
4. **休眠模式**: 不使用时进入低功耗模式

## 许可证

本代码遵循项目的许可证要求。
