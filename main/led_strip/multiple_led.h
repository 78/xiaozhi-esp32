#ifndef _LED_STRIP_EFFECT_V2_H_
#define _LED_STRIP_EFFECT_V2_H_

#include "led_strip_wrapper.h"

class MultipleLed : public LedStripWrapper {
public:
    MultipleLed(gpio_num_t gpio, uint8_t max_leds);
    virtual ~MultipleLed();

    void LightOn(LedStripEvent event) override;
};

#endif // _LED_STRIP_EFFECT_V2_H_
