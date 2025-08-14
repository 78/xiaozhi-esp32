#include "event_processor.h"
#include "event_engine.h"  // 现在包含完整定义
#include <esp_log.h>
#include <algorithm>

#define TAG "EventProcessor"

// 辅助函数：获取策略名称
static const char* GetStrategyName(EventProcessingStrategy strategy) {
    switch (strategy) {
        case EventProcessingStrategy::IMMEDIATE: return "IMMEDIATE";
        case EventProcessingStrategy::DEBOUNCE: return "DEBOUNCE";
        case EventProcessingStrategy::THROTTLE: return "THROTTLE";
        case EventProcessingStrategy::QUEUE: return "QUEUE";
        case EventProcessingStrategy::MERGE: return "MERGE";
        case EventProcessingStrategy::COOLDOWN: return "COOLDOWN";
        default: return "UNKNOWN";
    }
}

EventProcessor::EventProcessor() {
    // 设置默认策略为立即处理
    default_config_.strategy = EventProcessingStrategy::IMMEDIATE;
    default_config_.interval_ms = 0;
    ESP_LOGI(TAG, "EventProcessor created with default strategy IMMEDIATE");
}

void EventProcessor::ConfigureEventType(EventType type, const EventProcessingConfig& config) {
    event_states_[(int)type].config = config;
    ESP_LOGI(TAG, "Configured event type %d with strategy %d, interval %ldms", 
            (int)type, (int)config.strategy, config.interval_ms);
}

void EventProcessor::SetDefaultStrategy(const EventProcessingConfig& config) {
    default_config_ = config;
    ESP_LOGI(TAG, "Set default strategy to %d with interval %ldms", 
            (int)config.strategy, config.interval_ms);
}

bool EventProcessor::ProcessEvent(const Event& event, Event& processed_event) {
    // 获取或创建事件状态
    auto& state = event_states_[(int)event.type];
    
    // 输出接收到的事件
    ESP_LOGI(TAG, "[接收] Event type %d, strategy: %s", 
            (int)event.type, 
            GetStrategyName(state.config.strategy));
    
    // 如果没有配置，使用默认配置
    if (state.config.strategy == EventProcessingStrategy::IMMEDIATE && 
        state.config.interval_ms == 0 && 
        state.last_process_time == 0) {
        state.config = default_config_;
    }
    
    // 更新统计
    state.stats.received_count++;
    
    // 根据策略处理事件
    bool should_process = false;
    processed_event = event;  // 默认使用原事件
    
    switch (state.config.strategy) {
        case EventProcessingStrategy::IMMEDIATE:
            should_process = ProcessImmediate(processed_event, state);
            break;
        case EventProcessingStrategy::DEBOUNCE:
            should_process = ProcessDebounce(processed_event, state);
            break;
        case EventProcessingStrategy::THROTTLE:
            should_process = ProcessThrottle(processed_event, state);
            break;
        case EventProcessingStrategy::QUEUE:
            should_process = ProcessQueue(processed_event, state);
            break;
        case EventProcessingStrategy::MERGE:
            should_process = ProcessMerge(processed_event, state);
            break;
        case EventProcessingStrategy::COOLDOWN:
            should_process = ProcessCooldown(processed_event, state);
            break;
    }
    
    if (should_process) {
        state.stats.processed_count++;
        state.last_process_time = esp_timer_get_time();
        ESP_LOGI(TAG, "[处理] Event type %d processed (total processed: %ld, dropped: %ld, merged: %ld)", 
                (int)event.type, state.stats.processed_count, 
                state.stats.dropped_count, state.stats.merged_count);
    } else {
        state.stats.dropped_count++;
        ESP_LOGW(TAG, "[丢弃] Event type %d dropped by %s strategy (total dropped: %ld)", 
                (int)event.type, GetStrategyName(state.config.strategy), 
                state.stats.dropped_count);
    }
    
    return should_process;
}

bool EventProcessor::ProcessImmediate(Event& event, EventState& state) {
    // 立即处理所有事件
    return true;
}

bool EventProcessor::ProcessDebounce(Event& event, EventState& state) {
    int64_t current_time = esp_timer_get_time();
    
    // 保存事件，等待防抖时间结束
    if (state.pending_event) delete (Event*)state.pending_event;
    state.pending_event = new Event(event);
    state.has_pending = true;
    state.last_trigger_time = current_time;
    state.pending_count++;
    
    ESP_LOGD(TAG, "[DEBOUNCE] Event saved, pending count: %ld, waiting %ldms", 
            state.pending_count, state.config.interval_ms);
    
    // 检查是否应该处理
    if (state.pending_count == 1) {
        // 第一个事件，等待防抖时间
        return false;
    }
    
    // 检查防抖时间是否已过
    if ((current_time - state.last_trigger_time) >= (state.config.interval_ms * 1000)) {
        event = *(Event*)state.pending_event;
        delete (Event*)state.pending_event;
        state.pending_event = nullptr;
        state.has_pending = false;
        state.pending_count = 0;
        return true;
    }
    
    return false;
}

bool EventProcessor::ProcessThrottle(Event& event, EventState& state) {
    int64_t current_time = esp_timer_get_time();
    int64_t time_since_last = (current_time - state.last_process_time) / 1000;
    
    // 检查是否在节流时间内
    if (time_since_last < state.config.interval_ms) {
        ESP_LOGD(TAG, "[THROTTLE] Event throttled, %lldms remaining", 
                state.config.interval_ms - time_since_last);
        return false;  // 还在节流期，丢弃事件
    }
    
    ESP_LOGD(TAG, "[THROTTLE] Event allowed after %lldms", time_since_last);
    
    return true;
}

bool EventProcessor::ProcessQueue(Event& event, EventState& state) {
    // 添加到队列
    if (event_queue_.size() < state.config.max_queue_size) {
        event_queue_.push(new Event(event));
        ESP_LOGD(TAG, "Event queued, queue size: %zu", event_queue_.size());
    } else {
        ESP_LOGW(TAG, "Event queue full, dropping event");
        return false;
    }
    
    // 检查是否可以处理队列中的事件
    int64_t current_time = esp_timer_get_time();
    if ((current_time - state.last_process_time) >= (state.config.interval_ms * 1000)) {
        if (!event_queue_.empty()) {
            Event* queued_event = (Event*)event_queue_.front();
            event = *queued_event;
            delete queued_event;
            event_queue_.pop();
            return true;
        }
    }
    
    return false;
}

bool EventProcessor::ProcessMerge(Event& event, EventState& state) {
    int64_t current_time = esp_timer_get_time();
    
    // 检查是否在合并窗口内
    if (state.has_pending && 
        (current_time - state.last_trigger_time) < (state.config.merge_window_ms * 1000)) {
        // 合并事件
        MergeEvents(*(Event*)state.pending_event, event);
        state.pending_count++;
        state.stats.merged_count++;
        state.last_trigger_time = current_time;
        
        ESP_LOGI(TAG, "[MERGE] Event merged, total %ld events in window, merged count: %ld", 
                state.pending_count, state.stats.merged_count);
        return false;  // 继续等待更多事件
    }
    
    // 合并窗口结束或第一个事件
    if (state.has_pending) {
        // 处理之前合并的事件
        event = *(Event*)state.pending_event;
        delete (Event*)state.pending_event;
        state.pending_event = nullptr;
        state.has_pending = false;
        state.pending_count = 0;
        return true;
    } else {
        // 开始新的合并窗口
        state.pending_event = new Event(event);
        state.has_pending = true;
        state.pending_count = 1;
        state.last_trigger_time = current_time;
        return false;
    }
}

bool EventProcessor::ProcessCooldown(Event& event, EventState& state) {
    int64_t current_time = esp_timer_get_time();
    int64_t time_since_last = (current_time - state.last_process_time) / 1000;
    
    // 检查是否在冷却期
    if (time_since_last < state.config.interval_ms) {
        ESP_LOGI(TAG, "[COOLDOWN] Event in cooldown, %lldms remaining", 
                state.config.interval_ms - time_since_last);
        return false;
    }
    
    ESP_LOGD(TAG, "[COOLDOWN] Event allowed after %lldms cooldown", time_since_last);
    
    return true;
}

void EventProcessor::MergeEvents(Event& existing, const Event& new_event) {
    // 合并逻辑：根据事件类型决定如何合并
    if (existing.type == new_event.type) {
        // 对于触摸事件，增加计数
        if (existing.type == EventType::TOUCH_TAP) {
            // 使用touch_data.y存储点击次数（原本是持续时间）
            existing.data.touch_data.y++;
            ESP_LOGD(TAG, "Merged tap event, count: %d", existing.data.touch_data.y);
        }
        // 可以根据需要添加其他事件类型的合并逻辑
    }
}

bool EventProcessor::GetNextQueuedEvent(Event& event) {
    if (!event_queue_.empty()) {
        Event* queued_event = (Event*)event_queue_.front();
        event = *queued_event;
        delete queued_event;
        event_queue_.pop();
        return true;
    }
    return false;
}

void EventProcessor::ClearEventQueue(EventType type) {
    // 清空特定类型的事件
    std::queue<void*> new_queue;
    while (!event_queue_.empty()) {
        Event* e = (Event*)event_queue_.front();
        event_queue_.pop();
        if (e->type != type) {
            new_queue.push(e);
        } else {
            delete e;
        }
    }
    event_queue_ = new_queue;
}

bool EventProcessor::IsInCooldown(EventType type) const {
    auto it = event_states_.find((int)type);
    if (it == event_states_.end()) {
        return false;
    }
    
    int64_t current_time = esp_timer_get_time();
    return (current_time - it->second.last_process_time) < (it->second.config.interval_ms * 1000);
}

EventProcessor::EventStats EventProcessor::GetStats(EventType type) const {
    auto it = event_states_.find((int)type);
    if (it != event_states_.end()) {
        return it->second.stats;
    }
    return EventStats{0, 0, 0, 0, 0};
}