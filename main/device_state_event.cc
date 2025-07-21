#include "device_state_event.h"

ESP_EVENT_DEFINE_BASE(XIAOZHI_STATE_EVENTS);

DeviceStateEventManager& DeviceStateEventManager::GetInstance() {
    static DeviceStateEventManager instance;
    return instance;
}

void DeviceStateEventManager::RegisterStateChangeCallback(std::function<void(DeviceState, DeviceState)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.push_back(callback);
}

void DeviceStateEventManager::PostStateChangeEvent(DeviceState previous_state, DeviceState current_state) {
    device_state_event_data_t event_data = {
        .previous_state = previous_state,
        .current_state = current_state
    };
    esp_event_post(XIAOZHI_STATE_EVENTS, XIAOZHI_STATE_CHANGED_EVENT, &event_data, sizeof(event_data), portMAX_DELAY);
}

std::vector<std::function<void(DeviceState, DeviceState)>> DeviceStateEventManager::GetCallbacks() {
    std::lock_guard<std::mutex> lock(mutex_);
    return callbacks_;
}

DeviceStateEventManager::DeviceStateEventManager() {
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(esp_event_handler_register(XIAOZHI_STATE_EVENTS, XIAOZHI_STATE_CHANGED_EVENT, 
        [](void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
            auto* data = static_cast<device_state_event_data_t*>(event_data);
            auto& manager = DeviceStateEventManager::GetInstance();
            for (const auto& callback : manager.GetCallbacks()) {
                callback(data->previous_state, data->current_state);
            }
        }, nullptr));
}

DeviceStateEventManager::~DeviceStateEventManager() {
    esp_event_handler_unregister(XIAOZHI_STATE_EVENTS, XIAOZHI_STATE_CHANGED_EVENT, nullptr);
}