#pragma once
#include <functional>
#include <map>
#include <driver/gpio.h>
#include "ui/screen.h"
#include <vector>

enum class ButtonId {
    MENU_UP,
    MENU_DOWN,
    SELECT,
    BACK,
    PTT,
    PTT_ALT
};

class ButtonManager {
public:
    static ButtonManager& GetInstance();
    void Init(const std::map<ButtonId, gpio_num_t>& gpio_map);
    void RegisterCallback(ButtonId id, std::function<void()> cb);
    // Register a callback specific to a screen. When the active screen matches,
    // the callback will be invoked for the corresponding button trigger.
    void RegisterScreenCallback(ScreenId screen, ButtonId id, std::function<void()> cb);
    // Set the currently active screen
    void SetActiveScreen(ScreenId screen);
    // Trigger a button event (called by board button handlers)
    void Trigger(ButtonId id);

private:
    ButtonManager() = default;
    std::map<ButtonId, std::function<void()>> callbacks_;
    ScreenId active_screen_ = ScreenId::MAIN;
    // Map (screen -> (button -> callback))
    std::map<ScreenId, std::map<ButtonId, std::function<void()>>> screen_callbacks_;
};
