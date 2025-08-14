#include "event_config_loader.h"
#include <esp_log.h>
#include <cJSON.h>
#include <fstream>
#include <sstream>

#define TAG "EventConfigLoader"

// 静态成员初始化
std::map<std::string, EventResponse> EventConfigLoader::response_map_;

// 默认配置（嵌入式版本）
const char* DefaultEventConfig::GetDefaultConfig() {
    return R"({
        "event_processing_strategies": {
            "touch_events": {
                "TOUCH_TAP": {
                    "strategy": "MERGE",
                    "merge_window_ms": 1500,
                    "interval_ms": 500
                },
                "TOUCH_LONG_PRESS": {
                    "strategy": "COOLDOWN",
                    "interval_ms": 1000
                }
            },
            "motion_events": {
                "MOTION_SHAKE": {
                    "strategy": "THROTTLE",
                    "interval_ms": 2000
                },
                "MOTION_FREE_FALL": {
                    "strategy": "IMMEDIATE",
                    "allow_interrupt": true
                }
            },
            "default_strategy": {
                "strategy": "IMMEDIATE",
                "interval_ms": 0
            }
        }
    })";
}

bool EventConfigLoader::LoadFromFile(const std::string& filepath, EventEngine* engine) {
    // 尝试从文件系统读取配置
    FILE* file = fopen(filepath.c_str(), "r");
    if (!file) {
        ESP_LOGW(TAG, "Config file not found: %s, using default config", filepath.c_str());
        return LoadFromEmbedded(engine);
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // 读取文件内容
    char* json_data = new char[file_size + 1];
    fread(json_data, 1, file_size, file);
    json_data[file_size] = '\0';
    fclose(file);
    
    // 解析JSON
    bool result = ParseJsonConfig(json_data, engine);
    delete[] json_data;
    
    if (!result) {
        ESP_LOGW(TAG, "Failed to parse config file, using default config");
        return LoadFromEmbedded(engine);
    }
    
    ESP_LOGI(TAG, "Loaded event config from file: %s", filepath.c_str());
    return true;
}

bool EventConfigLoader::LoadFromEmbedded(EventEngine* engine) {
    const char* default_config = DefaultEventConfig::GetDefaultConfig();
    return ParseJsonConfig(default_config, engine);
}

bool EventConfigLoader::ParseJsonConfig(const char* json_data, EventEngine* engine) {
    cJSON* root = cJSON_Parse(json_data);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON config");
        return false;
    }
    
    // 解析事件处理策略
    cJSON* strategies = cJSON_GetObjectItem(root, "event_processing_strategies");
    if (strategies) {
        // 解析默认策略
        cJSON* default_strategy = cJSON_GetObjectItem(strategies, "default_strategy");
        if (default_strategy) {
            EventProcessingConfig config;
            
            cJSON* strategy = cJSON_GetObjectItem(default_strategy, "strategy");
            if (strategy) {
                config.strategy = ParseStrategy(strategy->valuestring);
            }
            
            cJSON* interval = cJSON_GetObjectItem(default_strategy, "interval_ms");
            if (interval) {
                config.interval_ms = interval->valueint;
            }
            
            engine->SetDefaultProcessingStrategy(config);
            ESP_LOGI(TAG, "Set default strategy: %d with interval %ldms", 
                    (int)config.strategy, config.interval_ms);
        }
        
        // 解析触摸事件策略
        cJSON* touch_events = cJSON_GetObjectItem(strategies, "touch_events");
        if (touch_events) {
            cJSON* event = NULL;
            cJSON_ArrayForEach(event, touch_events) {
                if (!event->string) continue;
                
                EventType event_type = ParseEventType(event->string);
                if (event_type == EventType::MOTION_NONE) continue;
                
                EventProcessingConfig config;
                
                cJSON* strategy = cJSON_GetObjectItem(event, "strategy");
                if (strategy) {
                    config.strategy = ParseStrategy(strategy->valuestring);
                }
                
                cJSON* interval = cJSON_GetObjectItem(event, "interval_ms");
                if (interval) {
                    config.interval_ms = interval->valueint;
                }
                
                cJSON* merge_window = cJSON_GetObjectItem(event, "merge_window_ms");
                if (merge_window) {
                    config.merge_window_ms = merge_window->valueint;
                }
                
                cJSON* max_queue = cJSON_GetObjectItem(event, "max_queue_size");
                if (max_queue) {
                    config.max_queue_size = max_queue->valueint;
                }
                
                cJSON* allow_interrupt = cJSON_GetObjectItem(event, "allow_interrupt");
                if (allow_interrupt) {
                    config.allow_interrupt = cJSON_IsTrue(allow_interrupt);
                }
                
                engine->ConfigureEventProcessing(event_type, config);
                ESP_LOGI(TAG, "Configured %s with strategy %d", 
                        event->string, (int)config.strategy);
            }
        }
        
        // 解析运动事件策略
        cJSON* motion_events = cJSON_GetObjectItem(strategies, "motion_events");
        if (motion_events) {
            cJSON* event = NULL;
            cJSON_ArrayForEach(event, motion_events) {
                if (!event->string) continue;
                
                EventType event_type = ParseEventType(event->string);
                if (event_type == EventType::MOTION_NONE) continue;
                
                EventProcessingConfig config;
                
                cJSON* strategy = cJSON_GetObjectItem(event, "strategy");
                if (strategy) {
                    config.strategy = ParseStrategy(strategy->valuestring);
                }
                
                cJSON* interval = cJSON_GetObjectItem(event, "interval_ms");
                if (interval) {
                    config.interval_ms = interval->valueint;
                }
                
                cJSON* allow_interrupt = cJSON_GetObjectItem(event, "allow_interrupt");
                if (allow_interrupt) {
                    config.allow_interrupt = cJSON_IsTrue(allow_interrupt);
                }
                
                engine->ConfigureEventProcessing(event_type, config);
                ESP_LOGI(TAG, "Configured %s with strategy %d", 
                        event->string, (int)config.strategy);
            }
        }
    }
    
    // 解析响应映射
    cJSON* response_mappings = cJSON_GetObjectItem(root, "response_mappings");
    if (response_mappings) {
        // 解析单击响应
        cJSON* single_tap = cJSON_GetObjectItem(response_mappings, "single_tap");
        if (single_tap) {
            cJSON* left = cJSON_GetObjectItem(single_tap, "left");
            if (left) {
                EventResponse response;
                cJSON* motion = cJSON_GetObjectItem(left, "motion");
                if (motion) response.motion = motion->valuestring;
                cJSON* sound = cJSON_GetObjectItem(left, "sound");
                if (sound) response.sound = sound->valuestring;
                cJSON* emotion = cJSON_GetObjectItem(left, "emotion");
                if (emotion) response.emotion = emotion->valuestring;
                
                response_map_["tap_left"] = response;
            }
            
            cJSON* right = cJSON_GetObjectItem(single_tap, "right");
            if (right) {
                EventResponse response;
                cJSON* motion = cJSON_GetObjectItem(right, "motion");
                if (motion) response.motion = motion->valuestring;
                cJSON* sound = cJSON_GetObjectItem(right, "sound");
                if (sound) response.sound = sound->valuestring;
                cJSON* emotion = cJSON_GetObjectItem(right, "emotion");
                if (emotion) response.emotion = emotion->valuestring;
                
                response_map_["tap_right"] = response;
            }
        }
        
        // 解析多击响应
        cJSON* multi_tap = cJSON_GetObjectItem(response_mappings, "multi_tap");
        if (multi_tap) {
            cJSON* tap_config = NULL;
            cJSON_ArrayForEach(tap_config, multi_tap) {
                if (!tap_config->string) continue;
                
                EventResponse response;
                cJSON* motion = cJSON_GetObjectItem(tap_config, "motion");
                if (motion) response.motion = motion->valuestring;
                cJSON* sound = cJSON_GetObjectItem(tap_config, "sound");
                if (sound) response.sound = sound->valuestring;
                cJSON* emotion = cJSON_GetObjectItem(tap_config, "emotion");
                if (emotion) response.emotion = emotion->valuestring;
                
                response_map_[std::string("multi_") + tap_config->string] = response;
            }
        }
    }
    
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Event config loaded successfully");
    return true;
}

EventProcessingStrategy EventConfigLoader::ParseStrategy(const std::string& strategy) {
    if (strategy == "IMMEDIATE") return EventProcessingStrategy::IMMEDIATE;
    if (strategy == "DEBOUNCE") return EventProcessingStrategy::DEBOUNCE;
    if (strategy == "THROTTLE") return EventProcessingStrategy::THROTTLE;
    if (strategy == "QUEUE") return EventProcessingStrategy::QUEUE;
    if (strategy == "MERGE") return EventProcessingStrategy::MERGE;
    if (strategy == "COOLDOWN") return EventProcessingStrategy::COOLDOWN;
    
    ESP_LOGW(TAG, "Unknown strategy: %s, using IMMEDIATE", strategy.c_str());
    return EventProcessingStrategy::IMMEDIATE;
}

EventType EventConfigLoader::ParseEventType(const std::string& type_str) {
    // 触摸事件
    if (type_str == "TOUCH_TAP") return EventType::TOUCH_TAP;
    if (type_str == "TOUCH_DOUBLE_TAP") return EventType::TOUCH_DOUBLE_TAP;
    if (type_str == "TOUCH_LONG_PRESS") return EventType::TOUCH_LONG_PRESS;
    
    // 运动事件
    if (type_str == "MOTION_SHAKE") return EventType::MOTION_SHAKE;
    if (type_str == "MOTION_FLIP") return EventType::MOTION_FLIP;
    if (type_str == "MOTION_PICKUP") return EventType::MOTION_PICKUP;
    if (type_str == "MOTION_FREE_FALL") return EventType::MOTION_FREE_FALL;
    if (type_str == "MOTION_SHAKE_VIOLENTLY") return EventType::MOTION_SHAKE_VIOLENTLY;
    if (type_str == "MOTION_UPSIDE_DOWN") return EventType::MOTION_UPSIDE_DOWN;
    
    ESP_LOGW(TAG, "Unknown event type: %s", type_str.c_str());
    return EventType::MOTION_NONE;
}

EventResponse EventConfigLoader::GetResponseForEvent(EventType type, const Event& event) {
    // 根据事件类型和数据获取响应
    if (type == EventType::TOUCH_TAP) {
        // 检查是左侧还是右侧
        std::string key = event.data.touch_data.x < 0 ? "tap_left" : "tap_right";
        auto it = response_map_.find(key);
        if (it != response_map_.end()) {
            return it->second;
        }
    }
    
    // 返回默认响应
    return EventResponse("", "", "neutral");
}

EventResponse EventConfigLoader::GetMultiTapResponse(int tap_count) {
    std::string key = "multi_" + std::to_string(tap_count) + "_taps";
    auto it = response_map_.find(key);
    if (it != response_map_.end()) {
        return it->second;
    }
    
    // 返回默认响应
    return EventResponse("", "", "neutral");
}

bool EventConfigLoader::CheckSpecialPattern(const std::vector<Event>& recent_events) {
    // TODO: 实现特殊模式检测
    // 例如：检查是否是左右交替点击等
    return false;
}