#ifndef ALICHUANGTEST_EVENT_ENGINE_H
#define ALICHUANGTEST_EVENT_ENGINE_H

#include "../qmi8658.h"
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

// 前向声明
class Qmi8658;

// 事件引擎类
class EventEngine {
public:
    using EventCallback = std::function<void(const Event&)>;
    
    EventEngine();
    ~EventEngine();
    
    // 初始化引擎
    void Initialize(Qmi8658* imu = nullptr);
    
    // 注册事件回调
    void RegisterCallback(EventCallback callback);
    void RegisterCallback(EventType type, EventCallback callback);
    
    // 处理函数（在主循环中调用）
    void Process();
    
    // 手动触发事件
    void TriggerEvent(const Event& event);
    void TriggerEvent(EventType type);
    
    // 运动检测相关接口
    void EnableMotionDetection(bool enable) { motion_detection_enabled_ = enable; }
    bool IsMotionDetectionEnabled() const { return motion_detection_enabled_; }
    
    // 获取运动状态
    bool IsPickedUp() const { return is_picked_up_; }
    bool IsUpsideDown() const { return is_upside_down_; }
    const ImuData& GetCurrentImuData() const { return current_imu_data_; }
    
    // 调试输出控制
    void SetDebugOutput(bool enable) { debug_output_ = enable; }
    
private:
    // IMU相关
    Qmi8658* imu_;
    bool motion_detection_enabled_;
    
    // 事件回调
    EventCallback global_callback_;
    std::vector<std::pair<EventType, EventCallback>> type_callbacks_;
    
    // 运动检测状态
    ImuData current_imu_data_;
    ImuData last_imu_data_;
    bool first_reading_;
    int64_t last_event_time_us_;
    int64_t last_debug_time_us_;
    bool debug_output_;
    
    // 自由落体检测的状态跟踪
    int64_t free_fall_start_time_;
    bool in_free_fall_;
    
    // 倒置检测的状态跟踪
    bool is_upside_down_;
    int upside_down_count_;
    
    // 拿起检测的状态跟踪
    bool is_picked_up_;
    int stable_count_;
    float stable_z_reference_;
    
    // 运动检测阈值
    static constexpr float FREE_FALL_THRESHOLD_G = 0.3f;
    static constexpr int64_t FREE_FALL_MIN_TIME_US = 200000;
    static constexpr float SHAKE_VIOLENTLY_THRESHOLD_G = 3.0f;
    static constexpr float SHAKE_THRESHOLD_G = 1.5f;
    static constexpr float FLIP_THRESHOLD_DEG_S = 400.0f;
    static constexpr float PICKUP_THRESHOLD_G = 0.15f;
    static constexpr float UPSIDE_DOWN_THRESHOLD_G = -0.8f;
    static constexpr int UPSIDE_DOWN_STABLE_COUNT = 10;
    static constexpr int64_t DEBOUNCE_TIME_US = 300000;
    static constexpr int64_t DEBUG_INTERVAL_US = 1000000;
    
    // 运动检测方法
    void ProcessMotionDetection();
    bool DetectFreeFall(const ImuData& data, int64_t current_time);
    bool DetectShakeViolently(const ImuData& data);
    bool DetectFlip(const ImuData& data);
    bool DetectShake(const ImuData& data);
    bool DetectPickup(const ImuData& data);
    bool DetectUpsideDown(const ImuData& data);
    float CalculateAccelMagnitude(const ImuData& data);
    float CalculateAccelDelta(const ImuData& current, const ImuData& last);
    
    // 事件分发
    void DispatchEvent(const Event& event);
};

#endif // ALICHUANGTEST_EVENT_ENGINE_H