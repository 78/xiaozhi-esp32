#include "touch_engine.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/touch_pad.h>
#include <soc/touch_sensor_channel.h>

#define TAG "TouchEngine"

// ESP32-S3: GPIO1-14支持触摸
// 我们使用GPIO10和GPIO11

TouchEngine::TouchEngine() 
    : enabled_(false)
    , left_touched_(false)
    , right_touched_(false)
    , left_baseline_(0)
    , right_baseline_(0)
    , left_threshold_(0)
    , right_threshold_(0)
    , task_handle_(nullptr)
    , both_touch_start_time_(0)
    , cradled_triggered_(false) {
    
    // 初始化触摸状态
    left_state_ = {false, false, 0, 0, false};
    right_state_ = {false, false, 0, 0, false};
}

TouchEngine::~TouchEngine() {
    // 停止任务
    if (task_handle_) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
    
    // 反初始化触摸传感器
    touch_pad_deinit();
}

void TouchEngine::Initialize() {
    ESP_LOGI(TAG, "Initializing ESP32-S3 touch engine with denoise");
    
    // 1. 初始化触摸传感器驱动
    esp_err_t ret = touch_pad_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch pad init failed: %s", esp_err_to_name(ret));
        return;
    }
    
    // 2. 配置触摸传感器
    InitializeGPIO();
    
    // 3. 创建触摸处理任务
    xTaskCreate(TouchTask, "touch_task", 3072, this, 10, &task_handle_);
    
    enabled_ = true;
    ESP_LOGI(TAG, "Touch engine initialized - GPIO10 (LEFT), GPIO11 (RIGHT)");
}

void TouchEngine::InitializeGPIO() {
    // 1. 配置触摸通道
    esp_err_t ret = touch_pad_config(TOUCH_PAD_NUM10);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config touch pad 10: %s", esp_err_to_name(ret));
        return;
    }
    
    ret = touch_pad_config(TOUCH_PAD_NUM11);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config touch pad 11: %s", esp_err_to_name(ret));
        return;
    }
    
    // 2. 配置去噪功能（重要！防止误触发）
    touch_pad_denoise_t denoise = {
        .grade = TOUCH_PAD_DENOISE_BIT4,      // 去噪级别
        .cap_level = TOUCH_PAD_DENOISE_CAP_L4, // 电容级别
    };
    touch_pad_denoise_set_config(&denoise);
    touch_pad_denoise_enable();
    ESP_LOGI(TAG, "Denoise function enabled");
    
    // 3. 使用TIMER模式自动触发测量
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_fsm_start();
    
    // 4. 等待稳定后读取基准值
    vTaskDelay(pdMS_TO_TICKS(100));
    ReadBaseline();
}

void TouchEngine::ReadBaseline() {
    uint32_t sum10 = 0, sum11 = 0;
    const int samples = 30;  // 增加采样次数
    
    // 读取多次建立稳定基准
    for(int i = 0; i < samples; i++) {
        uint32_t val10 = 0, val11 = 0;
        
        // TIMER模式下自动测量，直接读取
        esp_err_t ret10 = touch_pad_read_raw_data(TOUCH_PAD_NUM10, &val10);
        if (ret10 == ESP_OK) {
            sum10 += val10;
        } else if (i == 0) {
            ESP_LOGE(TAG, "Failed to read TOUCH_PAD_NUM10: %s", esp_err_to_name(ret10));
        }
        
        esp_err_t ret11 = touch_pad_read_raw_data(TOUCH_PAD_NUM11, &val11);
        if (ret11 == ESP_OK) {
            sum11 += val11;
        } else if (i == 0) {
            ESP_LOGE(TAG, "Failed to read TOUCH_PAD_NUM11: %s", esp_err_to_name(ret11));
        }
        
        // 首次读取时显示原始值
        if (i == 0) {
            ESP_LOGI(TAG, "Initial raw values - Touch10: %ld, Touch11: %ld", val10, val11);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // 计算平均值作为基准
    left_baseline_ = sum10 / samples;
    right_baseline_ = sum11 / samples;
    
    // 设置更保守的阈值，减少误触发
    // ESP32-S3触摸时数值通常会减少
    left_threshold_ = left_baseline_ * 0.6;   // 60%的基准值（更大的变化才触发）
    right_threshold_ = right_baseline_ * 0.6;
    
    ESP_LOGI(TAG, "Touch baselines - Left: %ld (thr: %ld), Right: %ld (thr: %ld)", 
             left_baseline_, left_threshold_, right_baseline_, right_threshold_);
}

void TouchEngine::RegisterCallback(TouchEventCallback callback) {
    callbacks_.push_back(callback);
}

void TouchEngine::TouchTask(void* param) {
    TouchEngine* engine = static_cast<TouchEngine*>(param);
    
    while (true) {
        if (engine->enabled_) {
            engine->Process();
        }
        
        // 20ms轮询间隔，提高响应速度
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void TouchEngine::Process() {
    // TIMER模式下自动测量，直接读取值
    uint32_t left_value = 0, right_value = 0;
    esp_err_t ret1 = touch_pad_read_raw_data(TOUCH_PAD_NUM10, &left_value);
    esp_err_t ret2 = touch_pad_read_raw_data(TOUCH_PAD_NUM11, &right_value);
    
    if (ret1 != ESP_OK || ret2 != ESP_OK) {
        return;  // 读取失败，跳过这次
    }
    
    // 调试输出（每10秒一次，减少干扰）
    static int64_t last_debug_time = 0;
    int64_t current_time = esp_timer_get_time();
    if (current_time - last_debug_time > 10000000) {  // 10秒
        ESP_LOGD(TAG, "Touch values - L: %ld/%ld (%.1f%%), R: %ld/%ld (%.1f%%)", 
                left_value, left_baseline_, 
                left_baseline_ > 0 ? (float)left_value/left_baseline_*100 : 0,
                right_value, right_baseline_,
                right_baseline_ > 0 ? (float)right_value/right_baseline_*100 : 0);
        last_debug_time = current_time;
    }
    
    // 简化检测逻辑 - ESP32-S3触摸时数值减少
    // 添加死区防止噪声触发
    bool left_touched = false;
    bool right_touched = false;
    
    // 只有当变化足够大时才认为是触摸
    if (left_baseline_ > 0) {
        float left_ratio = (float)left_value / left_baseline_;
        left_touched = (left_ratio < 0.6);  // 值下降到60%以下才认为触摸
    }
    
    if (right_baseline_ > 0) {
        float right_ratio = (float)right_value / right_baseline_;
        right_touched = (right_ratio < 0.6);  // 值下降到60%以下才认为触摸
    }
    
    // 处理单侧触摸事件
    ProcessSingleTouch(left_touched, TouchPosition::LEFT, left_state_);
    ProcessSingleTouch(right_touched, TouchPosition::RIGHT, right_state_);
    
    // 处理特殊事件（cradled, tickled）
    ProcessSpecialEvents();
    
    // 更新全局状态
    left_touched_ = left_touched;
    right_touched_ = right_touched;
}

void TouchEngine::ProcessSingleTouch(bool currently_touched, TouchPosition position, TouchState& state) {
    int64_t current_time = esp_timer_get_time();
    
    // 消抖处理
    if (currently_touched != state.was_touched) {
        if ((current_time - state.last_change_time) < (DEBOUNCE_TIME_MS * 1000)) {
            return;  // 忽略抖动
        }
        state.last_change_time = current_time;
    }
    
    // 状态转换处理
    if (currently_touched && !state.is_touched) {
        // 按下事件
        state.is_touched = true;
        state.touch_start_time = current_time;
        state.event_triggered = false;
        
        // 记录触摸时间用于tickled检测
        tickle_detector_.touch_times.push_back(current_time);
        
    } else if (state.is_touched && !currently_touched) {
        // 释放事件
        uint32_t duration_ms = (current_time - state.touch_start_time) / 1000;
        
        if (!state.event_triggered && duration_ms < TAP_MAX_DURATION_MS) {
            // 触发单击事件
            TouchEvent event;
            event.type = TouchEventType::SINGLE_TAP;
            event.position = position;
            event.timestamp_us = current_time;
            event.duration_ms = duration_ms;
            
            DispatchEvent(event);
            
            ESP_LOGI(TAG, "SINGLE_TAP on %s (duration: %ld ms)", 
                    position == TouchPosition::LEFT ? "LEFT" : "RIGHT", duration_ms);
        }
        
        state.is_touched = false;
        state.event_triggered = false;
    }
    
    state.was_touched = currently_touched;
}

void TouchEngine::ProcessSpecialEvents() {
    int64_t current_time = esp_timer_get_time();
    
    // 1. 检测cradled事件（双侧持续触摸>2秒且IMU静止）
    if (left_touched_ && right_touched_) {
        if (both_touch_start_time_ == 0) {
            both_touch_start_time_ = current_time;
            cradled_triggered_ = false;
        } else {
            uint32_t duration_ms = (current_time - both_touch_start_time_) / 1000;
            if (!cradled_triggered_ && duration_ms >= CRADLED_MIN_DURATION_MS) {
                // 检查IMU是否稳定
                if (IsIMUStable()) {
                    cradled_triggered_ = true;
                    
                    TouchEvent event;
                    event.type = TouchEventType::CRADLED;
                    event.position = TouchPosition::BOTH;
                    event.timestamp_us = current_time;
                    event.duration_ms = duration_ms;
                    
                    DispatchEvent(event);
                    ESP_LOGI(TAG, "CRADLED detected (both sides held for %ld ms with stable IMU)", duration_ms);
                }
            }
        }
    } else {
        both_touch_start_time_ = 0;
        cradled_triggered_ = false;
    }
    
    // 2. 检测tickled事件（2秒内多次无规律触摸>4次）
    // 清理过时的触摸记录
    auto& times = tickle_detector_.touch_times;
    times.erase(
        std::remove_if(times.begin(), times.end(),
            [current_time](int64_t t) { 
                return (current_time - t) > (TICKLED_WINDOW_MS * 1000); 
            }),
        times.end()
    );
    
    // 检查是否达到tickled条件
    if (times.size() >= TICKLED_MIN_TOUCHES) {
        TouchEvent event;
        event.type = TouchEventType::TICKLED;
        event.position = TouchPosition::ANY;
        event.timestamp_us = current_time;
        event.duration_ms = 0;
        
        DispatchEvent(event);
        ESP_LOGI(TAG, "TICKLED detected (%zu touches in 2 seconds)", times.size());
        
        // 清空记录，避免重复触发
        times.clear();
    }
}

bool TouchEngine::IsIMUStable() {
    // TODO: 需要从MotionEngine获取IMU稳定状态
    // 暂时返回true，后续集成MotionEngine时实现
    return true;
}


void TouchEngine::DispatchEvent(const TouchEvent& event) {
    for (const auto& callback : callbacks_) {
        if (callback) {
            callback(event);
        }
    }
}