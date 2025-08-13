#include "event_engine.h"
#include <esp_log.h>
#include <esp_timer.h>

#define TAG "EventEngine"

EventEngine::EventEngine() 
    : motion_engine_(nullptr)
    , touch_engine_(nullptr) {
}

EventEngine::~EventEngine() {
}

void EventEngine::Initialize(MotionEngine* motion_engine, TouchEngine* touch_engine) {
    motion_engine_ = motion_engine;
    touch_engine_ = touch_engine;
    
    if (motion_engine_) {
        // 注册运动事件回调
        motion_engine_->RegisterCallback(
            [this](const MotionEvent& event) {
                this->OnMotionEvent(event);
            }
        );
        ESP_LOGI(TAG, "Event engine initialized with motion engine support");
    }
    
    if (touch_engine_) {
        // 注册触摸事件回调
        touch_engine_->RegisterCallback(
            [this](const TouchEvent& event) {
                this->OnTouchEvent(event);
            }
        );
        ESP_LOGI(TAG, "Event engine initialized with touch engine support");
    }
    
    if (!motion_engine_ && !touch_engine_) {
        ESP_LOGW(TAG, "Event engine initialized without any input engines");
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
    
    // 注意：TouchEngine有自己的任务，不需要在这里调用Process
    // TouchEngine的事件会通过回调异步到达
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

bool EventEngine::IsLeftTouched() const {
    if (touch_engine_) {
        return touch_engine_->IsLeftTouched();
    }
    return false;
}

bool EventEngine::IsRightTouched() const {
    if (touch_engine_) {
        return touch_engine_->IsRightTouched();
    }
    return false;
}

void EventEngine::OnTouchEvent(const TouchEvent& touch_event) {
    // 将TouchEvent转换为Event
    Event event;
    event.type = ConvertTouchEventType(touch_event.type, touch_event.position);
    event.timestamp_us = touch_event.timestamp_us;
    
    // 将触摸位置信息存储在touch_data中
    if (touch_event.position == TouchPosition::LEFT) {
        event.data.touch_data.x = -1;  // 左侧用负值表示
        event.data.touch_data.y = static_cast<int>(touch_event.duration_ms);
    } else {
        event.data.touch_data.x = 1;   // 右侧用正值表示
        event.data.touch_data.y = static_cast<int>(touch_event.duration_ms);
    }
    
    // 分发事件
    DispatchEvent(event);
}

EventType EventEngine::ConvertTouchEventType(TouchEventType touch_type, TouchPosition position) {
    // 根据触摸类型和位置映射到事件类型
    switch (touch_type) {
        case TouchEventType::SINGLE_TAP:
            if (position == TouchPosition::LEFT) {
                return EventType::TOUCH_TAP;  // 左侧单击映射为普通TAP
            } else {
                return EventType::TOUCH_DOUBLE_TAP;  // 右侧单击映射为DOUBLE_TAP（临时）
            }
            
        case TouchEventType::HOLD:
            return EventType::TOUCH_LONG_PRESS;
            
        case TouchEventType::RELEASE:
            // 释放事件暂时不映射，或可以创建新的事件类型
            return EventType::TOUCH_TAP;  // 临时映射
            
        default:
            return EventType::MOTION_NONE;
    }
}