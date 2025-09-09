#include "circular_strip.h"
#include "application.h"
#include <esp_log.h>

#define TAG "CircularStrip"

#define BLINK_INFINITE -1

CircularStrip::CircularStrip(gpio_num_t gpio, uint8_t max_leds) : max_leds_(max_leds) {
    // If the gpio is not connected, you should use NoLed class
    assert(gpio != GPIO_NUM_NC);

    colors_.resize(max_leds_);

    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = gpio;
    strip_config.max_leds = max_leds_;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.led_model = LED_MODEL_WS2812;

    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000; // 10MHz

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
    led_strip_clear(led_strip_);

    esp_timer_create_args_t strip_timer_args = {
        .callback = [](void *arg) {
            auto strip = static_cast<CircularStrip*>(arg);
            std::lock_guard<std::mutex> lock(strip->mutex_);
            if (strip->strip_callback_ != nullptr) {
                strip->strip_callback_();
            }
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "strip_timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&strip_timer_args, &strip_timer_));
}

CircularStrip::~CircularStrip() {
    esp_timer_stop(strip_timer_);
    if (led_strip_ != nullptr) {
        led_strip_del(led_strip_);
    }
}


void CircularStrip::SetAllColor(StripColor color) {
    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(strip_timer_);
    for (int i = 0; i < max_leds_; i++) {
        colors_[i] = color;
        led_strip_set_pixel(led_strip_, i, color.red, color.green, color.blue);
    }
    led_strip_refresh(led_strip_);
}

void CircularStrip::SetSingleColor(uint8_t index, StripColor color) {
    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(strip_timer_);
    colors_[index] = color;
    led_strip_set_pixel(led_strip_, index, color.red, color.green, color.blue);
    led_strip_refresh(led_strip_);
}

void CircularStrip::Blink(StripColor color, int interval_ms) {
    for (int i = 0; i < max_leds_; i++) {
        colors_[i] = color;
    }
    StartStripTask(interval_ms, [this]() {
        static bool on = true;
        if (on) {
            for (int i = 0; i < max_leds_; i++) {
                led_strip_set_pixel(led_strip_, i, colors_[i].red, colors_[i].green, colors_[i].blue);
            }
            led_strip_refresh(led_strip_);
        } else {
            led_strip_clear(led_strip_);
        }
        on = !on;
    });
}

void CircularStrip::FadeOut(int interval_ms) {
    StartStripTask(interval_ms, [this]() {
        bool all_off = true;
        for (int i = 0; i < max_leds_; i++) {
            colors_[i].red /= 2;
            colors_[i].green /= 2;
            colors_[i].blue /= 2;
            if (colors_[i].red != 0 || colors_[i].green != 0 || colors_[i].blue != 0) {
                all_off = false;
            }
            led_strip_set_pixel(led_strip_, i, colors_[i].red, colors_[i].green, colors_[i].blue);
        }
        if (all_off) {
            led_strip_clear(led_strip_);
            esp_timer_stop(strip_timer_);
        } else {
            led_strip_refresh(led_strip_);
        }
    });
}

void CircularStrip::Breathe(StripColor low, StripColor high, int interval_ms) {
    StartStripTask(interval_ms, [this, low, high]() {
        static bool increase = true;
        static StripColor color = low;
        if (increase) {
            if (color.red < high.red) {
                color.red++;
            }
            if (color.green < high.green) {
                color.green++;
            }
            if (color.blue < high.blue) {
                color.blue++;
            }
            if (color.red == high.red && color.green == high.green && color.blue == high.blue) {
                increase = false;
            }
        } else {
            if (color.red > low.red) {
                color.red--;
            }
            if (color.green > low.green) {
                color.green--;
            }
            if (color.blue > low.blue) {
                color.blue--;
            }
            if (color.red == low.red && color.green == low.green && color.blue == low.blue) {
                increase = true;
            }
        }
        for (int i = 0; i < max_leds_; i++) {
            led_strip_set_pixel(led_strip_, i, color.red, color.green, color.blue);
        }
        led_strip_refresh(led_strip_);
    });
}

void CircularStrip::Scroll(StripColor low, StripColor high, int length, int interval_ms) {
    for (int i = 0; i < max_leds_; i++) {
        colors_[i] = low;
    }
    StartStripTask(interval_ms, [this, low, high, length]() {
        static int offset = 0;
        for (int i = 0; i < max_leds_; i++) {
            colors_[i] = low;
        }
        for (int j = 0; j < length; j++) {
            int i = (offset + j) % max_leds_;
            colors_[i] = high;
        }
        for (int i = 0; i < max_leds_; i++) {
            led_strip_set_pixel(led_strip_, i, colors_[i].red, colors_[i].green, colors_[i].blue);
        }
        led_strip_refresh(led_strip_);
        offset = (offset + 1) % max_leds_;
    });
}

void CircularStrip::StartStripTask(int interval_ms, std::function<void()> cb) {
    if (led_strip_ == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(strip_timer_);
    
    strip_callback_ = cb;
    esp_timer_start_periodic(strip_timer_, interval_ms * 1000);
}

void CircularStrip::SetBrightness(uint8_t default_brightness, uint8_t low_brightness) {
    default_brightness_ = default_brightness;
    low_brightness_ = low_brightness;
    OnStateChanged();
}

void CircularStrip::OnStateChanged() {
    auto& app = Application::GetInstance();
    auto device_state = app.GetDeviceState();
    switch (device_state) {
        case kDeviceStateStarting: {
            StripColor low = { 0, 0, 0 };
            StripColor high = { low_brightness_, low_brightness_, default_brightness_ };
            Scroll(low, high, 3, 100);
            break;
        }
        case kDeviceStateWifiConfiguring: {
            StripColor color = { low_brightness_, low_brightness_, default_brightness_ };
            Blink(color, 500);
            break;
        }
        case kDeviceStateIdle:
            FadeOut(50);
            break;
        case kDeviceStateConnecting: {
            StripColor color = { low_brightness_, low_brightness_, default_brightness_ };
            SetAllColor(color);
            break;
        }
        case kDeviceStateListening:
        case kDeviceStateAudioTesting: {
            StripColor color = { default_brightness_, low_brightness_, low_brightness_ };
            SetAllColor(color);
            break;
        }
        case kDeviceStateSpeaking: {
            StripColor color = { low_brightness_, default_brightness_, low_brightness_ };
            SetAllColor(color);
            break;
        }
        case kDeviceStateUpgrading: {
            StripColor color = { low_brightness_, default_brightness_, low_brightness_ };
            Blink(color, 100);
            break;
        }
        case kDeviceStateActivating: {
            StripColor color = { low_brightness_, default_brightness_, low_brightness_ };
            Blink(color, 500);
            break;
        }
        default:
            ESP_LOGW(TAG, "Unknown led strip event: %d", device_state);
            return;
    }
}
