#include "input/button_manager.h"
#include "esp_log.h"

static const char* TAG = "ButtonManager";

ButtonManager& ButtonManager::GetInstance() {
    static ButtonManager inst;
    return inst;
}

void ButtonManager::Init(const std::map<ButtonId, gpio_num_t>& gpio_map) {
    ESP_LOGI(TAG, "ButtonManager init");
    (void)gpio_map;
}

void ButtonManager::RegisterCallback(ButtonId id, std::function<void()> cb) {
    callbacks_[id] = cb;
}

void ButtonManager::RegisterScreenCallback(ScreenId screen, ButtonId id, std::function<void()> cb) {
    screen_callbacks_[screen][id] = cb;
}

void ButtonManager::SetActiveScreen(ScreenId screen) {
    active_screen_ = screen;
}

void ButtonManager::Trigger(ButtonId id) {
    auto sit = screen_callbacks_.find(active_screen_);
    if (sit != screen_callbacks_.end()) {
        auto bit = sit->second.find(id);
        if (bit != sit->second.end() && bit->second) {
            bit->second();
            return;
        }
    }

    auto it = callbacks_.find(id);
    if (it != callbacks_.end() && it->second) {
        it->second();
    }
}
