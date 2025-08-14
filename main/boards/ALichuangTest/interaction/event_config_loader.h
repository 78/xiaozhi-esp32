#ifndef ALICHUANGTEST_EVENT_CONFIG_LOADER_H
#define ALICHUANGTEST_EVENT_CONFIG_LOADER_H

#include "event_processor.h"
#include "event_engine.h"
#include <string>
#include <map>

// 事件响应动作
struct EventResponse {
    std::string motion;      // 动作名称
    std::string sound;       // 声音效果
    std::string emotion;     // 情感状态
    
    EventResponse() = default;
    EventResponse(const std::string& m, const std::string& s, const std::string& e)
        : motion(m), sound(s), emotion(e) {}
};

// 事件配置加载器
class EventConfigLoader {
public:
    // 从JSON文件加载配置
    static bool LoadFromFile(const std::string& filepath, EventEngine* engine);
    
    // 从嵌入的配置加载（编译时嵌入）
    static bool LoadFromEmbedded(EventEngine* engine);
    
    // 获取事件响应映射
    static EventResponse GetResponseForEvent(EventType type, const Event& event);
    
    // 获取多次点击的响应
    static EventResponse GetMultiTapResponse(int tap_count);
    
    // 检查是否需要特殊模式处理
    static bool CheckSpecialPattern(const std::vector<Event>& recent_events);
    
private:
    // 解析策略字符串
    static EventProcessingStrategy ParseStrategy(const std::string& strategy);
    
    // 解析事件类型字符串
    static EventType ParseEventType(const std::string& type_str);
    
    // 存储响应映射
    static std::map<std::string, EventResponse> response_map_;
    
    // 解析JSON配置
    static bool ParseJsonConfig(const char* json_data, EventEngine* engine);
};

// 默认配置（编译时嵌入）
namespace DefaultEventConfig {
    // 获取默认配置JSON字符串
    const char* GetDefaultConfig();
    
    // 获取触摸事件默认配置
    inline EventProcessingConfig GetDefaultTouchConfig() {
        EventProcessingConfig config;
        config.strategy = EventProcessingStrategy::MERGE;
        config.merge_window_ms = 1500;
        config.interval_ms = 500;
        return config;
    }
    
    // 获取运动事件默认配置
    inline EventProcessingConfig GetDefaultMotionConfig() {
        EventProcessingConfig config;
        config.strategy = EventProcessingStrategy::THROTTLE;
        config.interval_ms = 2000;
        return config;
    }
    
    // 获取紧急事件默认配置
    inline EventProcessingConfig GetDefaultEmergencyConfig() {
        EventProcessingConfig config;
        config.strategy = EventProcessingStrategy::IMMEDIATE;
        config.allow_interrupt = true;
        return config;
    }
}

#endif // ALICHUANGTEST_EVENT_CONFIG_LOADER_H