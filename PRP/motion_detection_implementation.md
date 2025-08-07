# XiaoZhi ESP32 运动检测功能实现步骤

本文档提供了在立创开发板上实现基本运动检测功能的详细步骤，包括读取QMI8658 IMU数据、检测简单运动事件、播放音频反馈和上报事件到云端。

## 核心代码分析

### 1. Application架构
- **主应用程序**：`main/application.cc` - 单例模式，管理整个系统生命周期
- **事件循环**：基于FreeRTOS的单线程事件循环，使用`Schedule()`进行异步任务调度
- **设备状态**：通过`DeviceStateEventManager`管理状态转换和事件通知

### 2. 硬件配置
- **I2C配置**：SDA=GPIO_1, SCL=GPIO_2, 速度400kHz
- **LED控制**：GPIO_48，使用LEDC PWM控制亮度和闪烁
- **I2C总线**：已初始化，可通过`i2c_bus_`句柄添加新设备

### 3. 音频系统
- **音频播放**：`audio_service_.PlaySound()` - 播放预定义的P3格式音频
- **可用音频**：P3_SUCCESS, P3_VIBRATION, P3_EXCLAMATION等

### 4. 云端通信
- **协议接口**：通过`protocol_->SendMcpMessage()`发送JSON格式消息
- **消息格式**：`{"type": "event", "event_id": "xxx", "timestamp": xxx}`

## 实现步骤

### 步骤1：创建QMI8658 IMU驱动

#### 1.1 创建驱动头文件
```cpp
// main/boards/common/qmi8658.h
#ifndef QMI8658_H
#define QMI8658_H

#include "i2c_device.h"
#include <esp_err.h>

// QMI8658寄存器定义
#define QMI8658_I2C_ADDR        0x6B
#define QMI8658_WHO_AM_I        0x00
#define QMI8658_REVISION_ID     0x01
#define QMI8658_CTRL1           0x02
#define QMI8658_CTRL2           0x03
#define QMI8658_CTRL3           0x04
#define QMI8658_CTRL7           0x08
#define QMI8658_CTRL9           0x0A
#define QMI8658_STATUS0         0x2E
#define QMI8658_STATUS1         0x2F
#define QMI8658_TIMESTAMP_L     0x30
#define QMI8658_TIMESTAMP_M     0x31
#define QMI8658_TIMESTAMP_H     0x32
#define QMI8658_TEMP_L          0x33
#define QMI8658_TEMP_H          0x34
#define QMI8658_AX_L            0x35
#define QMI8658_AX_H            0x36
#define QMI8658_AY_L            0x37
#define QMI8658_AY_H            0x38
#define QMI8658_AZ_L            0x39
#define QMI8658_AZ_H            0x3A
#define QMI8658_GX_L            0x3B
#define QMI8658_GX_H            0x3C
#define QMI8658_GY_L            0x3D
#define QMI8658_GY_H            0x3E
#define QMI8658_GZ_L            0x3F
#define QMI8658_GZ_H            0x40

// 控制值
#define QMI8658_CTRL1_ACC_ENABLE    0x01
#define QMI8658_CTRL1_GYR_ENABLE    0x02
#define QMI8658_CTRL2_ACC_FS_8G     0x03
#define QMI8658_CTRL3_GYR_FS_2000   0x03
#define QMI8658_CTRL7_ACC_ENABLE    0x01
#define QMI8658_CTRL7_GYR_ENABLE    0x02

struct ImuData {
    float accel_x, accel_y, accel_z;  // 单位: g
    float gyro_x, gyro_y, gyro_z;     // 单位: deg/s
    int64_t timestamp_us;
};

class Qmi8658 : public I2cDevice {
public:
    Qmi8658(i2c_master_bus_handle_t i2c_bus);
    esp_err_t Initialize();
    esp_err_t ReadData(ImuData& data);
    bool IsPresent();

private:
    static constexpr float ACCEL_SCALE = 8.0f / 32768.0f;  // ±8g范围
    static constexpr float GYRO_SCALE = 2000.0f / 32768.0f;  // ±2000 deg/s范围
};

#endif // QMI8658_H
```

#### 1.2 创建驱动实现文件
```cpp
// main/boards/common/qmi8658.cc
#include "qmi8658.h"
#include <esp_log.h>
#include <esp_timer.h>
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

    // 软复位
    WriteReg(QMI8658_CTRL9, 0x01);
    vTaskDelay(pdMS_TO_TICKS(50));

    // 设置加速度计：±8g，使能
    WriteReg(QMI8658_CTRL2, QMI8658_CTRL2_ACC_FS_8G);
    
    // 设置陀螺仪：±2000 deg/s
    WriteReg(QMI8658_CTRL3, QMI8658_CTRL3_GYR_FS_2000);
    
    // 使能加速度计和陀螺仪
    WriteReg(QMI8658_CTRL7, QMI8658_CTRL7_ACC_ENABLE | QMI8658_CTRL7_GYR_ENABLE);
    
    // 设置ODR（输出数据率）为100Hz
    WriteReg(QMI8658_CTRL1, 0x42);  // 100Hz for both

    ESP_LOGI(TAG, "QMI8658 initialized successfully");
    return ESP_OK;
}

esp_err_t Qmi8658::ReadData(ImuData& data) {
    // 检查数据是否准备好
    uint8_t status = ReadReg(QMI8658_STATUS0);
    if ((status & 0x03) != 0x03) {  // 检查加速度计和陀螺仪数据都准备好
        return ESP_ERR_NOT_READY;
    }

    // 读取原始数据
    uint8_t buffer[12];
    ReadRegs(QMI8658_AX_L, buffer, 12);

    // 转换为有符号16位整数
    int16_t ax = (int16_t)(buffer[1] << 8 | buffer[0]);
    int16_t ay = (int16_t)(buffer[3] << 8 | buffer[2]);
    int16_t az = (int16_t)(buffer[5] << 8 | buffer[4]);
    int16_t gx = (int16_t)(buffer[7] << 8 | buffer[6]);
    int16_t gy = (int16_t)(buffer[9] << 8 | buffer[8]);
    int16_t gz = (int16_t)(buffer[11] << 8 | buffer[10]);

    // 转换为物理单位
    data.accel_x = ax * ACCEL_SCALE;
    data.accel_y = ay * ACCEL_SCALE;
    data.accel_z = az * ACCEL_SCALE;
    data.gyro_x = gx * GYRO_SCALE;
    data.gyro_y = gy * GYRO_SCALE;
    data.gyro_z = gz * GYRO_SCALE;
    data.timestamp_us = esp_timer_get_time();

    return ESP_OK;
}
```

### 步骤2：创建运动检测器

#### 2.1 创建运动检测器头文件
```cpp
// main/motion/motion_detector.h
#ifndef MOTION_DETECTOR_H
#define MOTION_DETECTOR_H

#include "qmi8658.h"
#include <functional>
#include <memory>

enum class MotionEvent {
    NONE,
    PICKUP,         // 设备被拿起
    PUTDOWN_HARD,   // 设备被用力放下
    SHAKE,          // 设备被摇晃
    FLIP            // 设备被翻转
};

class MotionDetector {
public:
    using EventCallback = std::function<void(MotionEvent)>;

    MotionDetector(Qmi8658* imu);
    void SetEventCallback(EventCallback callback);
    void Process();  // 在主循环中调用

private:
    Qmi8658* imu_;
    EventCallback callback_;
    
    // 运动检测状态
    ImuData last_data_;
    bool is_stationary_ = true;
    int64_t last_event_time_us_ = 0;
    
    // 运动检测阈值
    static constexpr float PICKUP_THRESHOLD_G = 1.5f;
    static constexpr float PUTDOWN_THRESHOLD_G = 2.0f;
    static constexpr float SHAKE_THRESHOLD_G = 2.5f;
    static constexpr float FLIP_THRESHOLD_DEG_S = 200.0f;
    static constexpr int64_t DEBOUNCE_TIME_US = 500000;  // 500ms去抖

    bool DetectPickup(const ImuData& data);
    bool DetectPutdownHard(const ImuData& data);
    bool DetectShake(const ImuData& data);
    bool DetectFlip(const ImuData& data);
    float CalculateAccelMagnitude(const ImuData& data);
};

#endif // MOTION_DETECTOR_H
```

#### 2.2 创建运动检测器实现
```cpp
// main/motion/motion_detector.cc
#include "motion_detector.h"
#include <esp_log.h>
#include <cmath>

#define TAG "MotionDetector"

MotionDetector::MotionDetector(Qmi8658* imu) : imu_(imu) {
}

void MotionDetector::SetEventCallback(EventCallback callback) {
    callback_ = callback;
}

void MotionDetector::Process() {
    ImuData data;
    if (imu_->ReadData(data) != ESP_OK) {
        return;
    }

    // 检查去抖时间
    int64_t current_time = esp_timer_get_time();
    if (current_time - last_event_time_us_ < DEBOUNCE_TIME_US) {
        return;
    }

    // 检测各种运动事件
    MotionEvent event = MotionEvent::NONE;
    
    if (DetectPickup(data)) {
        event = MotionEvent::PICKUP;
        ESP_LOGI(TAG, "Motion detected: PICKUP");
    } else if (DetectPutdownHard(data)) {
        event = MotionEvent::PUTDOWN_HARD;
        ESP_LOGI(TAG, "Motion detected: PUTDOWN_HARD");
    } else if (DetectShake(data)) {
        event = MotionEvent::SHAKE;
        ESP_LOGI(TAG, "Motion detected: SHAKE");
    } else if (DetectFlip(data)) {
        event = MotionEvent::FLIP;
        ESP_LOGI(TAG, "Motion detected: FLIP");
    }

    if (event != MotionEvent::NONE && callback_) {
        last_event_time_us_ = current_time;
        callback_(event);
    }

    last_data_ = data;
}

float MotionDetector::CalculateAccelMagnitude(const ImuData& data) {
    return std::sqrt(data.accel_x * data.accel_x + 
                    data.accel_y * data.accel_y + 
                    data.accel_z * data.accel_z);
}

bool MotionDetector::DetectPickup(const ImuData& data) {
    // 检测Z轴向上的加速度突变
    float z_diff = data.accel_z - last_data_.accel_z;
    return z_diff > PICKUP_THRESHOLD_G && std::abs(data.accel_z) > 0.5f;
}

bool MotionDetector::DetectPutdownHard(const ImuData& data) {
    // 检测强烈的向下冲击
    float magnitude = CalculateAccelMagnitude(data);
    float last_magnitude = CalculateAccelMagnitude(last_data_);
    return magnitude > PUTDOWN_THRESHOLD_G && magnitude > last_magnitude * 1.5f;
}

bool MotionDetector::DetectShake(const ImuData& data) {
    // 检测快速来回运动
    float accel_change = std::abs(data.accel_x - last_data_.accel_x) +
                        std::abs(data.accel_y - last_data_.accel_y) +
                        std::abs(data.accel_z - last_data_.accel_z);
    return accel_change > SHAKE_THRESHOLD_G;
}

bool MotionDetector::DetectFlip(const ImuData& data) {
    // 检测快速旋转
    float gyro_magnitude = std::sqrt(data.gyro_x * data.gyro_x + 
                                   data.gyro_y * data.gyro_y + 
                                   data.gyro_z * data.gyro_z);
    return gyro_magnitude > FLIP_THRESHOLD_DEG_S;
}
```

### 步骤3：集成到立创开发板

#### 3.1 修改立创开发板类
```cpp
// 在 main/boards/lichuang-dev/lichuang_dev_board.cc 中添加

#include "qmi8658.h"
#include "motion_detector.h"

class LichuangDevBoard : public WifiBoard {
private:
    // ... 现有成员变量 ...
    Qmi8658* imu_ = nullptr;
    MotionDetector* motion_detector_ = nullptr;

    void InitializeImu() {
        // 创建IMU设备
        imu_ = new Qmi8658(i2c_bus_);
        
        if (imu_->Initialize() == ESP_OK) {
            ESP_LOGI(TAG, "IMU initialized successfully");
            
            // 创建运动检测器
            motion_detector_ = new MotionDetector(imu_);
            
            // 设置运动事件回调
            motion_detector_->SetEventCallback([this](MotionEvent event) {
                HandleMotionEvent(event);
            });
        } else {
            ESP_LOGW(TAG, "Failed to initialize IMU");
            delete imu_;
            imu_ = nullptr;
        }
    }

    void HandleMotionEvent(MotionEvent event) {
        auto& app = Application::GetInstance();
        
        // 使用Schedule确保在主线程中执行
        app.Schedule([this, event]() {
            // LED反馈
            auto led = GetLed();
            switch (event) {
                case MotionEvent::PICKUP:
                    led->SetBrightness(100);
                    led->TurnOn();
                    // 播放成功音效
                    Application::GetInstance().PlaySound(Lang::Sounds::P3_SUCCESS);
                    break;
                    
                case MotionEvent::PUTDOWN_HARD:
                    led->SetBrightness(100);
                    led->Blink(3, 100);  // 快速闪烁3次
                    // 播放振动音效
                    Application::GetInstance().PlaySound(Lang::Sounds::P3_VIBRATION);
                    break;
                    
                case MotionEvent::SHAKE:
                    led->SetBrightness(80);
                    led->StartContinuousBlink(200);
                    // 播放感叹音效
                    Application::GetInstance().PlaySound(Lang::Sounds::P3_EXCLAMATION);
                    break;
                    
                case MotionEvent::FLIP:
                    led->StartFadeTask();  // 呼吸灯效果
                    break;
            }
            
            // 上报事件到云端
            ReportMotionEvent(event);
        });
    }

    void ReportMotionEvent(MotionEvent event) {
        // 构建事件消息
        const char* event_name = nullptr;
        switch (event) {
            case MotionEvent::PICKUP: event_name = "pickup"; break;
            case MotionEvent::PUTDOWN_HARD: event_name = "putdown_hard"; break;
            case MotionEvent::SHAKE: event_name = "shake"; break;
            case MotionEvent::FLIP: event_name = "flip"; break;
            default: return;
        }
        
        // 创建JSON消息
        char json_buffer[256];
        snprintf(json_buffer, sizeof(json_buffer),
                "{\"type\":\"event\",\"event_id\":\"%s\",\"timestamp\":%lld}",
                event_name, esp_timer_get_time() / 1000);  // 转换为毫秒
        
        // 发送到云端
        Application::GetInstance().SendMcpMessage(std::string(json_buffer));
        ESP_LOGI(TAG, "Motion event reported: %s", json_buffer);
    }

public:
    LichuangDevBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeTouch();
        InitializeButtons();
        InitializeCamera();
        InitializeImu();  // 添加IMU初始化
        
        GetBacklight()->RestoreBrightness();
    }

    // 添加获取运动检测器的方法
    MotionDetector* GetMotionDetector() {
        return motion_detector_;
    }
};
```

### 步骤4：集成到主应用程序

#### 4.1 修改Application类
```cpp
// 在 main/application.cc 中添加

void Application::Start() {
    // ... 现有代码 ...
    
    // 在主循环前启动运动检测定时器
    auto& board = Board::GetInstance();
    if (auto* lichuang_board = dynamic_cast<LichuangDevBoard*>(&board)) {
        if (auto* motion_detector = lichuang_board->GetMotionDetector()) {
            // 创建定时器，每50ms处理一次运动数据
            esp_timer_create_args_t motion_timer_args = {
                .callback = [](void* arg) {
                    auto* detector = static_cast<MotionDetector*>(arg);
                    detector->Process();
                },
                .arg = motion_detector,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "motion_timer",
                .skip_unhandled_events = true
            };
            esp_timer_handle_t motion_timer;
            esp_timer_create(&motion_timer_args, &motion_timer);
            esp_timer_start_periodic(motion_timer, 50000);  // 50ms
            
            ESP_LOGI(TAG, "Motion detection started");
        }
    }
    
    // ... 继续现有代码 ...
}
```

### 步骤5：修改CMakeLists.txt

```cmake
# 在 main/CMakeLists.txt 中添加
set(COMPONENT_SRCS 
    # ... 现有源文件 ...
    "boards/common/qmi8658.cc"
    "motion/motion_detector.cc"
)

set(COMPONENT_ADD_INCLUDEDIRS 
    # ... 现有包含目录 ...
    "motion"
)
```

## 测试验证

### 1. 编译测试
```bash
# 编译立创开发板固件
python scripts/release.py lichuang-dev
```

### 2. 烧录和监控
```bash
# 烧录固件
idf.py -p COM3 flash monitor

# 或者使用具体的串口
idf.py -p /dev/ttyUSB0 flash monitor
```

### 3. 功能验证
- **拿起设备**：LED变亮，播放成功音效，串口显示"Motion detected: PICKUP"
- **用力放下**：LED快速闪烁3次，播放振动音效
- **摇晃设备**：LED持续闪烁，播放感叹音效
- **翻转设备**：LED呼吸灯效果

### 4. 云端消息验证
检查MQTT/WebSocket消息，应该收到类似格式：
```json
{
    "type": "event",
    "event_id": "pickup",
    "timestamp": 1234567890
}
```

## 故障排查

### 1. IMU初始化失败
- 检查I2C连接是否正确
- 确认QMI8658的I2C地址（默认0x6B）
- 检查电源供应

### 2. 运动检测不灵敏
- 调整motion_detector.cc中的阈值常量
- 增加或减少处理频率（当前50ms）
- 检查去抖时间设置

### 3. 音频播放问题
- 确认音频文件存在于assets目录
- 检查音频编解码器是否正常工作
- 验证音频服务是否已启动

### 4. LED反馈异常
- 检查GPIO_48配置是否正确
- 验证LEDC PWM初始化
- 确认LED硬件连接

## 扩展建议

1. **添加更多运动模式**：
   - 双击检测
   - 长时间静止检测
   - 自由落体检测

2. **优化检测算法**：
   - 使用滑动窗口平滑数据
   - 实现自适应阈值
   - 添加机器学习模型

3. **增强反馈机制**：
   - 添加不同的LED颜色模式
   - 使用不同的音效组合
   - 实现振动马达反馈

4. **云端功能扩展**：
   - 记录运动历史数据
   - 实现运动模式学习
   - 添加用户自定义动作

## 总结

本实现提供了一个基础的运动检测框架，可以检测四种基本运动事件并提供即时的本地反馈和云端上报。代码遵循XiaoZhi项目的架构规范，易于扩展和维护。通过调整阈值和添加新的检测逻辑，可以实现更复杂的交互功能。