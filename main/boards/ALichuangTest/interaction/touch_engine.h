#ifndef ALICHUANGTEST_TOUCH_ENGINE_H
#define ALICHUANGTEST_TOUCH_ENGINE_H

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <functional>
#include <vector>
#include <algorithm>
#include "touch_config.h"

// 触摸事件类型
enum class TouchEventType {
    NONE,
    SINGLE_TAP,     // 单击（左或右，<500ms）
    HOLD,           // 长按（>500ms）
    RELEASE,        // 释放（之前有HOLD）
    CRADLED,        // 摇篮模式（双侧持续触摸>2秒且IMU静止）
    TICKLED,        // 挠痒模式（2秒内多次无规律触摸>4次）
};

// 触摸位置
enum class TouchPosition {
    LEFT,           // GPIO10
    RIGHT,          // GPIO11
    BOTH,           // 双侧同时
    ANY,            // 任意侧（用于tickled事件）
};

// 触摸事件数据结构
struct TouchEvent {
    TouchEventType type;
    TouchPosition position;
    int64_t timestamp_us;
    uint32_t duration_ms;  // 对于HOLD和RELEASE事件，记录持续时间
    
    TouchEvent() : type(TouchEventType::NONE), position(TouchPosition::LEFT), 
                   timestamp_us(0), duration_ms(0) {}
};

// 触摸引擎类 - 专门处理触摸输入
class TouchEngine {
public:
    using TouchEventCallback = std::function<void(const TouchEvent&)>;
    
    TouchEngine();
    ~TouchEngine();
    
    // 初始化引擎
    void Initialize();
    
    // 加载配置
    void LoadConfiguration(const char* config_path = nullptr);
    
    // 注册事件回调
    void RegisterCallback(TouchEventCallback callback);
    
    // 启用/禁用触摸检测
    void Enable(bool enable) { enabled_ = enable; }
    bool IsEnabled() const { return enabled_; }
    
    // 处理函数（由任务调用）
    void Process();
    
    // 获取触摸状态
    bool IsLeftTouched() const { return left_touched_; }
    bool IsRightTouched() const { return right_touched_; }
    
private:
    // GPIO配置
    static constexpr gpio_num_t GPIO_TOUCH_LEFT = GPIO_NUM_10;
    static constexpr gpio_num_t GPIO_TOUCH_RIGHT = GPIO_NUM_11;
    
    // 配置参数（从配置文件加载）
    TouchDetectionConfig config_;
    
    // 触摸状态
    struct TouchState {
        bool is_touched;
        bool was_touched;
        int64_t touch_start_time;
        int64_t last_change_time;
        bool event_triggered;  // 替代hold_triggered，更通用
    };
    
    // 挠痒检测状态
    struct TickleDetector {
        std::vector<int64_t> touch_times;  // 记录触摸时间戳
        int64_t window_start_time;
        
        TickleDetector() : window_start_time(0) {}
    };
    
    // 状态变量
    bool enabled_;
    bool left_touched_;
    bool right_touched_;
    TouchState left_state_;
    TouchState right_state_;
    
    // ESP32-S3触摸传感器相关
    uint32_t left_baseline_;   // 左侧触摸基准值
    uint32_t right_baseline_;  // 右侧触摸基准值
    uint32_t left_threshold_;  // 左侧触摸阈值
    uint32_t right_threshold_; // 右侧触摸阈值
    
    // 传感器卡死检测
    int stuck_detection_count_;
    static const int STUCK_THRESHOLD = 10; // 连续10次检测到卡死状态
    
    // 特殊事件检测
    TickleDetector tickle_detector_;
    int64_t both_touch_start_time_;  // 双侧同时触摸开始时间
    bool cradled_triggered_;
    
    // 任务相关
    TaskHandle_t task_handle_;
    static void TouchTask(void* param);
    
    // 事件回调
    std::vector<TouchEventCallback> callbacks_;
    
    // GPIO初始化
    void InitializeGPIO();
    
    // 读取基准值（新增）
    void ReadBaseline();
    
    // 重置触摸传感器
    void ResetTouchSensor();
    
    // 处理单个触摸输入（旧版本，保留兼容）
    void ProcessTouch(gpio_num_t gpio, TouchPosition position, TouchState& state);
    
    // 处理触摸状态
    void ProcessSingleTouch(bool currently_touched, TouchPosition position, TouchState& state);
    void ProcessSpecialEvents();  // 处理特殊事件（cradled, tickled）
    bool IsIMUStable();  // 检查IMU是否稳定（用于cradled检测）
    
    // 事件分发
    void DispatchEvent(const TouchEvent& event);
    
    // 读取GPIO状态（旧版本，保留兼容）
    bool ReadTouchGPIO(gpio_num_t gpio);
};

#endif // ALICHUANGTEST_TOUCH_ENGINE_H