#ifndef ALICHUANGTEST_TOUCH_ENGINE_H
#define ALICHUANGTEST_TOUCH_ENGINE_H

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <functional>
#include <vector>

// 触摸事件类型
enum class TouchEventType {
    NONE,
    SINGLE_TAP,     // 单击
    HOLD,           // 长按
    RELEASE,        // 释放（长按后）
};

// 触摸位置
enum class TouchPosition {
    LEFT,           // GPIO10
    RIGHT,          // GPIO11
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
    
    // 时间阈值（毫秒）
    static constexpr uint32_t TAP_MAX_DURATION_MS = 500;     // 超过500ms认为是长按
    static constexpr uint32_t HOLD_MIN_DURATION_MS = 500;    // 最少500ms才触发长按
    static constexpr uint32_t DEBOUNCE_TIME_MS = 50;         // 消抖时间
    
    // 触摸状态
    struct TouchState {
        bool is_touched;
        bool was_touched;
        int64_t touch_start_time;
        int64_t last_change_time;
        bool hold_triggered;
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
    
    // 任务相关
    TaskHandle_t task_handle_;
    static void TouchTask(void* param);
    
    // 事件回调
    std::vector<TouchEventCallback> callbacks_;
    
    // GPIO初始化
    void InitializeGPIO();
    
    // 读取基准值（新增）
    void ReadBaseline();
    
    // 处理单个触摸输入（旧版本，保留兼容）
    void ProcessTouch(gpio_num_t gpio, TouchPosition position, TouchState& state);
    
    // 处理触摸状态（新版本）
    void ProcessTouchWithState(bool currently_touched, TouchPosition position, TouchState& state);
    
    // 事件分发
    void DispatchEvent(const TouchEvent& event);
    
    // 读取GPIO状态（旧版本，保留兼容）
    bool ReadTouchGPIO(gpio_num_t gpio);
};

#endif // ALICHUANGTEST_TOUCH_ENGINE_H