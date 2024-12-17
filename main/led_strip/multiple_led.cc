#include "multiple_led.h"
#include <esp_log.h>

#define TAG "MultipleLed"

MultipleLed::MultipleLed(gpio_num_t gpio, uint8_t max_leds) : LedStripWrapper(gpio, max_leds) {
}

MultipleLed::~MultipleLed() {
}

void MultipleLed::LightOn(LedStripEvent event) {
    switch (event) {
        case kStartup:
            ChasingLight(kLedColorWhite, kLedColorBlue, 100, HIGH_BRIGHTNESS);
            break;
        case kListeningAndSpeaking:
            BreathLight(kLedColorRed, 100);
            break;
        case kListening:
            BreathLight(kLedColorRed, 100);
            break;
        case kSpeaking:
            StaticLight(kLedColorGreen, HIGH_BRIGHTNESS);
            break;
        case kStandby:
            BlinkOnce(kLedColorGreen);
            break;
        case kConnecting:
            Blink(kLedColorBlue, 1000, 500);
            break;
        case kUpgrading:
            ContinuousBlink(kLedColorGreen, 100);
            break;
        default:
            ESP_LOGE(TAG, "Invalid led strip event: %d", event);
            return;
    }
}
