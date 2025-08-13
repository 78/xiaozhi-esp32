#ifndef ALICHUANGTEST_MOTION_ENGINE_H
#define ALICHUANGTEST_MOTION_ENGINE_H

#include "../qmi8658.h"
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>

// 运动事件类型
enum class MotionEventType {
    NONE,
    FREE_FALL,          // 自由落体
    SHAKE_VIOLENTLY,    // 剧烈摇晃
    FLIP,               // 设备被快速翻转
    SHAKE,              // 设备被摇晃
    PICKUP,             // 设备被拿起
    UPSIDE_DOWN,        // 设备被倒置（持续状态）
};

// 运动事件数据结构
struct MotionEvent {
    MotionEventType type;
    int64_t timestamp_us;
    ImuData imu_data;
    
    MotionEvent() : type(MotionEventType::NONE), timestamp_us(0) {}
    MotionEvent(MotionEventType t) : type(t), timestamp_us(0) {}
};

// 运动引擎类 - 专门处理IMU相关的运动检测
class MotionEngine {
public:
    using MotionEventCallback = std::function<void(const MotionEvent&)>;
    
    MotionEngine();
    ~MotionEngine();
    
    // 初始化引擎
    void Initialize(Qmi8658* imu);
    
    // 注册事件回调
    void RegisterCallback(MotionEventCallback callback);
    
    // 处理函数（在主循环中调用）
    void Process();
    
    // 启用/禁用运动检测
    void Enable(bool enable) { enabled_ = enable; }
    bool IsEnabled() const { return enabled_; }
    
    // 获取运动状态
    bool IsPickedUp() const { return is_picked_up_; }
    bool IsUpsideDown() const { return is_upside_down_; }
    const ImuData& GetCurrentImuData() const { return current_imu_data_; }
    
    // 调试输出控制
    void SetDebugOutput(bool enable) { debug_output_ = enable; }
    
private:
    // IMU相关
    Qmi8658* imu_;
    bool enabled_;
    
    // 事件回调
    std::vector<MotionEventCallback> callbacks_;
    
    // 运动检测状态
    ImuData current_imu_data_;
    ImuData last_imu_data_;
    bool first_reading_;
    std::unordered_map<MotionEventType, int64_t> last_event_times_;
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
    int64_t pickup_start_time_;
    
    // 运动检测阈值
    static constexpr float FREE_FALL_THRESHOLD_G = 0.3f;
    static constexpr int64_t FREE_FALL_MIN_TIME_US = 200000;
    static constexpr float SHAKE_VIOLENTLY_THRESHOLD_G = 3.0f;
    static constexpr float SHAKE_THRESHOLD_G = 1.5f;
    static constexpr float FLIP_THRESHOLD_DEG_S = 400.0f;
    static constexpr float PICKUP_THRESHOLD_G = 0.15f;
    static constexpr float UPSIDE_DOWN_THRESHOLD_G = -0.8f;
    static constexpr int UPSIDE_DOWN_STABLE_COUNT = 10;
    static constexpr int64_t DEBUG_INTERVAL_US = 1000000;
    
    // 事件特定的冷却时间（微秒）
    static constexpr int64_t FREE_FALL_COOLDOWN_US = 500000;      // 500ms
    static constexpr int64_t SHAKE_VIOLENTLY_COOLDOWN_US = 400000; // 400ms
    static constexpr int64_t FLIP_COOLDOWN_US = 300000;           // 300ms
    static constexpr int64_t SHAKE_COOLDOWN_US = 200000;          // 200ms
    static constexpr int64_t PICKUP_COOLDOWN_US = 1000000;        // 1s
    static constexpr int64_t UPSIDE_DOWN_COOLDOWN_US = 500000;    // 500ms
    
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
    
    // 辅助函数
    bool IsStable(const ImuData& data, const ImuData& last_data);
    
    // 事件分发
    void DispatchEvent(const MotionEvent& event);
};

#endif // ALICHUANGTEST_MOTION_ENGINE_H