#ifndef _DEVICE_STATE_EVENT_H_
#define _DEVICE_STATE_EVENT_H_

#include <esp_event.h>
#include <functional>
#include <vector>
#include <mutex>
#include "device_state.h"

ESP_EVENT_DECLARE_BASE(XIAOZHI_STATE_EVENTS);

enum {
    XIAOZHI_STATE_CHANGED_EVENT,
};

struct device_state_event_data_t {
    DeviceState previous_state;
    DeviceState current_state;
};

class DeviceStateEventManager {
public:
    static DeviceStateEventManager& GetInstance();
    DeviceStateEventManager(const DeviceStateEventManager&) = delete;
    DeviceStateEventManager& operator=(const DeviceStateEventManager&) = delete;

    void RegisterStateChangeCallback(std::function<void(DeviceState, DeviceState)> callback);
    void PostStateChangeEvent(DeviceState previous_state, DeviceState current_state);
    std::vector<std::function<void(DeviceState, DeviceState)>> GetCallbacks();

private:
    DeviceStateEventManager();
    ~DeviceStateEventManager();

    std::vector<std::function<void(DeviceState, DeviceState)>> callbacks_;
    std::mutex mutex_;
};

#endif // _DEVICE_STATE_EVENT_H_ 