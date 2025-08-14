#ifndef ALICHUANGTEST_EVENT_PROCESSOR_H
#define ALICHUANGTEST_EVENT_PROCESSOR_H

#include <queue>
#include <map>
#include <esp_timer.h>

// 前向声明
struct Event;
enum class EventType;

// 事件处理策略
enum class EventProcessingStrategy {
    IMMEDIATE,      // 立即处理每个事件
    DEBOUNCE,       // 防抖：只处理最后一个事件
    THROTTLE,       // 节流：固定时间内只处理第一个事件
    QUEUE,          // 排队：按顺序处理，可设置最小间隔
    MERGE,          // 合并：相同类型事件合并处理
    COOLDOWN        // 冷却：处理后需要冷却时间才能处理下一个
};

// 事件处理配置
struct EventProcessingConfig {
    EventProcessingStrategy strategy;
    uint32_t interval_ms;      // 防抖/节流/冷却时间（毫秒）
    uint32_t merge_window_ms;  // 合并窗口时间
    uint32_t max_queue_size;   // 最大队列长度
    bool allow_interrupt;      // 是否允许被高优先级事件打断
    
    EventProcessingConfig() 
        : strategy(EventProcessingStrategy::IMMEDIATE)
        , interval_ms(500)
        , merge_window_ms(1000)
        , max_queue_size(10)
        , allow_interrupt(false) {}
};

// 事件处理器类
class EventProcessor {
public:
    EventProcessor();
    ~EventProcessor() = default;
    
    // 配置特定事件类型的处理策略
    void ConfigureEventType(EventType type, const EventProcessingConfig& config);
    
    // 设置默认策略
    void SetDefaultStrategy(const EventProcessingConfig& config);
    
    // 处理事件（返回是否应该立即处理）
    bool ProcessEvent(const Event& event, Event& processed_event);
    
    // 获取待处理的事件队列
    bool GetNextQueuedEvent(Event& event);
    
    // 清空特定类型的事件队列
    void ClearEventQueue(EventType type);
    
    // 检查是否在冷却期
    bool IsInCooldown(EventType type) const;
    
    // 获取事件统计信息
    struct EventStats {
        uint32_t received_count;    // 接收的事件数
        uint32_t processed_count;   // 处理的事件数
        uint32_t dropped_count;     // 丢弃的事件数
        uint32_t merged_count;      // 合并的事件数
        int64_t last_process_time;  // 最后处理时间
    };
    
    EventStats GetStats(EventType type) const;
    
private:
    // 事件状态追踪
    struct EventState {
        int64_t last_trigger_time;
        int64_t last_process_time;
        uint32_t pending_count;
        void* pending_event;  // 使用void*避免依赖Event定义
        bool has_pending;
        EventProcessingConfig config;
        EventStats stats;
        
        EventState() 
            : last_trigger_time(0)
            , last_process_time(0)
            , pending_count(0)
            , pending_event(nullptr)
            , has_pending(false)
            , stats{0, 0, 0, 0, 0} {}
        
        ~EventState() {
            // 清理pending_event内存将在EventProcessor中处理
        }
    };
    
    // 每种事件类型的状态
    std::map<int, EventState> event_states_;  // 使用int代替EventType
    
    // 事件队列（用于QUEUE策略）
    std::queue<void*> event_queue_;  // 使用void*代替Event
    
    // 默认配置
    EventProcessingConfig default_config_;
    
    // 处理策略实现
    bool ProcessImmediate(Event& event, EventState& state);
    bool ProcessDebounce(Event& event, EventState& state);
    bool ProcessThrottle(Event& event, EventState& state);
    bool ProcessQueue(Event& event, EventState& state);
    bool ProcessMerge(Event& event, EventState& state);
    bool ProcessCooldown(Event& event, EventState& state);
    
    // 合并事件逻辑
    void MergeEvents(Event& existing, const Event& new_event);
};

// 预定义的处理策略配置
namespace EventProcessingPresets {
    // 触摸事件：防抖300ms，避免误触
    inline EventProcessingConfig TouchTapConfig() {
        EventProcessingConfig config;
        config.strategy = EventProcessingStrategy::COOLDOWN;
        config.interval_ms = 300;  // 300ms冷却时间
        return config;
    }
    
    // 多次触摸：合并处理，转换为特殊动作
    inline EventProcessingConfig MultiTapConfig() {
        EventProcessingConfig config;
        config.strategy = EventProcessingStrategy::MERGE;
        config.merge_window_ms = 2000;  // 2秒内的点击合并
        config.interval_ms = 500;       // 处理后500ms冷却
        return config;
    }
    
    // 运动事件：节流处理，避免过于频繁
    inline EventProcessingConfig MotionEventConfig() {
        EventProcessingConfig config;
        config.strategy = EventProcessingStrategy::THROTTLE;
        config.interval_ms = 1000;  // 1秒内只处理一次
        return config;
    }
    
    // 紧急事件：立即处理
    inline EventProcessingConfig EmergencyEventConfig() {
        EventProcessingConfig config;
        config.strategy = EventProcessingStrategy::IMMEDIATE;
        config.allow_interrupt = true;
        return config;
    }
    
    // 队列处理：按顺序执行，设置最小间隔
    inline EventProcessingConfig QueuedEventConfig() {
        EventProcessingConfig config;
        config.strategy = EventProcessingStrategy::QUEUE;
        config.interval_ms = 800;  // 每个事件至少间隔800ms
        config.max_queue_size = 5;
        return config;
    }
}

#endif // ALICHUANGTEST_EVENT_PROCESSOR_H