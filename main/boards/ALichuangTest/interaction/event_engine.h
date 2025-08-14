#ifndef ALICHUANGTEST_EVENT_ENGINE_H
#define ALICHUANGTEST_EVENT_ENGINE_H

#include "motion_engine.h"
#include "touch_engine.h"
#include <functional>
#include <memory>
#include <vector>

// 事件类型枚举 - 包含所有可能的事件
enum class EventType {
    // 运动事件
    MOTION_NONE,
    MOTION_FREE_FALL,      // 自由落体
    MOTION_SHAKE_VIOLENTLY,// 剧烈摇晃
    MOTION_FLIP,           // 设备被快速翻转
    MOTION_SHAKE,          // 设备被摇晃
    MOTION_PICKUP,         // 设备被拿起
    MOTION_UPSIDE_DOWN,    // 设备被倒置（持续状态）
    
    // 触摸事件（预留）
    TOUCH_TAP,
    TOUCH_DOUBLE_TAP,
    TOUCH_LONG_PRESS,
    TOUCH_SWIPE_UP,
    TOUCH_SWIPE_DOWN,
    TOUCH_SWIPE_LEFT,
    TOUCH_SWIPE_RIGHT,
    
    // 音频事件（预留）
    AUDIO_WAKE_WORD,
    AUDIO_SPEAKING,
    AUDIO_LISTENING,
    
    // 系统事件（预留）
    SYSTEM_BOOT,
    SYSTEM_SHUTDOWN,
    SYSTEM_ERROR
};

// 事件数据结构
struct Event {
    EventType type;
    int64_t timestamp_us;
    union {
        ImuData imu_data;      // 运动事件的IMU数据
        struct {                // 触摸事件的坐标数据
            int x;
            int y;
        } touch_data;
        int audio_level;        // 音频事件的音量级别
        int error_code;         // 系统事件的错误码
    } data;
    
    Event() : type(EventType::MOTION_NONE), timestamp_us(0) {}
    Event(EventType t) : type(t), timestamp_us(0) {}
};

// 现在可以包含 event_processor.h，因为 Event 和 EventType 已定义
#include "event_processor.h"

// 前向声明
class MotionEngine;
class TouchEngine;
class Qmi8658;

// 事件引擎类 - 作为各种事件源的协调器
class EventEngine {
public:
    using EventCallback = std::function<void(const Event&)>;
    
    EventEngine();
    ~EventEngine();
    
    // 初始化引擎 - 内部创建和管理子引擎
    void Initialize();
    
    // 初始化运动引擎（内部创建）
    void InitializeMotionEngine(Qmi8658* imu, bool enable_debug = false);
    
    // 初始化触摸引擎（内部创建）
    void InitializeTouchEngine();
    
    // 注册事件回调
    void RegisterCallback(EventCallback callback);
    void RegisterCallback(EventType type, EventCallback callback);
    
    // 处理函数（在主循环中调用）
    void Process();
    
    // 手动触发事件
    void TriggerEvent(const Event& event);
    void TriggerEvent(EventType type);
    
    // 获取运动状态（通过MotionEngine）
    bool IsPickedUp() const;
    bool IsUpsideDown() const;
    
    // 获取触摸状态（通过TouchEngine）
    bool IsLeftTouched() const;
    bool IsRightTouched() const;
    
    // 配置事件处理策略
    void ConfigureEventProcessing(EventType type, const EventProcessingConfig& config);
    void SetDefaultProcessingStrategy(const EventProcessingConfig& config);
    
    // 获取事件统计
    EventProcessor::EventStats GetEventStats(EventType type) const;
    
private:
    // 运动引擎（内部创建和管理）
    MotionEngine* motion_engine_;
    bool owns_motion_engine_;
    // 触摸引擎（内部创建和管理）
    TouchEngine* touch_engine_;
    bool owns_touch_engine_;
    
    // 事件处理器
    EventProcessor* event_processor_;
    
    // 初始化子引擎的回调
    void SetupMotionEngineCallbacks();
    void SetupTouchEngineCallbacks();
    
    // 配置默认的事件处理策略
    void ConfigureDefaultEventProcessing();
    
    // 从配置文件或默认配置加载
    void LoadEventConfiguration();
    
    // 事件回调
    EventCallback global_callback_;
    std::vector<std::pair<EventType, EventCallback>> type_callbacks_;
    
    // 运动事件回调处理
    void OnMotionEvent(const MotionEvent& motion_event);
    
    // 触摸事件回调处理
    void OnTouchEvent(const TouchEvent& touch_event);
    
    // 事件分发
    void DispatchEvent(const Event& event);
    
    // 事件类型转换
    EventType ConvertMotionEventType(MotionEventType motion_type);
    EventType ConvertTouchEventType(TouchEventType touch_type, TouchPosition position);
};

#endif // ALICHUANGTEST_EVENT_ENGINE_H