name: "XiaoZhi ESP32 事件驱动物理交互框架"
description: |

## 目的
为 XiaoZhi ESP32 AI 语音聊天机器人增强事件驱动的物理交互框架，实现即时本地响应和智能云端上报功能。通过基于传感器的事件检测和多模态反馈，增加"生命感"的交互行为。

## 核心原则
1. **上下文为王**: 包含所有必要的 ESP-IDF 模式、XiaoZhi 架构知识和集成点
2. **验证循环**: 提供 AI 可以运行和修复的可执行测试和构建命令
3. **信息密集**: 使用现有 XiaoZhi 代码库中的关键词和模式
4. **渐进式成功**: 从 LED 验证开始，验证后再增强完整反馈
5. **最小侵入**: 新功能作为独立组件，对现有代码的修改最小化

---

## 目标
为立创 S3 开发板创建事件驱动的物理交互系统，使设备能够：
- 通过 I2C 稳定读取 QMI8658 六轴 IMU 传感器数据
- 检测 4 个关键的物理交互事件：pickup（拿起）、putdown_hard（用力放下）、flip（翻转）、shake（摇晃）
- 使用内置的 GPIO_NUM_48 LED 提供即时反馈
- 将检测到的事件 ID 记录到串口控制台以供验证
- 为扩展到其他开发板和更复杂交互奠定基础

## 为什么
- **用户体验**: 将设备从纯语音交互转变为多模态交互
- **响应性**: 本地反应消除了物理交互的网络延迟
- **上下文感知**: 物理事件为 LLM 决策提供关键上下文
- **可扩展性**: 框架支持未来的传感器集成和行为扩展

## 是什么
一个 ESP-IDF 组件 (`event_engine`)，它：
- 从 SPIFFS/LittleFS 加载事件配置，带有默认值回退
- 持续处理传感器数据以检测配置的事件
- 管理优先级动作队列以执行本地反馈
- 与现有的 MQTT/WebSocket 协议集成进行云端上报
- 在 XiaoZhi 的单线程事件循环架构内工作

### 成功标准
- [ ] QMI8658 传感器成功初始化并提供稳定的加速度计/陀螺仪读数
- [ ] 四个运动事件可靠检测：pickup、putdown_hard、flip、shake
- [ ] 内置 LED（GPIO_NUM_48）对每个事件响应不同的模式
- [ ] 串口控制台显示清晰的事件检测日志和时间戳
- [ ] 事件检测在不同运动速度和方向下工作一致
- [ ] 内存使用保持在额外 100KB 堆使用以下
- [ ] 构建系统与现有 lichuang-dev 开发板配置干净集成

## 所需的全部上下文

### 文档和参考资料
```yaml
# 必读 - 在你的上下文窗口中包含这些内容
- file: main/application.cc
  why: 核心应用程序单例模式和事件循环
  
- file: main/boards/lichuang-dev/lichuang_dev_board.cc
  why: 目标开发板实现及 I2C 配置
  
- file: main/boards/lichuang-dev/config.h
  why: 目标硬件的 GPIO 引脚、I2C 配置、LED 设置
  
- file: main/boards/common/i2c_device.h
  why: 项目中使用的 I2C 设备基类模式
  
- file: main/led/gpio_led.cc
  why: GPIO_NUM_48 反馈的 LED 控制模式
  
- file: main/audio/codecs/es8311_audio_codec.cc
  why: I2C 设备实现模式示例

- url: https://datasheet.lcsc.com/szlcsc/2109011930_QST-QMI8658_C2844981.pdf
  why: QMI8658 数据手册，寄存器定义和通信协议
  
- url: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/i2c.html
  why: ESP-IDF I2C 驱动程序文档
  
- url: https://github.com/QSTCorp/QMI8658-Driver-C/blob/main/qmi8658.c
  why: QMI8658 官方 C 驱动程序参考，初始化序列
```

### 当前立创开发板结构
```bash
xiaozhi-esp32/
├── main/
│   ├── application.cc/h          # 核心单例应用程序
│   ├── boards/lichuang-dev/      # 目标开发板配置
│   │   ├── config.h              # I2C 引脚：SDA=GPIO_1, SCL=GPIO_2, LED=GPIO_48
│   │   ├── config.json           # ESP32-S3 构建配置
│   │   └── lichuang_dev_board.cc # 开发板实现及 I2C 总线设置
│   ├── boards/common/            # 共享硬件驱动
│   │   ├── i2c_device.h/cc       # I2C 设备基类（400kHz，100ms 超时）
│   │   └── pca9557.h/cc          # GPIO 扩展器（现有 I2C 设备）
│   ├── led/                      # LED 控制系统
│   │   ├── led.h                 # LED 基接口
│   │   ├── gpio_led.cc           # GPIO LED 实现
│   │   └── single_led.cc         # 单个 LED 包装器
│   ├── audio/codecs/             # I2C 设备示例
│   │   └── es8311_audio_codec.cc # I2C 模式参考（0x18 地址）
│   └── CMakeLists.txt           # 构建配置
└── scripts/
    └── release.py               # 构建：python scripts/release.py lichuang-dev
```

### 目标实现结构（立创开发板聚焦）
```bash
xiaozhi-esp32/
├── main/
│   ├── sensors/                  # 新增：传感器子系统
│   │   ├── sensor.h             # 传感器基接口
│   │   ├── imu_sensor.h         # IMU 传感器基类
│   │   └── motion_detector.cc/h # 运动事件检测逻辑
│   │
│   ├── boards/common/           # 添加 QMI8658 驱动
│   │   └── qmi8658.cc/h         # QMI8658 六轴 IMU 驱动（I2C 地址 0x6B）
│   │
│   ├── boards/lichuang-dev/     # 修改目标开发板
│   │   ├── config.h             # 添加：QMI8658_ADDR，运动检测阈值
│   │   └── lichuang_dev_board.cc # 添加：GetImuSensor() 方法
│   │
│   ├── motion_engine.cc/h       # 新增：核心运动事件系统
│   ├── led_feedback.cc/h        # 新增：事件的 LED 模式控制器
│   │
│   ├── application.cc           # 修改：添加 motion_engine 初始化
│   └── CMakeLists.txt          # 修改：添加传感器和运动源文件
│
└── 测试验证：                    # 测试方法
    ├── 串口监控器                # 事件日志："Motion detected: pickup"
    ├── LED 模式                 # 每种事件类型的视觉反馈
    └── 物理测试                 # 手动运动测试
```

### 已知陷阱和立创开发板特性
```cpp
// 关键：XiaoZhi 使用单线程事件循环 - 回调中不能阻塞
// 关键：使用 Application::Schedule() 进行延迟执行
// 关键：I2C 总线与音频编解码器（ES8311）、摄像头、PCA9557 GPIO 扩展器共享
// 关键：QMI8658 默认 I2C 地址是 0x6B（地址引脚可设为 0x6A）
// 关键：内置 LED 在 GPIO_NUM_48 - 高电平有效逻辑
// 关键：I2C 总线运行在 400kHz - QMI8658 支持最高 400kHz
// 关键：QMI8658 需要正确的上电序列：VDD 后 15ms 延迟
// 关键：运动检测算法必须避免音频振动造成的误触发
// 关键：使用带有 "motion" 标签的 ESP_LOG 宏进行调试
// 关键：在开发板初始化配置 I2C 总线后初始化 IMU
```

## 实现蓝图

### 数据模型和结构

```cpp
// main/sensors/motion_detector.h - 运动事件定义
namespace motion {

enum class MotionEvent {
    PICKUP,        // 设备从表面拿起
    PUTDOWN_HARD,  // 设备用力放下，冲击 > 2g
    FLIP,          // 设备快速旋转 180 度
    SHAKE          // 检测到快速来回运动
};

enum class LedPattern {
    SOLID_CYAN,     // 拿起事件的纯色
    RED_FLASH,      // 用力放下的快速闪烁
    BLUE_SWEEP,     // 翻转的扫描模式
    YELLOW_BLINK    // 摇晃的快速闪烁
};

struct ImuReading {
    float accel_x, accel_y, accel_z;  // 单位 g（±8g 范围）
    float gyro_x, gyro_y, gyro_z;     // 单位 deg/s（±2000 dps 范围）
    uint64_t timestamp_us;            // 微秒时间戳
};

struct MotionTrigger {
    float accel_threshold;            // 加速度阈值，单位 g
    float gyro_threshold;             // 陀螺仪阈值，单位 deg/s
    uint32_t min_duration_ms;         // 最小事件持续时间
    uint32_t debounce_ms;            // 事件间去抖时间
};

// QMI8658 的事件检测阈值
struct MotionThresholds {
    static constexpr MotionTrigger PICKUP = {
        .accel_threshold = 1.5f,      // 1.5g 向上加速度
        .gyro_threshold = 50.0f,      // 50 deg/s 旋转
        .min_duration_ms = 100,       // 100ms 最小持续时间
        .debounce_ms = 500            // 拿起间 500ms 去抖
    };
    
    static constexpr MotionTrigger PUTDOWN_HARD = {
        .accel_threshold = 2.0f,      // 2g 冲击
        .gyro_threshold = 30.0f,      // 30 deg/s 稳定
        .min_duration_ms = 50,        // 50ms 冲击时间
        .debounce_ms = 300            // 冲击间 300ms 去抖
    };
    
    static constexpr MotionTrigger FLIP = {
        .accel_threshold = 1.0f,      // 旋转期间 1g
        .gyro_threshold = 200.0f,     // 200 deg/s 旋转
        .min_duration_ms = 200,       // 200ms 旋转时间
        .debounce_ms = 1000           // 翻转间 1s 去抖
    };
    
    static constexpr MotionTrigger SHAKE = {
        .accel_threshold = 2.5f,      // 2.5g 摇晃强度
        .gyro_threshold = 100.0f,     // 100 deg/s 摇晃
        .min_duration_ms = 300,       // 300ms 摇晃持续时间
        .debounce_ms = 500            // 摇晃间 500ms 去抖
    };
};

} // namespace motion
```

### 任务列表

```yaml
任务 1: 设置组件结构
创建 components/event_engine/CMakeLists.txt:
  - 模式：遵循 managed_components/ 中的 ESP-IDF 组件模式
  - 使用 REQUIRES 注册组件：esp_event, driver, spiffs, json
  - 包含目录和源文件

创建 components/event_engine/Kconfig:
  - 为 EVENT_ENGINE 配置添加菜单
  - IMU 类型选项（MPU6886、MPU6050、NONE）
  - 事件配置路径选项（默认："/spiffs/events_config.json"）

任务 2: 实现 IMU 驱动抽象
创建 include/imu_driver.h:
  - 模式：类似于 audio_codec.h 抽象
  - 带有 Init()、ReadAccel()、ReadGyro() 的虚基类
  - 优雅地支持"无 IMU"板

创建 src/imu_mpu6886.cc:
  - 模式：像音频编解码器一样使用 ESP-IDF I2C 驱动
  - 从板配置读取 I2C 引脚
  - 实现可配置速率的连续读取

任务 3: 事件配置系统
创建 src/event_config.cc:
  - 模式：遵循 settings.cc JSON 处理方法
  - 首先从 SPIFFS 加载，回退到 DEFAULT_EVENTS_JSON
  - 使用 cJSON 库解析（项目中已有）
  - 验证配置完整性

任务 4: 事件检测引擎
创建 src/event_detector.cc:
  - 模式：像 Application::Schedule() 一样使用 ESP-IDF 定时器
  - 根据配置的触发器处理 IMU 数据
  - 为传感器事件实现去抖动
  - 跟踪派生事件（空闲定时器）

任务 5: 动作队列实现
创建 src/action_queue.cc:
  - 模式：像 audio_service.cc 一样使用 FreeRTOS 队列
  - 基于优先级的执行和抢占
  - 在主线程中非阻塞执行
  - 通过板抽象进行 LED 控制

任务 6: 云端报告器集成
创建 src/cloud_reporter.cc:
  - 模式：像现有消息一样使用 Protocol 接口
  - 创建带有 event_id、timestamp 的 JSON 事件消息
  - 通过 Application 的协议实例排队发送
  - 处理离线缓冲

任务 7: 主程序集成
修改 main/application.cc:
  - 添加 event_engine::EventEngine* 成员
  - 在板初始化后在构造函数中初始化
  - 在主循环中调用 Update()
  - 在析构函数中清理关闭

修改 main/CMakeLists.txt:
  - 将 event_engine 添加到 REQUIRES
  - 确保组件路径已注册

任务 8: 测试和验证
创建 test/test_event_engine.cc:
  - 事件检测逻辑的单元测试
  - 模拟 IMU 数据注入
  - 验证动作队列优先级
  - 测试配置加载/回退

创建 data/events_config.json:
  - 初始事件库的完整配置
  - 包含规范中的所有 5 个事件
  - 测试各种触发条件
```

### 每个任务的实现细节

```cpp
// 任务 2：IMU 驱动模式
class ImuDriver {
public:
    virtual ~ImuDriver() = default;
    virtual esp_err_t Initialize(const BoardConfig& config) = 0;
    virtual esp_err_t ReadAcceleration(float& x, float& y, float& z) = 0;
    virtual bool IsPresent() const = 0;
};

class Mpu6886Driver : public ImuDriver {
    // 模式：类似于 Es8311Codec 实现
    esp_err_t Initialize(const BoardConfig& config) override {
        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = config.i2c_sda,  // 来自板配置
            .scl_io_num = config.i2c_scl,
            // ... 标准 I2C 设置
        };
        ESP_RETURN_ON_ERROR(i2c_param_config(I2C_NUM_0, &conf), TAG, "I2C 配置失败");
        
        // MPU6886 初始化序列
        ESP_RETURN_ON_ERROR(WriteRegister(PWR_MGMT_1, 0x00), TAG, "唤醒 MPU6886 失败");
        // ... 更多初始化
    }
};

// 任务 4：事件检测模式
void EventDetector::ProcessSensorData() {
    // 关键：从主循环调用，不能阻塞
    float x, y, z;
    if (imu_->ReadAcceleration(x, y, z) == ESP_OK) {
        // 检查每个传感器事件触发器
        for (const auto& event : sensor_events_) {
            if (CheckTriggerCondition(event, x, y, z)) {
                // 使用 Application::Schedule 进行延迟处理
                Application::GetInstance().Schedule([this, event]() {
                    HandleEventTriggered(event);
                });
            }
        }
    }
}

// 任务 7：Application 集成
void Application::InitializeEventEngine() {
    // 模式：在板初始化后，协议之前
    if (board_->HasImu()) {  // 检查板能力
        event_engine_ = std::make_unique<event_engine::EventEngine>();
        
        // 传递协议实例用于云端报告
        event_engine_->SetProtocol(protocol_.get());
        
        // 使用板配置初始化
        esp_err_t ret = event_engine_->Initialize(board_->GetConfig());
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "事件引擎初始化失败: %s", esp_err_to_name(ret));
            event_engine_.reset();  // 如果初始化失败则禁用
        }
    }
}
```

### 集成点
```yaml
构建系统：
  - 添加到：main/CMakeLists.txt
  - 内容：|
      set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../components)
      
开发板：
  - 带 IMU 的板：esp-box-3、m5stack-atoms3 等
  - 向 Board 基类添加 HasImu() 方法
  - IMU I2C 引脚来自现有板配置

SPIFFS 数据：
  - 在分区表中创建 spiffs_data 分区
  - 添加到 scripts/release.py 用于刷写
  - 挂载点：/spiffs

协议集成：
  - 事件消息使用 type: "event"
  - JSON 格式：{"type": "event", "event_id": "pickup", "timestamp": 1234567890}
  - 通过现有协议 OnIncomingJson 流程发送
```

## 验证循环

### 级别 1：组件构建
```bash
# 首先单独构建组件
cd components/event_engine
idf.py build

# 预期：组件构建无错误
# 如果有错误：检查 CMakeLists.txt REQUIRES
```

### 级别 2：集成构建
```bash
# 为 ESP-BOX-3 构建（有 MPU6886）
python scripts/release.py esp-box-3

# 预期：固件成功构建
# 检查：日志中 event_engine 已初始化
```

### 级别 3：功能测试
```bash
# 刷写固件和 SPIFFS 数据
idf.py -p /dev/ttyUSB0 flash
idf.py -p /dev/ttyUSB0 spiffs_data-flash

# 监控串口输出
idf.py monitor

# 预期日志序列：
# I (1234) event_engine: 从 /spiffs/events_config.json 加载配置
# I (1235) event_engine: 加载了 5 个事件
# I (1236) event_engine: IMU MPU6886 已初始化
# I (1237) event_engine: 事件引擎已启动

# 物理测试：拿起设备
# 预期：LED 变为青色持续 2 秒
# 预期日志："事件触发：pickup"
```

### 级别 4：云端上报测试
```python
# 使用修改后的 audio_debug_server.py 测试以显示事件消息
# 或检查 MQTT 代理的事件消息

# 检测到拿起时的预期消息：
{
    "type": "event",
    "event_id": "pickup", 
    "timestamp": 1234567890,
    "metadata": {
        "z_accel": 0.45,
        "magnitude": 1.2
    }
}
```

## 最终验证清单
- [ ] 组件独立构建
- [ ] 固件为多种板类型构建
- [ ] IMU 数据成功读取（检查日志）
- [ ] 事件配置从 SPIFFS 加载
- [ ] 回退到默认配置工作
- [ ] 所有事件的 LED 反馈可见
- [ ] 服务器接收到云端事件
- [ ] 无内存泄漏（监控堆）
- [ ] CPU 使用率可接受（空闲时 <5%）
- [ ] 在没有 IMU 的板上工作（优雅禁用）

---

## 要避免的反模式
- ❌ 不要在事件回调中阻塞 - 使用 Schedule()
- ❌ 不要动态分配大缓冲区 - 使用静态
- ❌ 不要假设所有板都有 IMU - 检查能力
- ❌ 不要硬编码 I2C 引脚 - 使用板配置
- ❌ 不要破坏现有音频管道 - 检查 I2C 冲突
- ❌ 不要直接修改设备状态 - 使用适当的事件
- ❌ 不要忽略板变体 - 测试多种配置

## 信心分数：8.5/10

高信心源于：
- 对 XiaoZhi 架构的深入理解
- 明确识别的集成点
- 可遵循的现有模式
- 模块化组件方法

轻微的不确定性：
- 所有 80+ 板上的 IMU 传感器可用性
- 与音频编解码器的潜在 I2C 冲突
- 所有板配置上的 SPIFFS 分区空间