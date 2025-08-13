#include "vibration.h"
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <string.h>
#include <cmath>

static const char* TAG = "Vibration";

// 简化振动控制 - 使用GPIO直接开关，避免LEDC冲突

// 音频提示功能 - 简单的日志提示（可以替换为实际音频播放）
void PlayBeepSound() {
    ESP_LOGI(TAG, "🔊 BEEP! Pattern starting in 1 second...");
    
    // 等待1秒作为提示持续时间
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// 振动任务配置
#define VIBRATION_TASK_STACK_SIZE 4096  // 增加栈大小，确保4字节对齐
#define VIBRATION_TASK_PRIORITY   3
#define VIBRATION_QUEUE_SIZE      8

// 振动模式数据定义 - 存储在Flash中节省RAM
static const vibration_keyframe_t vibration_short_buzz[] = {
    {255, 80},  // 高强度80ms
    {0, 0}      // 结束标记
};

static const vibration_keyframe_t vibration_purr_short[] = {
    {80, 50}, {100, 50}, {90, 50}, {110, 50}, {85, 50},
    {95, 50}, {75, 100}, {0, 0}
};

static const vibration_keyframe_t vibration_purr_pattern[] = {
    {60, 100}, {80, 100}, {70, 100}, {90, 100}, 
    {65, 100}, {85, 100}, {75, 200},
    {50, 100}, {70, 100}, {60, 100}, {80, 100},
    {0, 300}, // 300ms暂停后重复
    {60, 100}, {80, 100}, {70, 100}, {90, 100},
    {0, 0}
};

static const vibration_keyframe_t vibration_gentle_heartbeat[] = {
    {150, 100}, {80, 50},   // 强弱心跳
    {0, 600},               // 心跳间隔
    {150, 100}, {80, 50},   // 强弱心跳
    {0, 600},               // 心跳间隔
    {150, 100}, {80, 50},   // 强弱心跳
    {0, 0}
};

static const vibration_keyframe_t vibration_struggle_pattern[] = {
    {200, 80}, {0, 60}, {250, 120}, {0, 40}, 
    {180, 100}, {0, 80}, {220, 150}, {0, 50},
    {190, 90}, {0, 70}, {240, 110}, {0, 90},
    {210, 130}, {0, 0}
};

static const vibration_keyframe_t vibration_sharp_buzz[] = {
    {255, 150},  // 高强度150ms
    {0, 0}
};

static const vibration_keyframe_t vibration_tremble_pattern[] = {
    {40, 30}, {0, 20}, {45, 30}, {0, 20}, {35, 30}, {0, 20},
    {50, 30}, {0, 20}, {42, 30}, {0, 20}, {38, 30}, {0, 20},
    {47, 30}, {0, 20}, {41, 30}, {0, 20}, {36, 30}, {0, 20},
    {0, 200}, // 短暂停顿
    {40, 30}, {0, 20}, {45, 30}, {0, 20}, {35, 30}, {0, 20},
    {0, 0}
};

static const vibration_keyframe_t vibration_giggle_pattern[] = {
    {120, 60}, {0, 40}, {130, 50}, {0, 30}, {140, 60}, {0, 40},
    {125, 50}, {0, 30}, {135, 60}, {0, 40}, {115, 50}, {0, 30},
    {145, 60}, {0, 40}, {120, 50}, {0, 30}, {130, 60}, {0, 200},
    {0, 0}
};

static const vibration_keyframe_t vibration_heartbeat_strong[] = {
    {200, 120}, {120, 80},  // 强心跳
    {0, 800},               // 更长的心跳间隔
    {200, 120}, {120, 80},  // 强心跳
    {0, 800},               // 更长的心跳间隔
    {200, 120}, {120, 80},  // 强心跳
    {0, 0}
};

static const vibration_keyframe_t vibration_erratic_strong[] = {
    {255, 70}, {0, 30}, {200, 120}, {0, 60}, {240, 90}, {0, 20},
    {180, 140}, {0, 80}, {220, 60}, {0, 40}, {255, 100}, {0, 90},
    {160, 110}, {0, 50}, {230, 80}, {0, 30}, {190, 130}, {0, 70},
    {255, 90}, {0, 40}, {210, 100}, {0, 0}
};

// 振动模式查找表
static const vibration_keyframe_t* const vibration_patterns[] = {
    [VIBRATION_SHORT_BUZZ] = vibration_short_buzz,
    [VIBRATION_PURR_SHORT] = vibration_purr_short,
    [VIBRATION_PURR_PATTERN] = vibration_purr_pattern,
    [VIBRATION_GENTLE_HEARTBEAT] = vibration_gentle_heartbeat,
    [VIBRATION_STRUGGLE_PATTERN] = vibration_struggle_pattern,
    [VIBRATION_SHARP_BUZZ] = vibration_sharp_buzz,
    [VIBRATION_TREMBLE_PATTERN] = vibration_tremble_pattern,
    [VIBRATION_GIGGLE_PATTERN] = vibration_giggle_pattern,
    [VIBRATION_HEARTBEAT_STRONG] = vibration_heartbeat_strong,
    [VIBRATION_ERRATIC_STRONG] = vibration_erratic_strong
};

static const char* const vibration_pattern_names[] = {
    [VIBRATION_SHORT_BUZZ] = "SHORT_BUZZ",
    [VIBRATION_PURR_SHORT] = "PURR_SHORT", 
    [VIBRATION_PURR_PATTERN] = "PURR_PATTERN",
    [VIBRATION_GENTLE_HEARTBEAT] = "GENTLE_HEARTBEAT",
    [VIBRATION_STRUGGLE_PATTERN] = "STRUGGLE_PATTERN",
    [VIBRATION_SHARP_BUZZ] = "SHARP_BUZZ",
    [VIBRATION_TREMBLE_PATTERN] = "TREMBLE_PATTERN",
    [VIBRATION_GIGGLE_PATTERN] = "GIGGLE_PATTERN",
    [VIBRATION_HEARTBEAT_STRONG] = "HEARTBEAT_STRONG",
    [VIBRATION_ERRATIC_STRONG] = "ERRATIC_STRONG",
    [VIBRATION_STOP] = "STOP"
};

// Vibration类实现
Vibration::Vibration() 
    : vibration_pin_(VIBRATION_MOTOR_GPIO),  // 使用GPIO10物理控制
      vibration_queue_(nullptr),
      vibration_task_handle_(nullptr),
      initialized_(false),
      current_pattern_(VIBRATION_MAX),
      current_emotion_("neutral"),
      emotion_based_enabled_(true),
      test_button_pin_(VIBRATION_TEST_BUTTON_GPIO),  // GPIO11测试按键
      button_test_task_handle_(nullptr),
      current_test_pattern_(VIBRATION_SHORT_BUZZ),  // 默认测试模式
      button_test_enabled_(false),
      cycle_test_mode_(false) {
}

Vibration::~Vibration() {
    if (initialized_) {
        // 停止振动
        Stop();
        
        // 禁用按键测试
        DisableButtonTest();
        
        // 删除任务
        if (vibration_task_handle_) {
            vTaskDelete(vibration_task_handle_);
            vibration_task_handle_ = nullptr;
        }
        
        // 删除队列
        if (vibration_queue_) {
            vQueueDelete(vibration_queue_);
            vibration_queue_ = nullptr;
        }
        
        initialized_ = false;
    }
}

esp_err_t Vibration::Initialize() {
    if (initialized_) {
        ESP_LOGW(TAG, "Vibration already initialized");
        return ESP_OK;
    }
    
    // 初始化GPIO10控制振动马达
    esp_err_t ret = InitVibrationGpio();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize vibration GPIO");
        return ret;
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "Vibration GPIO initialized successfully on GPIO%d", vibration_pin_);
    return ESP_OK;
}

esp_err_t Vibration::StartTask() {
    if (!initialized_) {
        ESP_LOGE(TAG, "Vibration not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (vibration_task_handle_ != nullptr) {
        ESP_LOGW(TAG, "Vibration task already started");
        return ESP_OK;
    }
    
    
    // 创建消息队列
    vibration_queue_ = xQueueCreate(VIBRATION_QUEUE_SIZE, sizeof(vibration_id_t));
    if (vibration_queue_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create vibration queue");
        return ESP_ERR_NO_MEM;
    }
    
    // 创建振动处理任务
    BaseType_t task_ret = xTaskCreate(
        VibrationTask,
        "vibration_task",
        VIBRATION_TASK_STACK_SIZE,
        this,
        VIBRATION_TASK_PRIORITY,
        &vibration_task_handle_
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create vibration task");
        vQueueDelete(vibration_queue_);
        vibration_queue_ = nullptr;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Vibration task started successfully on GPIO%d", vibration_pin_);
    return ESP_OK;
}

void Vibration::Play(vibration_id_t id) {
    if (!initialized_) {
        ESP_LOGW(TAG, "Vibration not initialized, call Initialize() first");
        return;
    }
    
    if (vibration_queue_ == nullptr || vibration_task_handle_ == nullptr) {
        ESP_LOGW(TAG, "Vibration task not started, call StartTask() first");
        return;
    }
    
    if (id >= VIBRATION_MAX && id != VIBRATION_STOP) {
        ESP_LOGW(TAG, "Invalid vibration ID: %d", id);
        return;
    }
    
    // 非阻塞发送振动命令到队列
    BaseType_t ret = xQueueSend(vibration_queue_, &id, 0);
    if (ret != pdTRUE) {
        ESP_LOGW(TAG, "Vibration queue full, pattern request dropped");
    } else {
        ESP_LOGD(TAG, "Queued vibration pattern: %s", vibration_pattern_names[id]);
    }
}

void Vibration::Stop() {
    Play(VIBRATION_STOP);
}

void Vibration::PlayForEmotion(const std::string& emotion) {
    if (!emotion_based_enabled_) {
        return;
    }
    
    current_emotion_ = emotion;
    vibration_id_t vibration_id = GetVibrationForEmotion(emotion);
    
    if (vibration_id != VIBRATION_MAX) {
        ESP_LOGI(TAG, "Playing vibration for emotion: %s -> %s", 
                emotion.c_str(), vibration_pattern_names[vibration_id]);
        Play(vibration_id);
    }
}

void Vibration::SetEmotionBasedEnabled(bool enabled) {
    emotion_based_enabled_ = enabled;
    ESP_LOGI(TAG, "Emotion-based vibration %s", enabled ? "enabled" : "disabled");
}

// 暂时未启用
vibration_id_t Vibration::GetVibrationForEmotion(const std::string& emotion) {
    // 根据情绪映射到对应的振动模式
    if (emotion == "happy" || emotion == "funny") {
        return VIBRATION_GIGGLE_PATTERN;
    }
    else if (emotion == "laughing") {
        return VIBRATION_GIGGLE_PATTERN;
    }
    else if (emotion == "angry") {
        return VIBRATION_STRUGGLE_PATTERN;
    }
    else if (emotion == "sad" || emotion == "crying") {
        return VIBRATION_GENTLE_HEARTBEAT;
    }
    else if (emotion == "surprised" || emotion == "shocked") {
        return VIBRATION_SHARP_BUZZ;
    }
    else if (emotion == "excited") {
        return VIBRATION_ERRATIC_STRONG;
    }
    else if (emotion == "comfortable" || emotion == "relaxed") {
        return VIBRATION_PURR_PATTERN;
    }
    else if (emotion == "thinking") {
        return VIBRATION_TREMBLE_PATTERN;
    }
    else if (emotion == "neutral") {
        return VIBRATION_SHORT_BUZZ;
    }
    
    // 默认情况下不播放振动
    return VIBRATION_MAX;
}

void Vibration::SetVibrationStrength(uint8_t strength) {
    if (vibration_pin_ == GPIO_NUM_NC) return;
    
    // 使用GPIO直接控制振动马达
    // 强度大于0时开启，等于0时关闭
    if (strength > 0) {
        gpio_set_level(vibration_pin_, 1);
        ESP_LOGD(TAG, "🔵 Vibration ON (strength: %d)", strength);
    } else {
        gpio_set_level(vibration_pin_, 0);
        ESP_LOGD(TAG, "⚫ Vibration OFF");
    }
}

void Vibration::PlayVibrationPattern(const vibration_keyframe_t* pattern) {
    if (!pattern || !initialized_) return;
    
    const vibration_keyframe_t* frame = pattern;
    
    while (frame->duration_ms > 0 || frame->strength > 0) {
        // 设置振动强度
        SetVibrationStrength(frame->strength);
        
        // 等待指定时间
        if (frame->duration_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(frame->duration_ms));
        }
        
        frame++;
        
        // 检查是否收到停止命令
        vibration_id_t stop_cmd;
        if (xQueueReceive(vibration_queue_, &stop_cmd, 0) == pdTRUE) {
            if (stop_cmd == VIBRATION_STOP) {
                ESP_LOGI(TAG, "Received stop command, terminating current pattern");
                break;
            } else {
                // 如果收到新的振动命令，将其放回队列稍后处理
                xQueueSendToFront(vibration_queue_, &stop_cmd, 0);
                ESP_LOGI(TAG, "Received new pattern command while playing, will handle after current");
                break;
            }
        }
    }
    
    // 确保振动停止
    SetVibrationStrength(0);
}

esp_err_t Vibration::InitVibrationGpio() {
    // 配置GPIO10为输出模式
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << vibration_pin_);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 初始化为低电平（关闭振动）
    gpio_set_level(vibration_pin_, 0);
    
    ESP_LOGI(TAG, "Vibration GPIO initialized on GPIO%d", vibration_pin_);
    return ESP_OK;
}

esp_err_t Vibration::InitTestButton() {
    // 配置GPIO11为输入模式，启用下拉电阻
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << test_button_pin_);
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;  // 启用下拉，确保低电平
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure test button GPIO: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Test button initialized on GPIO%d", test_button_pin_);
    return ESP_OK;
}

void Vibration::VibrationTask(void* parameter) {
    // 添加nullptr检查
    if (parameter == nullptr) {
        ESP_LOGE(TAG, "VibrationTask received null parameter");
        vTaskDelete(NULL);
        return;
    }
    
    Vibration* skill = static_cast<Vibration*>(parameter);
    ESP_LOGI(TAG, "Vibration task started");
    
    vibration_id_t pattern_id;
    
    while (true) {
        // 检查队列是否有效
        if (skill->vibration_queue_ == nullptr) {
            ESP_LOGE(TAG, "Vibration queue is null, terminating task");
            vTaskDelete(NULL);
            return;
        }
        
        // 阻塞等待振动命令
        if (xQueueReceive(skill->vibration_queue_, &pattern_id, portMAX_DELAY) == pdTRUE) {
            
            if (pattern_id == VIBRATION_STOP) {
                ESP_LOGI(TAG, "Stopping all vibrations");
                skill->SetVibrationStrength(0);
                skill->current_pattern_ = VIBRATION_MAX;
                continue;
            }
            
            if (pattern_id >= VIBRATION_MAX) {
                ESP_LOGW(TAG, "Invalid vibration pattern ID: %d", pattern_id);
                continue;
            }
            
            skill->current_pattern_ = pattern_id;
            ESP_LOGI(TAG, "Playing vibration pattern: %s", vibration_pattern_names[pattern_id]);
            
            // 获取对应的振动模式并播放
            const vibration_keyframe_t* pattern = vibration_patterns[pattern_id];
            if (pattern) {
                skill->PlayVibrationPattern(pattern);
            } else {
                ESP_LOGW(TAG, "Pattern not found for ID: %d", pattern_id);
            }
            
            skill->current_pattern_ = VIBRATION_MAX;
            ESP_LOGD(TAG, "Finished playing pattern: %s", vibration_pattern_names[pattern_id]);
        }
    }
}

void Vibration::ButtonTestTask(void* parameter) {
    Vibration* skill = static_cast<Vibration*>(parameter);
    if (parameter == nullptr) {
        ESP_LOGE(TAG, "ButtonTestTask received null parameter");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Button test task started on GPIO%d", skill->test_button_pin_);
    
    bool last_button_state = false;
    bool current_button_state = false;
    
    while (skill->button_test_enabled_) {
        // 读取按键状态
        current_button_state = gpio_get_level(skill->test_button_pin_);
        
        // 检测按键按下（上升沿触发）
        if (current_button_state && !last_button_state) {
            if (skill->cycle_test_mode_) {
                ESP_LOGI(TAG, "🔘 Test button pressed! Starting cycle test of all patterns");
                
                // 循环测试所有振动模式
                for (int i = 0; i < VIBRATION_MAX; i++) {
                    vibration_id_t pattern_id = static_cast<vibration_id_t>(i);
                    
                    ESP_LOGI(TAG, "🎵 Preparing pattern %d/%d: %s", i + 1, VIBRATION_MAX, vibration_pattern_names[pattern_id]);
                    
                    // 播放音频提示
                    PlayBeepSound();
                    
                    ESP_LOGI(TAG, "▶️  Now playing vibration pattern: %s", vibration_pattern_names[pattern_id]);
                    
                    // 触发振动模式
                    skill->Play(pattern_id);
                    
                    // 等待振动结束（给足够时间让模式播放完成）
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    
                    // 5秒间隔
                    if (i < VIBRATION_MAX - 1) {  // 最后一个模式后不需要等待
                        ESP_LOGI(TAG, "⏳ Waiting 5 seconds before next pattern...");
                        vTaskDelay(pdMS_TO_TICKS(5000));
                    }
                }
                
                ESP_LOGI(TAG, "✅ Cycle test completed!");
            } else {
                ESP_LOGI(TAG, "🔘 Test button pressed! Playing pattern: %s", 
                        vibration_pattern_names[skill->current_test_pattern_]);
                
                // 触发当前测试振动模式
                skill->Play(skill->current_test_pattern_);
            }
        }
        
        last_button_state = current_button_state;
        
        // 短暂延迟，避免CPU占用过高
        vTaskDelay(pdMS_TO_TICKS(50));  // 20Hz检测频率
    }
    
    ESP_LOGI(TAG, "Button test task stopped");
    vTaskDelete(NULL);
}

void Vibration::EnableButtonTest(vibration_id_t pattern_id) {
    if (!initialized_) {
        ESP_LOGW(TAG, "Vibration not initialized");
        return;
    }
    
    if (pattern_id >= VIBRATION_MAX) {
        ESP_LOGW(TAG, "Invalid pattern ID for button test: %d", pattern_id);
        return;
    }
    
    if (button_test_enabled_) {
        ESP_LOGW(TAG, "Button test already enabled");
        return;
    }
    
    // 初始化测试按键
    esp_err_t ret = InitTestButton();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize test button");
        return;
    }
    
    current_test_pattern_ = pattern_id;
    button_test_enabled_ = true;
    cycle_test_mode_ = false;  // 单一模式测试
    
    // 创建按键测试任务
    BaseType_t task_ret = xTaskCreate(
        ButtonTestTask,
        "button_test_task",
        2048,
        this,
        2,  // 中等优先级
        &button_test_task_handle_
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button test task");
        button_test_enabled_ = false;
        return;
    }
    
    ESP_LOGI(TAG, "Button test enabled with pattern: %s", vibration_pattern_names[pattern_id]);
}

void Vibration::EnableButtonCycleTest() {
    if (!initialized_) {
        ESP_LOGW(TAG, "Vibration not initialized");
        return;
    }
    
    if (button_test_enabled_) {
        ESP_LOGW(TAG, "Button test already enabled");
        return;
    }
    
    // 初始化测试按键
    esp_err_t ret = InitTestButton();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize test button");
        return;
    }
    
    button_test_enabled_ = true;
    cycle_test_mode_ = true;  // 循环模式测试
    current_test_pattern_ = VIBRATION_SHORT_BUZZ;  // 默认值，循环模式下不使用
    
    // 创建按键测试任务
    BaseType_t task_ret = xTaskCreate(
        ButtonTestTask,
        "button_cycle_test_task",
        4096,  // 增加栈大小，因为循环测试需要更多空间
        this,
        2,  // 中等优先级
        &button_test_task_handle_
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button cycle test task");
        button_test_enabled_ = false;
        return;
    }
    
    ESP_LOGI(TAG, "Button cycle test enabled - press button to test all %d patterns", VIBRATION_MAX);
}

void Vibration::DisableButtonTest() {
    if (!button_test_enabled_) {
        return;
    }
    
    button_test_enabled_ = false;
    
    // 等待任务自然结束
    if (button_test_task_handle_) {
        vTaskDelay(pdMS_TO_TICKS(100));  // 给任务时间退出
        button_test_task_handle_ = nullptr;
    }
    
    ESP_LOGI(TAG, "Button test disabled");
}

void Vibration::SetTestPattern(vibration_id_t pattern_id) {
    if (pattern_id >= VIBRATION_MAX) {
        ESP_LOGW(TAG, "Invalid pattern ID: %d", pattern_id);
        return;
    }
    
    current_test_pattern_ = pattern_id;
    ESP_LOGI(TAG, "Test pattern changed to: %s", vibration_pattern_names[pattern_id]);
}