#include "circular_led_strip.h"
#include "application.h"
#include <esp_log.h>
#include <algorithm>
#include "ws2812_task.h"

#define TAG "CircularLedStrip"

#define DEFAULT_BRIGHTNESS 4
#define HIGH_BRIGHTNESS 16
#define LOW_BRIGHTNESS 2
#define BLINK_INFINITE -1
#define WAVE_EFFECT_INTERVAL_MS 100 // 律动效果间隔时间(毫秒)

CircularLedStrip::CircularLedStrip(gpio_num_t gpio) {
    assert(gpio != GPIO_NUM_NC);
    ws2812_start();
}

CircularLedStrip::~CircularLedStrip() {
 
}
 
 
 
 
void CircularLedStrip::OnStateChanged() {
    auto& app = Application::GetInstance();
    auto device_state = app.GetDeviceState();
    
    switch (device_state) {
        case kDeviceStateStarting:
            ws2812_set_mode(WS2812_MODE_BLINK_BLUE);
            break;
        case kDeviceStateWifiConfiguring:
            ws2812_set_mode(WS2812_MODE_BLINK_BLUE);
            break;
        case kDeviceStateIdle:
             ws2812_set_state(WS2812_STATE_IDLE);
            break;
        case kDeviceStateConnecting:
            ws2812_set_mode(WS2812_MODE_BLINK_BLUE);
            break;
        case kDeviceStateListening:
            if (app.IsVoiceDetected()) {
                ws2812_set_state(WS2812_STATE_LISTENING_VOICE);
            } else {
                ws2812_set_state(WS2812_STATE_LISTENING_NO_VOICE);
            }
            break;
        case kDeviceStateSpeaking:
            ws2812_set_state(WS2812_STATE_SPEAKING);
            break;
        case kDeviceStateUpgrading:
            ws2812_set_mode(WS2812_MODE_BLINK_GREEN);
            break;
        case kDeviceStateActivating:
            ws2812_set_mode(WS2812_MODE_BLINK_GREEN);
            break;
        default:
            ESP_LOGW(TAG, "Unknown led strip event: %d", device_state);
            break;
    }
}