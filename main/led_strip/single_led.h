#ifndef _LED_STRIP_EFFECT_V1_H_
#define _LED_STRIP_EFFECT_V1_H_

#include "led_strip_wrapper.h"

class SingleLed : public LedStripWrapper {
public:
    SingleLed(gpio_num_t gpio);
    virtual ~SingleLed();

    void LightOn(LedStripEvent event) override;
};

#endif // _LED_STRIP_EFFECT_V1_H_
