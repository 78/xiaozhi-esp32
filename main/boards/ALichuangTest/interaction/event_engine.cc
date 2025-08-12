#include "event_engine.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <cmath>
#include <algorithm>

#define TAG "EventEngine"

EventEngine::EventEngine() 
    : imu_(nullptr)
    , motion_detection_enabled_(false)
    , first_reading_(true)
    , last_event_time_us_(0)
    , last_debug_time_us_(0)
    , debug_output_(false)
    , free_fall_start_time_(0)
    , in_free_fall_(false)
    , is_upside_down_(false)
    , upside_down_count_(0)
    , is_picked_up_(false)
    , stable_count_(0)
    , stable_z_reference_(1.0f) {
}

EventEngine::~EventEngine() {
}

void EventEngine::Initialize(Qmi8658* imu) {
    imu_ = imu;
    if (imu_) {
        motion_detection_enabled_ = true;
        ESP_LOGI(TAG, "Event engine initialized with IMU support");
    } else {
        ESP_LOGI(TAG, "Event engine initialized without IMU");
    }
}

void EventEngine::RegisterCallback(EventCallback callback) {
    global_callback_ = callback;
}

void EventEngine::RegisterCallback(EventType type, EventCallback callback) {
    type_callbacks_.push_back({type, callback});
}

void EventEngine::Process() {
    // 处理运动检测
    if (motion_detection_enabled_ && imu_) {
        ProcessMotionDetection();
    }
    
    // 未来可以在这里添加其他事件源的处理
    // ProcessTouchEvents();
    // ProcessAudioEvents();
}

void EventEngine::ProcessMotionDetection() {
    // 读取IMU数据和角度
    if (imu_->ReadDataWithAngles(current_imu_data_) != ESP_OK) {
        return;
    }
    
    // 第一次读取，初始化last_data_
    if (first_reading_) {
        last_imu_data_ = current_imu_data_;
        first_reading_ = false;
        return;
    }
    
    int64_t current_time = esp_timer_get_time();
    
    // 检查去抖时间
    if (current_time - last_event_time_us_ < DEBOUNCE_TIME_US) {
        last_imu_data_ = current_imu_data_;
        return;
    }
    
    // 检测各种运动事件
    EventType motion_type = EventType::MOTION_NONE;
    
    // 按优先级检测运动事件（优先级从高到低）
    // 1. 自由落体（最危险，需要立即检测）
    if (DetectFreeFall(current_imu_data_, current_time)) {
        motion_type = EventType::MOTION_FREE_FALL;
        ESP_LOGW(TAG, "Motion detected: FREE_FALL! Duration: %lld ms | Magnitude: %.3f g", 
                (current_time - free_fall_start_time_) / 1000, CalculateAccelMagnitude(current_imu_data_));
    } 
    // 2. 剧烈摇晃（可能损坏设备）
    else if (DetectShakeViolently(current_imu_data_)) {
        motion_type = EventType::MOTION_SHAKE_VIOLENTLY;
        float accel_delta = CalculateAccelDelta(current_imu_data_, last_imu_data_);
        ESP_LOGW(TAG, "Motion detected: SHAKE_VIOLENTLY! AccelDelta: %.2f g", accel_delta);
    } 
    // 3. 翻转（快速旋转）
    else if (DetectFlip(current_imu_data_)) {
        motion_type = EventType::MOTION_FLIP;
        float gyro_mag = std::sqrt(current_imu_data_.gyro_x * current_imu_data_.gyro_x + 
                                  current_imu_data_.gyro_y * current_imu_data_.gyro_y + 
                                  current_imu_data_.gyro_z * current_imu_data_.gyro_z);
        ESP_LOGI(TAG, "Motion detected: FLIP | Gyro: %.1f deg/s (X:%.1f Y:%.1f Z:%.1f)", 
                gyro_mag, current_imu_data_.gyro_x, current_imu_data_.gyro_y, current_imu_data_.gyro_z);
    } 
    // 4. 普通摇晃
    else if (DetectShake(current_imu_data_)) {
        motion_type = EventType::MOTION_SHAKE;
        float accel_delta = CalculateAccelDelta(current_imu_data_, last_imu_data_);
        ESP_LOGI(TAG, "Motion detected: SHAKE | AccelDelta: %.2f g", accel_delta);
    } 
    // 5. 拿起
    else if (DetectPickup(current_imu_data_)) {
        motion_type = EventType::MOTION_PICKUP;
        float z_diff = current_imu_data_.accel_z - last_imu_data_.accel_z;
        ESP_LOGI(TAG, "Motion detected: PICKUP | Z-diff: %.3f g, Current Z: %.2f g (State: picked up)", 
                z_diff, current_imu_data_.accel_z);
    }
    // 6. 倒置状态（持续状态检测）
    else if (DetectUpsideDown(current_imu_data_)) {
        motion_type = EventType::MOTION_UPSIDE_DOWN;
        ESP_LOGI(TAG, "Motion detected: UPSIDE_DOWN | Z-axis: %.2f g, Count: %d", 
                current_imu_data_.accel_z, upside_down_count_);
    }
    
    if (motion_type != EventType::MOTION_NONE) {
        last_event_time_us_ = current_time;
        
        // 创建事件并分发
        Event event;
        event.type = motion_type;
        event.timestamp_us = current_time;
        event.data.imu_data = current_imu_data_;
        
        DispatchEvent(event);
    }
    
    last_imu_data_ = current_imu_data_;
}

void EventEngine::TriggerEvent(const Event& event) {
    DispatchEvent(event);
}

void EventEngine::TriggerEvent(EventType type) {
    Event event;
    event.type = type;
    event.timestamp_us = esp_timer_get_time();
    DispatchEvent(event);
}

void EventEngine::DispatchEvent(const Event& event) {
    // 调用全局回调
    if (global_callback_) {
        global_callback_(event);
    }
    
    // 调用特定类型的回调
    for (const auto& pair : type_callbacks_) {
        if (pair.first == event.type) {
            pair.second(event);
        }
    }
}

float EventEngine::CalculateAccelMagnitude(const ImuData& data) {
    return std::sqrt(data.accel_x * data.accel_x + 
                    data.accel_y * data.accel_y + 
                    data.accel_z * data.accel_z);
}

float EventEngine::CalculateAccelDelta(const ImuData& current, const ImuData& last) {
    float dx = current.accel_x - last.accel_x;
    float dy = current.accel_y - last.accel_y;
    float dz = current.accel_z - last.accel_z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool EventEngine::DetectPickup(const ImuData& data) {
    // 状态式拿起检测：区分拿起和放下，避免重复触发
    
    // 计算运动参数
    float z_diff = data.accel_z - last_imu_data_.accel_z;
    float current_magnitude = CalculateAccelMagnitude(data);
    float accel_delta = CalculateAccelDelta(data, last_imu_data_);
    
    // 检测设备是否处于稳定状态
    bool is_stable = (accel_delta < 0.1f) && (std::abs(data.gyro_x) < 20.0f) && 
                     (std::abs(data.gyro_y) < 20.0f) && (std::abs(data.gyro_z) < 20.0f);
    
    if (!is_picked_up_) {
        // 当前未拿起状态，检测是否被拿起
        
        // 更新稳定时的Z轴参考值（设备平放时约为1g）
        if (is_stable) {
            stable_z_reference_ = data.accel_z;
            stable_count_++;
        } else {
            stable_count_ = 0;
        }
        
        // 检测向上的运动（拿起动作）
        bool upward_motion = z_diff > PICKUP_THRESHOLD_G;  // Z轴正向加速
        bool magnitude_increase = (current_magnitude - CalculateAccelMagnitude(last_imu_data_)) > PICKUP_THRESHOLD_G;
        
        // 排除向下运动（放置动作）
        bool downward_motion = z_diff < -PICKUP_THRESHOLD_G;
        
        if ((upward_motion || magnitude_increase) && !downward_motion) {
            // 检测到拿起动作
            is_picked_up_ = true;
            stable_count_ = 0;
            
            if (debug_output_) {
                ESP_LOGD(TAG, "Pickup started - Z_diff:%.3f Mag:%.3f", z_diff, current_magnitude);
            }
            return true;
        }
        
    } else {
        // 当前已拿起状态，检测是否被放下
        
        if (is_stable) {
            stable_count_++;
            
            // 需要持续稳定一段时间，且Z轴回到接近初始值
            if (stable_count_ >= 20) {  // 约1秒稳定
                // 检查Z轴是否回到水平位置附近（0.8g ~ 1.2g）
                if (std::abs(data.accel_z) > 0.8f && std::abs(data.accel_z) < 1.2f) {
                    is_picked_up_ = false;
                    stable_count_ = 0;
                    
                    if (debug_output_) {
                        ESP_LOGD(TAG, "Device put down - Z:%.3f stable for %d frames", 
                                data.accel_z, stable_count_);
                    }
                }
            }
        } else {
            stable_count_ = 0;
        }
        
        // 已经在拿起状态，不再触发新的拿起事件
        return false;
    }
    
    return false;
}

bool EventEngine::DetectUpsideDown(const ImuData& data) {
    // 倒置检测：Z轴持续接近-1g（设备倒置）
    // 需要稳定且不在剧烈运动中
    
    float accel_delta = CalculateAccelDelta(data, last_imu_data_);
    bool is_stable = accel_delta < 0.5f;  // 相对稳定，没有剧烈运动
    
    // 检查Z轴是否接近-1g（倒置）
    bool z_axis_inverted = data.accel_z < UPSIDE_DOWN_THRESHOLD_G;
    
    if (z_axis_inverted && is_stable) {
        upside_down_count_++;
        
        // 需要持续一定帧数才判定为倒置
        if (!is_upside_down_ && upside_down_count_ >= UPSIDE_DOWN_STABLE_COUNT) {
            is_upside_down_ = true;
            ESP_LOGD(TAG, "Device is now upside down: Z=%.2f g", data.accel_z);
            return true;
        }
    } else {
        // 不再倒置
        if (is_upside_down_ && !z_axis_inverted) {
            ESP_LOGD(TAG, "Device is no longer upside down: Z=%.2f g", data.accel_z);
            is_upside_down_ = false;
        }
        upside_down_count_ = 0;
    }
    
    return false;
}

bool EventEngine::DetectShake(const ImuData& data) {
    // 检测快速来回运动
    float accel_delta = CalculateAccelDelta(data, last_imu_data_);
    return accel_delta > SHAKE_THRESHOLD_G;
}

bool EventEngine::DetectFreeFall(const ImuData& data, int64_t current_time) {
    // 自由落体检测：总加速度接近0g，持续超过200ms
    float magnitude = CalculateAccelMagnitude(data);
    
    // 检测是否处于自由落体状态（总加速度小于阈值）
    bool is_falling = magnitude < FREE_FALL_THRESHOLD_G;
    
    if (is_falling) {
        if (!in_free_fall_) {
            // 刚开始自由落体
            in_free_fall_ = true;
            free_fall_start_time_ = current_time;
            ESP_LOGD(TAG, "Free fall started: magnitude=%.3f g", magnitude);
        } else {
            // 检查是否持续足够长时间
            int64_t fall_duration = current_time - free_fall_start_time_;
            if (fall_duration >= FREE_FALL_MIN_TIME_US) {
                ESP_LOGD(TAG, "Free fall confirmed: duration=%lld ms, magnitude=%.3f g", 
                        fall_duration / 1000, magnitude);
                return true;
            }
        }
    } else {
        if (in_free_fall_) {
            // 自由落体结束
            int64_t fall_duration = current_time - free_fall_start_time_;
            ESP_LOGD(TAG, "Free fall ended: duration=%lld ms", fall_duration / 1000);
            in_free_fall_ = false;
        }
    }
    
    return false;
}

bool EventEngine::DetectShakeViolently(const ImuData& data) {
    // 剧烈摇晃检测：加速度变化超过3g
    float accel_delta = CalculateAccelDelta(data, last_imu_data_);
    
    // 同时检查陀螺仪是否有剧烈旋转
    float gyro_magnitude = std::sqrt(data.gyro_x * data.gyro_x + 
                                   data.gyro_y * data.gyro_y + 
                                   data.gyro_z * data.gyro_z);
    
    // 剧烈摇晃：大幅度加速度变化 或 高速旋转伴随加速度变化
    bool violent_shake = (accel_delta > SHAKE_VIOLENTLY_THRESHOLD_G) || 
                        (accel_delta > 2.0f && gyro_magnitude > 300.0f);
    
    if (violent_shake) {
        ESP_LOGD(TAG, "Violent shake: accel_delta=%.2f g, gyro=%.1f deg/s", 
                accel_delta, gyro_magnitude);
    }
    
    return violent_shake;
}

bool EventEngine::DetectFlip(const ImuData& data) {
    // 改进的翻转检测：需要持续高速旋转，避免误触发
    float gyro_magnitude = std::sqrt(data.gyro_x * data.gyro_x + 
                                   data.gyro_y * data.gyro_y + 
                                   data.gyro_z * data.gyro_z);
    
    // 检查是否有主导轴的高速旋转（真正的翻转通常在某个轴上）
    float max_single_axis = std::max({std::abs(data.gyro_x), 
                                      std::abs(data.gyro_y), 
                                      std::abs(data.gyro_z)});
    
    // 需要同时满足：
    // 1. 总旋转速度超过阈值
    // 2. 至少有一个轴的旋转速度超过阈值的70%（避免轻微晃动的累加效应）
    bool high_rotation = gyro_magnitude > FLIP_THRESHOLD_DEG_S;
    bool dominant_axis = max_single_axis > (FLIP_THRESHOLD_DEG_S * 0.7f);
    
    // 3. 加速度也要有明显变化（真正翻转时会有）
    float accel_change = CalculateAccelDelta(data, last_imu_data_);
    bool accel_detected = accel_change > 0.5f;  // 至少0.5g的加速度变化
    
    bool flip_detected = high_rotation && dominant_axis && accel_detected;
    
    if (flip_detected && debug_output_) {
        ESP_LOGD(TAG, "Flip details - Gyro:%.1f MaxAxis:%.1f AccelDelta:%.2f", 
                gyro_magnitude, max_single_axis, accel_change);
    }
    
    return flip_detected;
}