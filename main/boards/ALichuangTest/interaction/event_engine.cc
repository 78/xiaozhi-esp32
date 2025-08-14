#include "event_engine.h"
#include "motion_engine.h"
#include "touch_engine.h"
#include "event_processor.h"
#include "event_config_loader.h"
#include <esp_log.h>
#include <esp_timer.h>

#define TAG "EventEngine"

EventEngine::EventEngine() 
    : motion_engine_(nullptr)
    , owns_motion_engine_(false)
    , touch_engine_(nullptr)
    , owns_touch_engine_(false)
    , event_processor_(nullptr) {
    // 创建事件处理器
    event_processor_ = new EventProcessor();
}

EventEngine::~EventEngine() {
    // 清理内部创建的引擎
    if (owns_motion_engine_ && motion_engine_) {
        delete motion_engine_;
        motion_engine_ = nullptr;
    }
    if (owns_touch_engine_ && touch_engine_) {
        delete touch_engine_;
        touch_engine_ = nullptr;
    }
    if (event_processor_) {
        delete event_processor_;
        event_processor_ = nullptr;
    }
}

void EventEngine::Initialize() {
    // 尝试从配置文件加载，如果失败则使用默认配置
    LoadEventConfiguration();
    ESP_LOGI(TAG, "Event engine initialized with event processor");
}

void EventEngine::LoadEventConfiguration() {
    // 首先尝试从文件系统加载配置
    const char* config_path = "/spiffs/event_config.json";
    bool loaded = EventConfigLoader::LoadFromFile(config_path, this);
    
    if (!loaded) {
        // 如果文件不存在或加载失败，使用嵌入的默认配置
        ESP_LOGI(TAG, "Loading embedded default event configuration");
        EventConfigLoader::LoadFromEmbedded(this);
    }
}

void EventEngine::ConfigureDefaultEventProcessing() {
    // 这个方法现在作为备用，如果配置加载完全失败时使用
    // 触摸事件：使用冷却策略，避免连续触发
    event_processor_->ConfigureEventType(EventType::TOUCH_TAP, 
        EventProcessingPresets::TouchTapConfig());
    
    // 运动事件：使用节流策略
    event_processor_->ConfigureEventType(EventType::MOTION_SHAKE, 
        EventProcessingPresets::MotionEventConfig());
    event_processor_->ConfigureEventType(EventType::MOTION_FLIP, 
        EventProcessingPresets::MotionEventConfig());
    
    // 紧急事件：立即处理
    event_processor_->ConfigureEventType(EventType::MOTION_FREE_FALL, 
        EventProcessingPresets::EmergencyEventConfig());
    
    ESP_LOGI(TAG, "Fallback event processing strategies configured");
}

void EventEngine::ConfigureEventProcessing(EventType type, const EventProcessingConfig& config) {
    if (event_processor_) {
        event_processor_->ConfigureEventType(type, config);
    }
}

void EventEngine::SetDefaultProcessingStrategy(const EventProcessingConfig& config) {
    if (event_processor_) {
        event_processor_->SetDefaultStrategy(config);
    }
}

EventProcessor::EventStats EventEngine::GetEventStats(EventType type) const {
    if (event_processor_) {
        return event_processor_->GetStats(type);
    }
    return EventProcessor::EventStats{0, 0, 0, 0, 0};
}

void EventEngine::InitializeMotionEngine(Qmi8658* imu, bool enable_debug) {
    if (!imu) {
        ESP_LOGW(TAG, "Cannot initialize motion engine without IMU");
        return;
    }
    
    // 如果已经存在旧的引擎，先清理
    if (owns_motion_engine_ && motion_engine_) {
        delete motion_engine_;
    }
    
    // 创建新的运动引擎
    motion_engine_ = new MotionEngine();
    motion_engine_->Initialize(imu);
    
    if (enable_debug) {
        motion_engine_->SetDebugOutput(true);
    }
    
    owns_motion_engine_ = true;
    
    // 设置回调
    SetupMotionEngineCallbacks();
    
    ESP_LOGI(TAG, "Motion engine initialized and registered with event engine");
}

void EventEngine::InitializeTouchEngine() {
    // 如果已经存在旧的引擎，先清理
    if (owns_touch_engine_ && touch_engine_) {
        delete touch_engine_;
    }
    
    // 创建新的触摸引擎
    touch_engine_ = new TouchEngine();
    touch_engine_->Initialize();
    owns_touch_engine_ = true;
    
    // 设置回调
    SetupTouchEngineCallbacks();
    
    ESP_LOGI(TAG, "Touch engine initialized and registered with event engine - GPIO10 (LEFT), GPIO11 (RIGHT)");
}

void EventEngine::SetupMotionEngineCallbacks() {
    if (motion_engine_) {
        motion_engine_->RegisterCallback(
            [this](const MotionEvent& event) {
                this->OnMotionEvent(event);
            }
        );
    }
}

void EventEngine::SetupTouchEngineCallbacks() {
    if (touch_engine_) {
        ESP_LOGI(TAG, "Registering touch engine callback");
        touch_engine_->RegisterCallback(
            [this](const TouchEvent& event) {
                ESP_LOGI(TAG, "Lambda callback invoked for touch event");
                this->OnTouchEvent(event);
            }
        );
        ESP_LOGI(TAG, "Touch engine callback registered");
    } else {
        ESP_LOGW(TAG, "Touch engine is null, cannot register callback");
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
    ESP_LOGI(TAG, "DispatchEvent called with event type=%d", (int)event.type);
    
    // 通过事件处理器处理事件
    Event processed_event;
    bool should_process = event_processor_->ProcessEvent(event, processed_event);
    
    if (!should_process) {
        // 事件被丢弃（防抖、节流、冷却等）
        return;
    }
    
    // 调用全局回调
    if (global_callback_) {
        global_callback_(processed_event);
    }
    
    // 调用特定类型的回调
    for (const auto& pair : type_callbacks_) {
        if (pair.first == processed_event.type) {
            pair.second(processed_event);
        }
    }
    
    // 处理队列中的事件
    Event queued_event;
    while (event_processor_->GetNextQueuedEvent(queued_event)) {
        if (global_callback_) {
            global_callback_(queued_event);
        }
        for (const auto& pair : type_callbacks_) {
            if (pair.first == queued_event.type) {
                pair.second(queued_event);
            }
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
    
    // 如果事件类型无效，不处理
    if (event.type == EventType::MOTION_NONE) {
        ESP_LOGD(TAG, "Touch event type %d not mapped, ignoring", (int)touch_event.type);
        return;
    }
    
    // 将触摸位置信息存储在touch_data中
    if (touch_event.position == TouchPosition::LEFT) {
        event.data.touch_data.x = -1;  // 左侧用负值表示
        event.data.touch_data.y = static_cast<int>(touch_event.duration_ms);
    } else {
        event.data.touch_data.x = 1;   // 右侧用正值表示
        event.data.touch_data.y = static_cast<int>(touch_event.duration_ms);
    }
    
    ESP_LOGI(TAG, "Touch event received: touch_type=%d -> event_type=%d, position=%s, duration=%ldms", 
            (int)touch_event.type,
            (int)event.type,
            touch_event.position == TouchPosition::LEFT ? "LEFT" : "RIGHT",
            touch_event.duration_ms);
    
    // 分发事件
    DispatchEvent(event);
}

EventType EventEngine::ConvertTouchEventType(TouchEventType touch_type, TouchPosition position) {
    // 根据触摸类型和位置映射到事件类型
    switch (touch_type) {
        case TouchEventType::SINGLE_TAP:
            return EventType::TOUCH_TAP;  // 左右侧单击都映射为TAP
            
        case TouchEventType::HOLD:
            return EventType::TOUCH_LONG_PRESS;
            
        case TouchEventType::RELEASE:
            // 释放事件暂时不处理
            return EventType::MOTION_NONE;
            
        case TouchEventType::CRADLED:
            // TODO: 可以添加特殊的摇篮模式事件类型
            ESP_LOGI(TAG, "CRADLED event detected but not mapped to specific EventType");
            return EventType::MOTION_NONE;
            
        case TouchEventType::TICKLED:
            // TODO: 可以添加特殊的挠痒模式事件类型
            ESP_LOGI(TAG, "TICKLED event detected but not mapped to specific EventType");
            return EventType::MOTION_NONE;
            
        default:
            return EventType::MOTION_NONE;
    }
}