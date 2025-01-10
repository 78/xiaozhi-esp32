#include "led.h"
#include "board.h"

#include <cstring>
#include <cmath>
#include <esp_log.h>

#define TAG "Led"

Led::Led(gpio_num_t gpio, uint8_t max_leds) {
    if (gpio == GPIO_NUM_NC) {
        ESP_LOGI(TAG, "Builtin LED not connected");
        return;
    }

    led_ = new Led(gpio, max_leds);

    esp_timer_create_args_t led_strip_timer_args = {
        .callback = [](void *arg) {
            auto light = static_cast<LedStripWrapper*>(arg);
            light->OnBlinkTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Led Strip Timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&led_strip_timer_args, &led_strip_timer_));
}

LedStripWrapper::~LedStripWrapper() {
    if (led_strip_timer_ != nullptr) {
        esp_timer_delete(led_strip_timer_);
    }
    if (led_ != nullptr) {
        delete led_;
    }
}

void LedStripWrapper::OnBlinkTimer() {
    std::lock_guard<std::mutex> lock(mutex_);
    counter_--;
    timer_callback_();
}

void LedStripWrapper::SetLedBasicColor(LedBasicColor color, uint8_t brightness) {
    if (led_ == nullptr) {
        ESP_LOGE(TAG, "Builtin LED not connected");
        return;
    }

    switch (color) {
        case kLedColorWhite:
            led_->SetWhite(brightness);
            break;
        case kLedColorGrey:
            led_->SetGrey(brightness);
            break;
        case kLedColorRed:
            led_->SetRed(brightness);
            break;
        case kLedColorGreen:
            led_->SetGreen(brightness);
            break;
        case kLedColorBlue:
            led_->SetBlue(brightness);
            break;
    }
}

void LedStripWrapper::SetLedStripBasicColor(uint8_t index, LedBasicColor color, uint8_t brightness) {
    if (led_ == nullptr) {
        ESP_LOGE(TAG, "Builtin LED not connected");
        return;
    }

    if (index >= led_->max_leds()) {
        ESP_LOGE(TAG, "Invalid led index: %d", index);
        return;
    }

    switch (color) {
        case kLedColorWhite:
            led_strip_set_pixel(led_->led_strip(), index, brightness, brightness, brightness);
            break;
        case kLedColorGrey:
            led_strip_set_pixel(led_->led_strip(), index, brightness, brightness, brightness);
            break;
        case kLedColorRed:
            led_strip_set_pixel(led_->led_strip(), index, brightness, 0, 0);
            break;
        case kLedColorGreen:
            led_strip_set_pixel(led_->led_strip(), index, 0, brightness, 0);
            break;
        case kLedColorBlue:
            led_strip_set_pixel(led_->led_strip(), index, 0, 0, brightness);
            break;
    }
}

void LedStripWrapper::StartBlinkTask(uint32_t times, uint32_t interval_ms) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (led_ == nullptr) {
        ESP_LOGE(TAG, "Builtin LED not connected");
        return;
    }
    
    esp_timer_stop(led_strip_timer_);
    counter_ = times * 2;
    timer_callback_ = [this]() {
        if (counter_ & 1) {
            led_->TurnOn();
        } else {
            led_->TurnOff();
            if (counter_ == 0) {
                esp_timer_stop(led_strip_timer_);
            }
        }
    };
    esp_timer_start_periodic(led_strip_timer_, interval_ms * 1000);
}

void LedStripWrapper::BlinkOnce(LedBasicColor color, uint8_t brightness) {
    Blink(color, brightness, 1, 100);
}

void LedStripWrapper::Blink(LedBasicColor color, uint32_t times, uint32_t interval_ms, uint8_t brightness) {
    SetLedBasicColor(color, brightness);
    StartBlinkTask(times, interval_ms);
}

void LedStripWrapper::ContinuousBlink(LedBasicColor color, uint32_t interval_ms, uint8_t brightness) {
    SetLedBasicColor(color, brightness);
    StartBlinkTask(COUNTER_INFINITE, interval_ms);
}

void LedStripWrapper::StaticLight(LedBasicColor color, uint8_t brightness) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (led_ == nullptr) {
        ESP_LOGE(TAG, "Builtin LED not connected");
        return;
    }

    SetLedBasicColor(color, brightness);
    esp_timer_stop(led_strip_timer_);
    led_->TurnOn();
}

void LedStripWrapper::ChasingLight(LedBasicColor base_color, LedBasicColor color, uint32_t interval_ms, uint8_t brightness) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (led_ == nullptr) {
        ESP_LOGE(TAG, "Builtin LED not connected");
        return;
    }

    esp_timer_stop(led_strip_timer_);
    counter_ = COUNTER_INFINITE;
    timer_callback_ = [this, base_color, color, brightness]() {
        auto index = counter_ % led_->max_leds();
        for (uint8_t i = 0; i < led_->max_leds(); i++) {
            if (i == index || i == (index + 1) % led_->max_leds()) {
                SetLedStripBasicColor(i, color, brightness);
            } else {
                SetLedStripBasicColor(i, base_color, LOW_BRIGHTNESS);
            }
        }
        led_strip_refresh(led_->led_strip());
    };
    esp_timer_start_periodic(led_strip_timer_, interval_ms * 1000);
}

void LedStripWrapper::BreathLight(LedBasicColor color, uint32_t interval_ms) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (led_ == nullptr) {
        ESP_LOGE(TAG, "Builtin LED not connected");
        return;
    }

    esp_timer_stop(led_strip_timer_);
    counter_ = COUNTER_INFINITE;
    timer_callback_ = [this, color]() {
        static bool increase = true;
        static uint32_t brightness = LOW_BRIGHTNESS;

        for (uint8_t i = 0; i < led_->max_leds(); i++) {
            SetLedStripBasicColor(i, color, brightness);
        }
        led_strip_refresh(led_->led_strip());
        
        if (brightness == HIGH_BRIGHTNESS) {
            increase = false;
        } else if (brightness == LOW_BRIGHTNESS) {
            increase = true;
        }

        if (increase) {
            brightness += 1;
        } else {
            brightness -= 1;
        }
    };
    esp_timer_start_periodic(led_strip_timer_, interval_ms * 1000);
}

void LedStripWrapper::LightOff() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (led_ == nullptr) {
        ESP_LOGE(TAG, "Builtin LED not connected");
        return;
    }

    esp_timer_stop(led_strip_timer_);
    led_->TurnOff();
}
