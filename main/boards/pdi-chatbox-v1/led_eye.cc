#include "Led_Eye.h"
#include "application.h"
#include <esp_log.h>

#define TAG "LEDEYE"

#define DEFAULT_BRIGHTNESS 32
#define HIGH_BRIGHTNESS 64
#define LOW_BRIGHTNESS 18

#define BLINK_INFINITE -1


LedEye::LedEye(gpio_num_t gpio) {
    assert(gpio != GPIO_NUM_NC);
    led_strip_config_t strip_config = {};
    strip_config.strip_gpio_num = gpio;
    strip_config.max_leds = LED_MAX_NUM;
    strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    strip_config.led_model = LED_MODEL_WS2812;
    led_strip_rmt_config_t rmt_config = {};
    rmt_config.resolution_hz = 10 * 1000 * 1000; // 10MHz

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip_));
    led_strip_clear(led_strip_);

    esp_timer_create_args_t blink_timer_args = {
        .callback = [](void *arg) {
            auto led = static_cast<LedEye*>(arg);
            led->OnBlinkTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "blink_timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&blink_timer_args, &blink_timer_));
}

LedEye::~LedEye() {
    esp_timer_stop(blink_timer_);
    if (led_strip_ != nullptr) {
        led_strip_del(led_strip_);
    }
}

void LedEye::SetSingleColor(uint8_t index,uint8_t red,uint8_t green,uint8_t blue) {
    cyc_blink_flag=0;
    colors[index].red=red;
    colors[index].green=green;
    colors[index].blue=blue;
}

void LedEye::SetAllColor(uint8_t red,uint8_t green,uint8_t blue) {
    cyc_blink_flag=0;
    for(uint8_t i=0;i<LED_MAX_NUM;i++){
        colors[i].red=red;
        colors[i].green=green;
        colors[i].blue=blue;
    }
}

void LedEye::TurnOn() {
    if (led_strip_ == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(blink_timer_);
    for(uint8_t i=0;i<LED_MAX_NUM;i++){
        led_strip_set_pixel(led_strip_, i, colors[i].red, colors[i].green, colors[i].blue);
    }
    led_strip_refresh(led_strip_);
}

void LedEye::TurnOff() {
    if (led_strip_ == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(blink_timer_);
    led_strip_clear(led_strip_);
    cyc_blink_flag=0;
}

void LedEye::BlinkOnce() {
    Blink(1, 100);
}

void LedEye::Blink(int times, int interval_ms) {
    StartBlinkTask(times, interval_ms);
}

void LedEye::StartContinuousBlink(int interval_ms) {
    StartBlinkTask(BLINK_INFINITE, interval_ms);
}

void LedEye::StartBlink(int interval_ms) {
    cyc_blink_flag=1;
    StartBlinkTask(BLINK_INFINITE, interval_ms);
}

void LedEye::StartBlinkTask(int times, int interval_ms) {
    if (led_strip_ == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    esp_timer_stop(blink_timer_);

    blink_counter_ = times * 2;
    blink_interval_ms_ = interval_ms;
    esp_timer_start_periodic(blink_timer_, interval_ms * 1000);
}

void LedEye::OnBlinkTimer() {
    std::lock_guard<std::mutex> lock(mutex_);
    blink_counter_--;
    if (blink_counter_ & 1) {
        for(uint8_t i=0;i<LED_MAX_NUM;i++){
            led_strip_set_pixel(led_strip_, i, colors[i].red, colors[i].green, colors[i].blue);
        }
        led_strip_refresh(led_strip_);
    } else {
        led_strip_clear(led_strip_);
        if (blink_counter_ == 0) {
            if(cyc_blink_flag){
                blink_counter_=BLINK_INFINITE*2;
            }
            else{
                esp_timer_stop(blink_timer_);
            }
        }
    }
}

void LedEye::OnStateChanged() {
    auto& app = Application::GetInstance();
    auto device_state = app.GetDeviceState();
    switch (device_state) {
        case kDeviceStateStarting:
            // R
            SetAllColor(DEFAULT_BRIGHTNESS, 0, 0);
            StartContinuousBlink(100);
            break;
        case kDeviceStateWifiConfiguring:
            SetAllColor(DEFAULT_BRIGHTNESS, 0, 0);
            break;
        case kDeviceStateIdle:
            TurnOff();
            break;
        case kDeviceStateConnecting:
            SetAllColor(DEFAULT_BRIGHTNESS, 0, 0);
            StartContinuousBlink(50);
            TurnOn();
            break;
        case kDeviceStateListening:
            //B
            SetAllColor(0, 0, HIGH_BRIGHTNESS);
            TurnOn();
            StartBlink(500);
            break;
        case kDeviceStateSpeaking:
            //G
            SetAllColor(0, HIGH_BRIGHTNESS, 0);
            TurnOn();
            StartBlink(500);
            break;
        case kDeviceStateUpgrading:
            SetAllColor(LOW_BRIGHTNESS, 0, 0);
            StartContinuousBlink(100);
            break;
        case kDeviceStateActivating:
            //R
            SetAllColor(LOW_BRIGHTNESS, 0, 0);
            StartContinuousBlink(100);
            break;
        default:
            ESP_LOGW(TAG, "Unknown led strip event: %d", device_state);
            return;
    }
}
