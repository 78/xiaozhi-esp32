#include "touch_engine.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/touch_sensor.h>
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
    ESP_LOGI(TAG, "Initializing ESP32-S3 touch engine (Simple version)");
    
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
    ESP_LOGI(TAG, "Touch engine initialized - GPIO10 (TOUCH10), GPIO11 (TOUCH11)");
}

void TouchEngine::InitializeGPIO() {
    ESP_LOGI(TAG, "Configuring touch sensor...");
    
    // 1. 基本配置 - 按照文档最简单的方式
    // 配置GPIO10为触摸通道
    esp_err_t ret = touch_pad_config(TOUCH_PAD_NUM10);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config touch pad 10: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Touch pad 10 configured");
    
    // 配置GPIO11为触摸通道
    ret = touch_pad_config(TOUCH_PAD_NUM11);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config touch pad 11: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Touch pad 11 configured");
    
    // 2. 设置测量时间参数（可选）
    touch_pad_set_charge_discharge_times(500);
    touch_pad_set_measurement_interval(0x1000);
    
    // 3. 设置电压（可选）
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    
    // 4. 设置FSM模式为软件控制（我们手动触发）
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_SW);
    
    // 5. 等待稳定
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 6. 读取初始值来确定是否工作
    uint32_t value10 = 0, value11 = 0;
    
    // 触发一次测量
    touch_pad_sw_start();
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // 读取值
    ret = touch_pad_read_raw_data(TOUCH_PAD_NUM10, &value10);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Initial Touch10 value: %ld", value10);
    } else {
        ESP_LOGE(TAG, "Failed to read Touch10: %s", esp_err_to_name(ret));
    }
    
    ret = touch_pad_read_raw_data(TOUCH_PAD_NUM11, &value11);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Initial Touch11 value: %ld", value11);
    } else {
        ESP_LOGE(TAG, "Failed to read Touch11: %s", esp_err_to_name(ret));
    }
    
    // 7. 多次读取来建立基准值
    ReadBaseline();
}

void TouchEngine::ReadBaseline() {
    ESP_LOGI(TAG, "Establishing baseline values...");
    
    uint32_t sum10 = 0, sum11 = 0;
    uint32_t min10 = UINT32_MAX, max10 = 0;
    uint32_t min11 = UINT32_MAX, max11 = 0;
    
    // 读取20次，找出范围
    for(int i = 0; i < 20; i++) {
        uint32_t val10, val11;
        
        // 触发测量
        touch_pad_sw_start();
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // 读取GPIO10
        if (touch_pad_read_raw_data(TOUCH_PAD_NUM10, &val10) == ESP_OK) {
            sum10 += val10;
            if (val10 < min10) min10 = val10;
            if (val10 > max10) max10 = val10;
            if (i % 5 == 0) {  // 每5次显示一次
                ESP_LOGI(TAG, "Touch10 reading %d: %ld", i, val10);
            }
        }
        
        // 读取GPIO11
        if (touch_pad_read_raw_data(TOUCH_PAD_NUM11, &val11) == ESP_OK) {
            sum11 += val11;
            if (val11 < min11) min11 = val11;
            if (val11 > max11) max11 = val11;
            if (i % 5 == 0) {  // 每5次显示一次
                ESP_LOGI(TAG, "Touch11 reading %d: %ld", i, val11);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    
    // 计算平均值作为基准
    left_baseline_ = sum10 / 20;
    right_baseline_ = sum11 / 20;
    
    ESP_LOGI(TAG, "Touch10 - Baseline: %ld, Range: [%ld-%ld]", 
             left_baseline_, min10, max10);
    ESP_LOGI(TAG, "Touch11 - Baseline: %ld, Range: [%ld-%ld]", 
             right_baseline_, min11, max11);
    
    // 设置阈值 - 先尝试两种可能：
    // 1. 如果触摸时数值减少（大多数ESP32）
    left_threshold_ = left_baseline_ * 0.8;   // 80%的基准值
    right_threshold_ = right_baseline_ * 0.8;
    
    // 2. 如果基准值很小（<1000），可能触摸时数值增加
    if (left_baseline_ < 1000) {
        left_threshold_ = left_baseline_ * 1.5;  // 150%的基准值
        ESP_LOGI(TAG, "Touch10 using increase mode, threshold: %ld", left_threshold_);
    } else {
        ESP_LOGI(TAG, "Touch10 using decrease mode, threshold: %ld", left_threshold_);
    }
    
    if (right_baseline_ < 1000) {
        right_threshold_ = right_baseline_ * 1.5;
        ESP_LOGI(TAG, "Touch11 using increase mode, threshold: %ld", right_threshold_);
    } else {
        ESP_LOGI(TAG, "Touch11 using decrease mode, threshold: %ld", right_threshold_);
    }
}

void TouchEngine::RegisterCallback(TouchEventCallback callback) {
    callbacks_.push_back(callback);
}

void TouchEngine::TouchTask(void* param) {
    TouchEngine* engine = static_cast<TouchEngine*>(param);
    
    ESP_LOGI(TAG, "Touch task started");
    
    while (true) {
        if (engine->enabled_) {
            engine->Process();
        }
        
        // 50ms轮询间隔
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void TouchEngine::Process() {
    // 触发测量
    touch_pad_sw_start();
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // 读取触摸传感器值
    uint32_t left_value = 0, right_value = 0;
    esp_err_t ret1 = touch_pad_read_raw_data(TOUCH_PAD_NUM10, &left_value);
    esp_err_t ret2 = touch_pad_read_raw_data(TOUCH_PAD_NUM11, &right_value);
    
    if (ret1 != ESP_OK || ret2 != ESP_OK) {
        return;  // 读取失败，跳过这次
    }
    
    // 调试输出（每2秒一次，更详细）
    static int64_t last_debug_time = 0;
    int64_t current_time = esp_timer_get_time();
    if (current_time - last_debug_time > 2000000) {
        ESP_LOGI(TAG, "Touch values:");
        ESP_LOGI(TAG, "  Left  (GPIO10): curr=%ld, base=%ld, thr=%ld, touched=%s", 
                left_value, left_baseline_, left_threshold_,
                left_touched_ ? "YES" : "NO");
        ESP_LOGI(TAG, "  Right (GPIO11): curr=%ld, base=%ld, thr=%ld, touched=%s", 
                right_value, right_baseline_, right_threshold_,
                right_touched_ ? "YES" : "NO");
        last_debug_time = current_time;
    }
    
    // 根据基准值大小判断触摸检测逻辑
    bool left_touched = false;
    bool right_touched = false;
    
    // 对于GPIO10
    if (left_baseline_ < 1000) {
        // 小基准值：触摸时增加
        left_touched = (left_value > left_threshold_);
    } else {
        // 大基准值：触摸时减少
        left_touched = (left_value < left_threshold_);
    }
    
    // 对于GPIO11
    if (right_baseline_ < 1000) {
        // 小基准值：触摸时增加
        right_touched = (right_value > right_threshold_);
    } else {
        // 大基准值：触摸时减少
        right_touched = (right_value < right_threshold_);
    }
    
    // 如果检测到显著变化，输出日志
    if (left_touched != left_touched_) {
        ESP_LOGI(TAG, "Touch10 state changed: %s (value: %ld)", 
                left_touched ? "TOUCHED" : "RELEASED", left_value);
    }
    if (right_touched != right_touched_) {
        ESP_LOGI(TAG, "Touch11 state changed: %s (value: %ld)", 
                right_touched ? "TOUCHED" : "RELEASED", right_value);
    }
    
    // 处理触摸状态
    ProcessTouchWithState(left_touched, TouchPosition::LEFT, left_state_);
    ProcessTouchWithState(right_touched, TouchPosition::RIGHT, right_state_);
    
    // 更新全局状态
    left_touched_ = left_touched;
    right_touched_ = right_touched;
}

// 保持原有的ProcessTouchWithState和其他辅助函数不变
void TouchEngine::ProcessTouchWithState(bool currently_touched, TouchPosition position, TouchState& state) {
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
        state.hold_triggered = false;
        
        ESP_LOGI(TAG, "Touch DOWN on %s", 
                position == TouchPosition::LEFT ? "LEFT" : "RIGHT");
        
    } else if (state.is_touched && !currently_touched) {
        // 释放事件
        uint32_t duration_ms = (current_time - state.touch_start_time) / 1000;
        
        if (!state.hold_triggered && duration_ms < TAP_MAX_DURATION_MS) {
            // 触发单击事件
            TouchEvent event;
            event.type = TouchEventType::SINGLE_TAP;
            event.position = position;
            event.timestamp_us = current_time;
            event.duration_ms = duration_ms;
            
            DispatchEvent(event);
            
            ESP_LOGI(TAG, "SINGLE_TAP on %s (duration: %ld ms)", 
                    position == TouchPosition::LEFT ? "LEFT" : "RIGHT", duration_ms);
        } else if (state.hold_triggered) {
            // 长按释放事件
            TouchEvent event;
            event.type = TouchEventType::RELEASE;
            event.position = position;
            event.timestamp_us = current_time;
            event.duration_ms = duration_ms;
            
            DispatchEvent(event);
            
            ESP_LOGI(TAG, "RELEASE on %s after hold (duration: %ld ms)", 
                    position == TouchPosition::LEFT ? "LEFT" : "RIGHT", duration_ms);
        }
        
        state.is_touched = false;
        state.hold_triggered = false;
        
        ESP_LOGI(TAG, "Touch UP on %s", 
                position == TouchPosition::LEFT ? "LEFT" : "RIGHT");
        
    } else if (state.is_touched && currently_touched) {
        // 持续按下状态
        uint32_t duration_ms = (current_time - state.touch_start_time) / 1000;
        
        if (!state.hold_triggered && duration_ms >= HOLD_MIN_DURATION_MS) {
            // 触发长按事件
            state.hold_triggered = true;
            
            TouchEvent event;
            event.type = TouchEventType::HOLD;
            event.position = position;
            event.timestamp_us = current_time;
            event.duration_ms = duration_ms;
            
            DispatchEvent(event);
            
            ESP_LOGI(TAG, "HOLD on %s (duration: %ld ms)", 
                    position == TouchPosition::LEFT ? "LEFT" : "RIGHT", duration_ms);
        }
    }
    
    state.was_touched = currently_touched;
}

void TouchEngine::ProcessTouch(gpio_num_t gpio, TouchPosition position, TouchState& state) {
    // 这个函数在新版本中不再使用
}

bool TouchEngine::ReadTouchGPIO(gpio_num_t gpio) {
    // 这个函数在新版本中不再使用
    return false;
}

void TouchEngine::DispatchEvent(const TouchEvent& event) {
    for (const auto& callback : callbacks_) {
        if (callback) {
            callback(event);
        }
    }
}