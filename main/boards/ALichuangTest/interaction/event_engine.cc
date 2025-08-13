#include "event_engine.h"
#include <esp_log.h>
#include <esp_timer.h>

#define TAG "EventEngine"

EventEngine::EventEngine() 
    : motion_engine_(nullptr) {
}

EventEngine::~EventEngine() {
}

void EventEngine::Initialize(MotionEngine* motion_engine) {
    motion_engine_ = motion_engine;
    
    if (motion_engine_) {
        // 注册运动事件回调
        motion_engine_->RegisterCallback(
            [this](const MotionEvent& event) {
                this->OnMotionEvent(event);
            }
        );
        ESP_LOGI(TAG, "Event engine initialized with motion engine support");
    } else {
        ESP_LOGI(TAG, "Event engine initialized without motion engine");
    }
}

void EventEngine::RegisterCallback(EventCallback callback) {
    global_callback_ = callback;
}

void EventEngine::RegisterCallback(EventType type, EventCallback callback) {
    type_callbacks_.push_back({type, callback});
}

void EventEngine::Process() {
    // 处理运动引擎
    if (motion_engine_) {
        motion_engine_->Process();
    }
    
    // 未来可以在这里添加其他事件源的处理
    // if (touch_engine_) {
    //     touch_engine_->Process();
    // }
    // if (audio_engine_) {
    //     audio_engine_->Process();
    // }
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

void EventEngine::OnMotionEvent(const MotionEvent& motion_event) {
    // 将MotionEvent转换为Event
    Event event;
    event.type = ConvertMotionEventType(motion_event.type);
    event.timestamp_us = motion_event.timestamp_us;
    event.data.imu_data = motion_event.imu_data;
    
    // 分发事件
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

EventType EventEngine::ConvertMotionEventType(MotionEventType motion_type) {
    switch (motion_type) {
        case MotionEventType::FREE_FALL:
            return EventType::MOTION_FREE_FALL;
        case MotionEventType::SHAKE_VIOLENTLY:
            return EventType::MOTION_SHAKE_VIOLENTLY;
        case MotionEventType::FLIP:
            return EventType::MOTION_FLIP;
        case MotionEventType::SHAKE:
            return EventType::MOTION_SHAKE;
        case MotionEventType::PICKUP:
            return EventType::MOTION_PICKUP;
        case MotionEventType::UPSIDE_DOWN:
            return EventType::MOTION_UPSIDE_DOWN;
        case MotionEventType::NONE:
        default:
            return EventType::MOTION_NONE;
    }
}

bool EventEngine::IsPickedUp() const {
    if (motion_engine_) {
        return motion_engine_->IsPickedUp();
    }
    return false;
}

bool EventEngine::IsUpsideDown() const {
    if (motion_engine_) {
        return motion_engine_->IsUpsideDown();
    }
    return false;
}