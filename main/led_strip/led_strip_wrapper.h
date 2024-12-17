#ifndef _LED_STRIP_WRAPPER_H_
#define _LED_STRIP_WRAPPER_H_

#include "led.h"

#define COUNTER_INFINITE -1

enum LedStripEvent {
    kStartup,
    kListening,
    kListeningAndSpeaking,
    kSpeaking,
    kStandby,
    kConnecting,
    kUpgrading,
};

enum LedBasicColor {
    kLedColorWhite,
    kLedColorGrey,
    kLedColorRed,
    kLedColorGreen,
    kLedColorBlue,
};

typedef std::function<void()> TimerCallback;

class LedStripWrapper {
private:
    Led* led_ = nullptr;
    std::mutex mutex_;
    
    uint32_t counter_ = 0;
    esp_timer_handle_t led_strip_timer_ = nullptr;
    TimerCallback timer_callback_;

    void SetLedBasicColor(LedBasicColor color, uint8_t brightness);
    void SetLedStripBasicColor(uint8_t index, LedBasicColor color, uint8_t brightness = DEFAULT_BRIGHTNESS);
    void StartBlinkTask(uint32_t times, uint32_t interval_ms);
    void OnBlinkTimer();

public:
    LedStripWrapper(gpio_num_t gpio, uint8_t max_leds);
    virtual ~LedStripWrapper();

    void LightOff();
    virtual void LightOn(LedStripEvent event) = 0;

protected:
    void BlinkOnce(LedBasicColor color, uint8_t brightness = DEFAULT_BRIGHTNESS);
    void Blink(LedBasicColor color, uint32_t times, uint32_t interval_ms, uint8_t brightness = DEFAULT_BRIGHTNESS);
    void ContinuousBlink(LedBasicColor color, uint32_t interval_ms, uint8_t brightness = DEFAULT_BRIGHTNESS);
    void StaticLight(LedBasicColor color, uint8_t brightness = DEFAULT_BRIGHTNESS);
    void ChasingLight(LedBasicColor base_color, LedBasicColor color, uint32_t interval_ms, uint8_t brightness = DEFAULT_BRIGHTNESS);
    void BreathLight(LedBasicColor color, uint32_t interval_ms);
};

#endif // _LED_STRIP_WRAPPER_H_
