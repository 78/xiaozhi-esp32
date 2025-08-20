#include "touch_engine.h"
#include "touch_config.h"
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
    , stuck_detection_count_(0)
    , both_touch_start_time_(0)
    , cradled_triggered_(false)
    , task_handle_(nullptr) {
    
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
    
    // 1. 加载配置（使用默认路径）
    LoadConfiguration();
    
    // 2. 初始化触摸传感器驱动
    esp_err_t ret = touch_pad_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch pad init failed: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Touch pad driver initialized successfully");
    
    // 3. 配置触摸传感器
    InitializeGPIO();
    
    // 4. 创建触摸处理任务
    BaseType_t task_result = xTaskCreate(TouchTask, "touch_task", 3072, this, 10, &task_handle_);
    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create touch task");
        return;
    }
    
    enabled_ = true;
    ESP_LOGI(TAG, "Touch engine initialized - GPIO10 (LEFT), GPIO11 (RIGHT), task handle: %p", task_handle_);
}

void TouchEngine::LoadConfiguration(const char* config_path) {
    // 如果没有指定路径，使用默认路径
    const char* path = config_path ? config_path : "/spiffs/event_config.json";
    
    // 尝试从文件加载配置
    if (!TouchConfigLoader::LoadFromFile(path, config_)) {
        // 如果失败，使用默认配置
        config_ = TouchConfigLoader::LoadDefaults();
    }
    
    ESP_LOGI(TAG, "Touch detection configuration loaded:");
    ESP_LOGI(TAG, "  tap_max: %ldms, hold_min: %ldms, debounce: %ldms",
            config_.tap_max_duration_ms, 
            config_.hold_min_duration_ms,
            config_.debounce_time_ms);
    ESP_LOGI(TAG, "  threshold_ratio: %.1f", config_.touch_threshold_ratio);
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
        
            // 首次和最后一次读取时显示原始值
        if (i == 0 || i == samples - 1) {
            ESP_LOGI(TAG, "Sample %d raw values - Touch10: %ld, Touch11: %ld", i, val10, val11);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // 计算平均值作为基准
    left_baseline_ = sum10 / samples;
    right_baseline_ = sum11 / samples;
    
    // 设置更保守的阈值，减少误触发
    // ESP32-S3触摸时数值会增加（不是减少）
    left_threshold_ = left_baseline_ * config_.touch_threshold_ratio;
    right_threshold_ = right_baseline_ * config_.touch_threshold_ratio;
    
    ESP_LOGI(TAG, "Touch baselines - Left: %ld (thr: %ld), Right: %ld (thr: %ld)", 
             left_baseline_, left_threshold_, right_baseline_, right_threshold_);
    ESP_LOGI(TAG, "Touch detection: values must increase to 150%% of baseline");
}

void TouchEngine::ResetTouchSensor() {
    ESP_LOGW(TAG, "========== TOUCH SENSOR RESET START ==========");
    
    // 1. 清除内部状态
    left_state_ = {false, false, 0, 0, false};
    right_state_ = {false, false, 0, 0, false};
    left_touched_ = false;
    right_touched_ = false;
    both_touch_start_time_ = 0;
    cradled_triggered_ = false;
    
    ESP_LOGI(TAG, "Step 1: Internal state cleared");
    
    // 2. 停止FSM
    touch_pad_fsm_stop();
    vTaskDelay(pdMS_TO_TICKS(50));  // 增加延迟
    ESP_LOGI(TAG, "Step 2: FSM stopped");
    
    // 3. 反初始化
    touch_pad_deinit();
    vTaskDelay(pdMS_TO_TICKS(100));  // 增加延迟
    ESP_LOGI(TAG, "Step 3: Touch pad deinitialized");
    
    // 4. 重新初始化
    esp_err_t ret = touch_pad_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch sensor reset failed at init: %s", esp_err_to_name(ret));
        // 尝试第二次
        vTaskDelay(pdMS_TO_TICKS(500));
        ret = touch_pad_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Touch sensor reset failed again: %s", esp_err_to_name(ret));
            return;
        }
    }
    ESP_LOGI(TAG, "Step 4: Touch pad reinitialized");
    
    // 5. 重新配置
    InitializeGPIO();
    ESP_LOGI(TAG, "Step 5: GPIO reconfigured");
    
    // 6. 等待稳定并重新读取基线
    vTaskDelay(pdMS_TO_TICKS(200));
    ReadBaseline();
    
    ESP_LOGI(TAG, "========== TOUCH SENSOR RESET COMPLETE ==========");
    ESP_LOGI(TAG, "New baselines - L: %ld, R: %ld", left_baseline_, right_baseline_);
    
    // 7. 测试读取
    uint32_t test_left = 0, test_right = 0;
    esp_err_t test1 = touch_pad_read_raw_data(TOUCH_PAD_NUM10, &test_left);
    esp_err_t test2 = touch_pad_read_raw_data(TOUCH_PAD_NUM11, &test_right);
    ESP_LOGI(TAG, "Test read - L: %ld (err: %s), R: %ld (err: %s)", 
             test_left, esp_err_to_name(test1), test_right, esp_err_to_name(test2));
}

void TouchEngine::RegisterCallback(TouchEventCallback callback) {
    callbacks_.push_back(callback);
}

void TouchEngine::TouchTask(void* param) {
    TouchEngine* engine = static_cast<TouchEngine*>(param);
    ESP_LOGI(TAG, "Touch task started");
    
    int counter = 0;
    while (true) {
        try {
            if (engine->enabled_) {
                engine->Process();
                
                // 每5秒输出一次任务运行状态
                if (++counter >= 250) {  // 250 * 20ms = 5s
                    ESP_LOGD(TAG, "Touch task running - baselines: L=%ld, R=%ld", 
                            engine->left_baseline_, engine->right_baseline_);
                    counter = 0;
                }
            }
        } catch (...) {
            ESP_LOGE(TAG, "Exception in touch task processing!");
            // 继续运行，不要让任务退出
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
        static int error_count = 0;
        if (++error_count <= 10) {  // 增加错误报告次数
            ESP_LOGE(TAG, "Failed to read touch values: ret1=%s, ret2=%s (count: %d)", 
                    esp_err_to_name(ret1), esp_err_to_name(ret2), error_count);
        }
        
        // 如果连续失败超过20次，尝试重新初始化
        if (error_count > 20 && error_count % 50 == 0) {
            ESP_LOGE(TAG, "Touch sensor persistent failure, attempting recovery...");
            // TODO: 实现触摸传感器重新初始化
        }
        return;  // 读取失败，跳过这次
    }
    
    // 简化检测逻辑 - ESP32-S3触摸时数值减少
    // 添加死区防止噪声触发
    bool left_touched = false;
    bool right_touched = false;
    
    // 每隔一段时间输出原始值以便调试
    static int debug_counter = 0;
    static uint32_t last_left_value = 0;
    static uint32_t last_right_value = 0;
    static int frozen_count = 0;
    
    if (++debug_counter >= 100) {  // 100 * 20ms = 2s
        ESP_LOGD(TAG, "Touch values - L: %ld (%.1f%%), R: %ld (%.1f%%)",
                left_value, (float)left_value/left_baseline_*100,
                right_value, (float)right_value/right_baseline_*100);
        
        // 检测数值是否冻结（硬件驱动卡死）
        if (left_value == last_left_value && right_value == last_right_value) {
            frozen_count++;
            if (frozen_count >= 3) {  // 连续3次（6秒）数值完全不变
                ESP_LOGE(TAG, "Touch sensor values frozen! Hardware driver may be stuck.");
                ESP_LOGE(TAG, "Attempting automatic recovery...");
                ResetTouchSensor();
                frozen_count = 0;
            }
        } else {
            frozen_count = 0;  // 数值有变化，重置计数
        }
        
        // 检测异常高值（可能是传感器卡死）
        bool sensor_stuck = false;
        if (left_baseline_ > 0 && (float)left_value/left_baseline_ > 3.0f) {
            ESP_LOGW(TAG, "Left sensor stuck at high value! Ratio: %.1f%%", (float)left_value/left_baseline_*100);
            sensor_stuck = true;
        }
        if (right_baseline_ > 0 && (float)right_value/right_baseline_ > 3.0f) {
            ESP_LOGW(TAG, "Right sensor stuck at high value! Ratio: %.1f%%", (float)right_value/right_baseline_*100);
            sensor_stuck = true;
        }
        
        // 卡死检测和恢复
        if (sensor_stuck) {
            stuck_detection_count_++;
            if (stuck_detection_count_ >= STUCK_THRESHOLD) {
                ESP_LOGE(TAG, "Touch sensor persistently stuck, attempting reset...");
                ResetTouchSensor();
                stuck_detection_count_ = 0;
            }
        } else {
            stuck_detection_count_ = 0; // 重置计数器
        }
        
        // 更新上次的值
        last_left_value = left_value;
        last_right_value = right_value;
        debug_counter = 0;
    }
    
    // 只有当变化足够大时才认为是触摸
    // ESP32-S3触摸时数值增加（不是减少）
    if (left_baseline_ > 0) {
        float left_ratio = (float)left_value / left_baseline_;
        left_touched = (left_ratio > config_.touch_threshold_ratio);  // 值增加到配置的比例以上才认为触摸
        
        // 只在状态改变时输出日志
        static bool last_left_touched = false;
        if (left_touched != last_left_touched) {
            ESP_LOGI(TAG, "Left touch %s - value: %ld, baseline: %ld, ratio: %.1f%% (cradled: %d)", 
                    left_touched ? "DETECTED" : "RELEASED",
                    left_value, left_baseline_, left_ratio * 100, cradled_triggered_);
            last_left_touched = left_touched;
        }
        
    } else {
        ESP_LOGW(TAG, "Left baseline is 0, cannot detect touch");
    }
    
    if (right_baseline_ > 0) {
        float right_ratio = (float)right_value / right_baseline_;
        right_touched = (right_ratio > config_.touch_threshold_ratio);  // 值增加到配置的比例以上才认为触摸
        
        // 只在状态改变时输出日志
        static bool last_right_touched = false;
        if (right_touched != last_right_touched) {
            ESP_LOGI(TAG, "Right touch %s - value: %ld, baseline: %ld, ratio: %.1f%% (cradled: %d)", 
                    right_touched ? "DETECTED" : "RELEASED",
                    right_value, right_baseline_, right_ratio * 100, cradled_triggered_);
            last_right_touched = right_touched;
        }
    }
    
    // 处理单侧触摸事件
    ProcessSingleTouch(left_touched, TouchPosition::LEFT, left_state_);
    ProcessSingleTouch(right_touched, TouchPosition::RIGHT, right_state_);
    
    // 更新全局状态（应该在处理特殊事件之前）
    left_touched_ = left_touched;
    right_touched_ = right_touched;
    
    // 处理特殊事件（cradled, tickled）
    ProcessSpecialEvents();
}

void TouchEngine::ProcessSingleTouch(bool currently_touched, TouchPosition position, TouchState& state) {
    int64_t current_time = esp_timer_get_time();
    
    
    // 消抖处理
    if (currently_touched != state.was_touched) {
        if ((current_time - state.last_change_time) < (config_.debounce_time_ms * 1000)) {
            // ESP_LOGD(TAG, "Debounce: ignoring change within %ldms", config_.debounce_time_ms);
            return;  // 忽略抖动
        }
        state.last_change_time = current_time;
    }
    
    // 状态转换处理
    if (currently_touched && !state.is_touched) {
        // 按下事件
        ESP_LOGI(TAG, "Touch PRESSED on %s", 
                position == TouchPosition::LEFT ? "LEFT" : "RIGHT");
        
        state.is_touched = true;
        state.touch_start_time = current_time;
        state.event_triggered = false;
        
        // 记录触摸时间用于tickled检测
        tickle_detector_.touch_times.push_back(current_time);
        
    } else if (state.is_touched && currently_touched) {
        // 持续按住状态 - 检查是否应该触发长按事件
        uint32_t duration_ms = (current_time - state.touch_start_time) / 1000;
        
        // 如果超过长按阈值且还没有触发过事件
        if (!state.event_triggered && duration_ms >= config_.hold_min_duration_ms) {
            // 触发长按事件
            TouchEvent event;
            event.type = TouchEventType::HOLD;
            event.position = position;
            event.timestamp_us = current_time;
            event.duration_ms = duration_ms;
            
            ESP_LOGI(TAG, "Creating HOLD event: type=%d, position=%d, duration=%ld ms", 
                    (int)event.type, (int)event.position, event.duration_ms);
            
            DispatchEvent(event);
            
            ESP_LOGI(TAG, "HOLD on %s dispatched (duration: %ld ms)", 
                    position == TouchPosition::LEFT ? "LEFT" : "RIGHT", duration_ms);
            
            state.event_triggered = true;  // 标记已触发，避免重复触发
        }
        
    } else if (state.is_touched && !currently_touched) {
        // 释放事件
        uint32_t duration_ms = (current_time - state.touch_start_time) / 1000;
        
        ESP_LOGI(TAG, "Touch RELEASED on %s: duration=%ldms, triggered=%d, TAP_MAX=%ld", 
                position == TouchPosition::LEFT ? "LEFT" : "RIGHT",
                duration_ms, state.event_triggered, config_.tap_max_duration_ms);
        
        if (!state.event_triggered && duration_ms < config_.tap_max_duration_ms) {
            // 触发单击事件（只有在没有触发长按且时间短于TAP_MAX时）
            TouchEvent event;
            event.type = TouchEventType::SINGLE_TAP;
            event.position = position;
            event.timestamp_us = current_time;
            event.duration_ms = duration_ms;
            
            ESP_LOGI(TAG, "Creating SINGLE_TAP event: type=%d, position=%d, duration=%ld ms", 
                    (int)event.type, (int)event.position, event.duration_ms);
            
            DispatchEvent(event);
            
            ESP_LOGI(TAG, "SINGLE_TAP on %s dispatched (duration: %ld ms)", 
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
            if (!cradled_triggered_ && duration_ms >= config_.cradled_min_duration_ms) {
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
        // 双侧触摸结束，重置cradled状态
        if (both_touch_start_time_ != 0 || cradled_triggered_) {
            ESP_LOGD(TAG, "Both touch ended - resetting cradled state (was_triggered=%d)", cradled_triggered_);
        }
        both_touch_start_time_ = 0;
        cradled_triggered_ = false;
    }
    
    // 2. 检测tickled事件（2秒内多次无规律触摸>4次）
    // 清理过时的触摸记录
    auto& times = tickle_detector_.touch_times;
    times.erase(
        std::remove_if(times.begin(), times.end(),
            [current_time, this](int64_t t) { 
                return (current_time - t) > (config_.tickled_window_ms * 1000); 
            }),
        times.end()
    );
    
    // 检查是否达到tickled条件
    if (times.size() >= config_.tickled_min_touches) {
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
    ESP_LOGI(TAG, "Dispatching TouchEvent: type=%d, position=%d, callbacks=%zu", 
            (int)event.type, (int)event.position, callbacks_.size());
    
    for (size_t i = 0; i < callbacks_.size(); ++i) {
        if (callbacks_[i]) {
            ESP_LOGD(TAG, "Calling callback %zu with event type=%d", i, (int)event.type);
            try {
                callbacks_[i](event);
                ESP_LOGD(TAG, "Callback %zu completed", i);
            } catch (...) {
                ESP_LOGE(TAG, "Exception in callback %zu for event type=%d", i, (int)event.type);
            }
        }
    }
    ESP_LOGD(TAG, "Event dispatch completed for type=%d", (int)event.type);
}