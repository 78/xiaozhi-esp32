#include "led_controller.h"
#include "config.h"
#include "application.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "KinconyLedController"

KinconyLedController::KinconyLedController(gpio_num_t bottom_gpio, gpio_num_t vertical_gpio) {
    bottom_strip_ = new CircularStrip(bottom_gpio, 3);  // 3 LEDs
    vertical_strip_ = new CircularStrip(vertical_gpio, 1);  // 1 LED
}

KinconyLedController::~KinconyLedController() {
    if (rainbow_task_ != nullptr) {
        vTaskDelete(rainbow_task_);
    }
    delete bottom_strip_;
    delete vertical_strip_;
}

void KinconyLedController::OnStateChanged() {
    auto& app = Application::GetInstance();
    auto device_state = app.GetDeviceState();
    
    // Stop rainbow if running
    TurnOff();
    
    switch (device_state) {
        case kDeviceStateStarting: {
            // Rainbow on bottom LEDs during startup
            ShowRainbow();
            vertical_strip_->SetAllColor({0, 0, 0});
            break;
        }
        case kDeviceStateWifiConfiguring: {
            // Blue blinking for WiFi config
            bottom_strip_->SetAllColor({0, 0, 32});
            bottom_strip_->Blink({0, 0, 32}, 500);
            vertical_strip_->SetAllColor({0, 0, 32});
            break;
        }
        case kDeviceStateIdle: {
            // Green for idle
            bottom_strip_->SetAllColor({0, 32, 0});
            vertical_strip_->SetAllColor({0, 32, 0});
            break;
        }
        case kDeviceStateConnecting: {
            // Yellow for connecting
            bottom_strip_->SetAllColor({32, 32, 0});
            vertical_strip_->SetAllColor({32, 32, 0});
            break;
        }
        case kDeviceStateListening: {
            // Red for listening
            bottom_strip_->SetAllColor({32, 0, 0});
            vertical_strip_->SetAllColor({32, 0, 0});
            break;
        }
        case kDeviceStateSpeaking: {
            // Purple for speaking
            bottom_strip_->SetAllColor({16, 0, 32});
            vertical_strip_->SetAllColor({16, 0, 32});
            break;
        }
        case kDeviceStateUpgrading: {
            // Cyan blinking for upgrading
            bottom_strip_->SetAllColor({0, 32, 32});
            bottom_strip_->Blink({0, 32, 32}, 100);
            vertical_strip_->SetAllColor({0, 32, 32});
            break;
        }
        case kDeviceStateActivating: {
            // White blinking for activating
            bottom_strip_->SetAllColor({16, 16, 16});
            bottom_strip_->Blink({16, 16, 16}, 500);
            vertical_strip_->SetAllColor({16, 16, 16});
            break;
        }
        default:
            ESP_LOGW(TAG, "Unknown led state: %d", device_state);
            bottom_strip_->SetAllColor({0, 0, 0});
            vertical_strip_->SetAllColor({0, 0, 0});
            break;
    }
}

void KinconyLedController::ShowRainbow() {
    if (rainbow_task_ == nullptr) {
        xTaskCreate(RainbowTaskWrapper, "rainbow_task", 2048, this, 5, &rainbow_task_);
    }
}

void KinconyLedController::TurnOff() {
    if (rainbow_task_ != nullptr) {
        vTaskDelete(rainbow_task_);
        rainbow_task_ = nullptr;
    }
    // Don't set colors here, let OnStateChanged handle it
}

void KinconyLedController::RainbowTask() {
    uint8_t hue = 0;
    while (true) {
        // Rainbow colors for bottom LEDs
        for (int i = 0; i < 3; i++) {
            float h = (hue + i * 120.0f) / 360.0f;  // Distribute hues
            uint8_t r, g, b;
            if (h < 1.0f/3.0f) {
                r = 255; g = (uint8_t)(h * 3 * 255); b = 0;
            } else if (h < 2.0f/3.0f) {
                r = (uint8_t)((2.0f/3.0f - h) * 3 * 255); g = 255; b = 0;
            } else {
                r = 0; g = 255; b = (uint8_t)((h - 2.0f/3.0f) * 3 * 255);
            }
            bottom_strip_->SetSingleColor(i, {r, g, b});
        }
        
        // Vertical LED - cycling through rainbow
        float h = hue / 360.0f;
        uint8_t r, g, b;
        if (h < 1.0f/3.0f) {
            r = 255; g = (uint8_t)(h * 3 * 255); b = 0;
        } else if (h < 2.0f/3.0f) {
            r = (uint8_t)((2.0f/3.0f - h) * 3 * 255); g = 255; b = 0;
        } else {
            r = 0; g = 255; b = (uint8_t)((h - 2.0f/3.0f) * 3 * 255);
        }
        vertical_strip_->SetSingleColor(0, {r, g, b});

        hue = (hue + 1) % 360;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void KinconyLedController::RainbowTaskWrapper(void* arg) {
    static_cast<KinconyLedController*>(arg)->RainbowTask();
}