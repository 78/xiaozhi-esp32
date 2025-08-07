#ifndef ALICHUANGTEST_MOTION_DETECTOR_H
#define ALICHUANGTEST_MOTION_DETECTOR_H

#include "qmi8658.h"
#include <functional>
#include <memory>

enum class MotionEvent {
    NONE,
    FREE_FALL,      // 自由落体
    SHAKE_VIOLENTLY,// 剧烈摇晃
    FLIP,           // 设备被快速翻转
    SHAKE,          // 设备被摇晃
    PICKUP,         // 设备被拿起
    UPSIDE_DOWN     // 设备被倒置（持续状态）
};

class MotionDetector {
public:
    using EventCallback = std::function<void(MotionEvent, const ImuData&)>;

    MotionDetector(Qmi8658* imu);
    void SetEventCallback(EventCallback callback);
    void Process();  // 在主循环中调用
    
    // 获取当前IMU数据
    const ImuData& GetCurrentData() const { return current_data_; }
    
    // 获取当前拿起状态
    bool IsPickedUp() const { return is_picked_up_; }
    
    // 启用/禁用调试输出
    void SetDebugOutput(bool enable) { debug_output_ = enable; }

private:
    Qmi8658* imu_;
    EventCallback callback_;
    
    // 运动检测状态
    ImuData current_data_;
    ImuData last_data_;
    bool first_reading_ = true;
    int64_t last_event_time_us_ = 0;
    int64_t last_debug_time_us_ = 0;
    bool debug_output_ = false;
    
    // 自由落体检测的状态跟踪
    int64_t free_fall_start_time_ = 0;  // 自由落体开始时间
    bool in_free_fall_ = false;         // 是否处于自由落体状态
    
    // 倒置检测的状态跟踪
    bool is_upside_down_ = false;       // 当前是否倒置
    int upside_down_count_ = 0;         // 倒置状态持续计数
    
    // 拿起检测的状态跟踪
    bool is_picked_up_ = false;         // 当前是否处于拿起状态
    int stable_count_ = 0;              // 稳定状态计数（用于判断是否放下）
    float stable_z_reference_ = 1.0f;   // 稳定时的Z轴参考值
    
    // 运动检测阈值
    static constexpr float FREE_FALL_THRESHOLD_G = 0.3f;   // 自由落体阈值（总加速度接近0）
    static constexpr int64_t FREE_FALL_MIN_TIME_US = 200000; // 最小自由落体时间200ms
    static constexpr float SHAKE_VIOLENTLY_THRESHOLD_G = 3.0f; // 剧烈摇晃阈值
    static constexpr float SHAKE_THRESHOLD_G = 1.5f;       // 摇晃检测阈值
    static constexpr float FLIP_THRESHOLD_DEG_S = 400.0f;  // 翻转检测阈值（提高到400°/s，减少误触发）
    static constexpr float PICKUP_THRESHOLD_G = 0.15f;     // 拿起检测阈值（降低到0.15g，提高灵敏度）
    static constexpr float UPSIDE_DOWN_THRESHOLD_G = -0.8f; // 倒置检测阈值（Z轴接近-1g）
    static constexpr int UPSIDE_DOWN_STABLE_COUNT = 10;    // 倒置需要持续10帧（500ms）
    static constexpr int64_t DEBOUNCE_TIME_US = 300000;    // 300ms去抖
    static constexpr int64_t DEBUG_INTERVAL_US = 1000000;  // 1秒调试输出间隔

    bool DetectFreeFall(const ImuData& data, int64_t current_time);
    bool DetectShakeViolently(const ImuData& data);
    bool DetectFlip(const ImuData& data);
    bool DetectShake(const ImuData& data);
    bool DetectPickup(const ImuData& data);
    bool DetectUpsideDown(const ImuData& data);
    float CalculateAccelMagnitude(const ImuData& data);
    float CalculateAccelDelta(const ImuData& current, const ImuData& last);
};

#endif // ALICHUANGTEST_MOTION_DETECTOR_H