#include "motion_detector.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <cmath>

#define TAG "MotionDetector"

MotionDetector::MotionDetector(Qmi8658* imu) : imu_(imu) {
}

void MotionDetector::SetEventCallback(EventCallback callback) {
    callback_ = callback;
}

void MotionDetector::Process() {
    // 读取IMU数据和角度
    if (imu_->ReadDataWithAngles(current_data_) != ESP_OK) {
        return;
    }
    
    // 第一次读取，初始化last_data_
    if (first_reading_) {
        last_data_ = current_data_;
        first_reading_ = false;
        return;
    }
    
    int64_t current_time = esp_timer_get_time();
    
    // 定期输出调试信息
    if (debug_output_ && (current_time - last_debug_time_us_ >= DEBUG_INTERVAL_US)) {
        ESP_LOGI(TAG, "IMU Data - Accel(g): X=%.2f Y=%.2f Z=%.2f | Gyro(deg/s): X=%.1f Y=%.1f Z=%.1f",
                current_data_.accel_x, current_data_.accel_y, current_data_.accel_z,
                current_data_.gyro_x, current_data_.gyro_y, current_data_.gyro_z);
        ESP_LOGI(TAG, "Angles(deg): X=%.1f Y=%.1f Z=%.1f",
                current_data_.angle_x, current_data_.angle_y, current_data_.angle_z);
        last_debug_time_us_ = current_time;
    }
    
    // 检查去抖时间
    if (current_time - last_event_time_us_ < DEBOUNCE_TIME_US) {
        last_data_ = current_data_;
        return;
    }
    
    // 检测各种运动事件
    MotionEvent event = MotionEvent::NONE;
    
    // 按优先级检测运动事件（优先级从高到低）
    // 1. 自由落体（最危险，需要立即检测）
    if (DetectFreeFall(current_data_, current_time)) {
        event = MotionEvent::FREE_FALL;
        ESP_LOGW(TAG, "Motion detected: FREE_FALL! Duration: %lld ms", 
                (current_time - free_fall_start_time_) / 1000);
    } 
    // 2. 剧烈摇晃（可能损坏设备）
    else if (DetectShakeViolently(current_data_)) {
        event = MotionEvent::SHAKE_VIOLENTLY;
        ESP_LOGW(TAG, "Motion detected: SHAKE_VIOLENTLY!");
    } 
    // 3. 翻转（快速旋转）
    else if (DetectFlip(current_data_)) {
        event = MotionEvent::FLIP;
        ESP_LOGI(TAG, "Motion detected: FLIP");
    } 
    // 4. 普通摇晃
    else if (DetectShake(current_data_)) {
        event = MotionEvent::SHAKE;
        ESP_LOGI(TAG, "Motion detected: SHAKE");
    } 
    // 5. 拿起
    else if (DetectPickup(current_data_)) {
        event = MotionEvent::PICKUP;
        ESP_LOGI(TAG, "Motion detected: PICKUP");
    }
    // 6. 倒置状态（持续状态检测）
    else if (DetectUpsideDown(current_data_)) {
        event = MotionEvent::UPSIDE_DOWN;
        ESP_LOGI(TAG, "Motion detected: UPSIDE_DOWN (Z-axis: %.2f g)", current_data_.accel_z);
    }
    
    if (event != MotionEvent::NONE && callback_) {
        last_event_time_us_ = current_time;
        callback_(event, current_data_);
    }
    
    last_data_ = current_data_;
}

float MotionDetector::CalculateAccelMagnitude(const ImuData& data) {
    return std::sqrt(data.accel_x * data.accel_x + 
                    data.accel_y * data.accel_y + 
                    data.accel_z * data.accel_z);
}

float MotionDetector::CalculateAccelDelta(const ImuData& current, const ImuData& last) {
    float dx = current.accel_x - last.accel_x;
    float dy = current.accel_y - last.accel_y;
    float dz = current.accel_z - last.accel_z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool MotionDetector::DetectPickup(const ImuData& data) {
    // 检测Z轴向上的加速度变化（设备被拿起）
    float z_diff = data.accel_z - last_data_.accel_z;
    float magnitude_diff = CalculateAccelMagnitude(data) - CalculateAccelMagnitude(last_data_);
    
    return (z_diff > PICKUP_THRESHOLD_G) || (magnitude_diff > PICKUP_THRESHOLD_G);
}

bool MotionDetector::DetectUpsideDown(const ImuData& data) {
    // 倒置检测：Z轴持续接近-1g（设备倒置）
    // 需要稳定且不在剧烈运动中
    
    float accel_delta = CalculateAccelDelta(data, last_data_);
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

bool MotionDetector::DetectShake(const ImuData& data) {
    // 检测快速来回运动
    float accel_delta = CalculateAccelDelta(data, last_data_);
    return accel_delta > SHAKE_THRESHOLD_G;
}


bool MotionDetector::DetectFreeFall(const ImuData& data, int64_t current_time) {
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

bool MotionDetector::DetectShakeViolently(const ImuData& data) {
    // 剧烈摇晃检测：加速度变化超过3g
    float accel_delta = CalculateAccelDelta(data, last_data_);
    
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

bool MotionDetector::DetectFlip(const ImuData& data) {
    // 检测快速旋转
    float gyro_magnitude = std::sqrt(data.gyro_x * data.gyro_x + 
                                   data.gyro_y * data.gyro_y + 
                                   data.gyro_z * data.gyro_z);
    return gyro_magnitude > FLIP_THRESHOLD_DEG_S;
}