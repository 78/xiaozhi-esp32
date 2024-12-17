#include "single_led.h"
#include <esp_log.h> 

#define TAG "SingleLed"

SingleLed::SingleLed(gpio_num_t gpio) : LedStripWrapper(gpio, 1) {
}

SingleLed::~SingleLed() {
}

void SingleLed::LightOn(LedStripEvent event) {
    switch (event) {
        case kStartup:
            ContinuousBlink(kLedColorBlue, 100);
            break;
        case kListeningAndSpeaking:
            StaticLight(kLedColorRed, HIGH_BRIGHTNESS);
            break;
        case kListening:
            StaticLight(kLedColorRed, LOW_BRIGHTNESS);
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
