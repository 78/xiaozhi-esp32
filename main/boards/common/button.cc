#include "button.h"

#include <esp_log.h>

static const char* TAG = "Button";

Button::Button(gpio_num_t gpio_num, bool active_high, uint16_t short_press_time) : gpio_num_(gpio_num) {
    if (gpio_num == GPIO_NUM_NC) {
        return;
    }
    button_config_t button_config = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 1000,
        .short_press_time = short_press_time,
        .gpio_button_config = {
            .gpio_num = gpio_num,
            .active_level = static_cast<uint8_t>(active_high ? 1 : 0)
        }
    };
    button_handle_ = iot_button_create(&button_config);
    if (button_handle_ == NULL) {
        ESP_LOGE(TAG, "Failed to create button handle");
        return;
    }
}

Button::~Button() {
    if (button_handle_ != NULL) {
        iot_button_delete(button_handle_);
    }
}

void Button::OnPressDown(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_press_down_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_PRESS_DOWN, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_press_down_) {
            button->on_press_down_();
        }
    }, this);
}

void Button::OnPressUp(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_press_up_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_PRESS_UP, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_press_up_) {
            button->on_press_up_();
        }
    }, this);
}

void Button::OnLongPress(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_long_press_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_LONG_PRESS_START, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_long_press_) {
            button->on_long_press_();
        }
    }, this);
}

void Button::OnClick(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_click_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_SINGLE_CLICK, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_click_) {
            button->on_click_();
        }
    }, this);
}

void Button::OnDoubleClick(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_double_click_ = callback;
    iot_button_register_cb(button_handle_, BUTTON_DOUBLE_CLICK, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_double_click_) {
            button->on_double_click_();
        }
    }, this);
}

void Button::OnThreeClick(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_three_click_ = callback;
    button_event_config_t btn_cfg;
    btn_cfg.event = BUTTON_MULTIPLE_CLICK;
    btn_cfg.event_data.multiple_clicks.clicks = 3;
    iot_button_register_event_cb(button_handle_, btn_cfg, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_three_click_) {
            button->on_three_click_();
        }
    }, this);
}

void Button::OnFourClick(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_four_click_ = callback;
    button_event_config_t btn_cfg;
    btn_cfg.event = BUTTON_MULTIPLE_CLICK;
    btn_cfg.event_data.multiple_clicks.clicks = 4;
    iot_button_register_event_cb(button_handle_, btn_cfg, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);
        if (button->on_four_click_) {
            button->on_four_click_();
        }
    }, this);
}

int Button::getButtonLevel() const {
    if (gpio_num_ == GPIO_NUM_NC) {
        return -1;
    }
    return gpio_get_level(gpio_num_);
}
