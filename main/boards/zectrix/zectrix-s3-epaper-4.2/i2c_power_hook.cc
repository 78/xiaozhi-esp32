#include <driver/gpio.h>

#include "config.h"

extern "C" void BoardI2cForcePowerOn() {
    const gpio_num_t pin = static_cast<gpio_num_t>(Audio_PWR_PIN);
    gpio_hold_dis(pin);
    gpio_set_level(pin, AUDIO_PWR_FORCE_LEVEL);
    gpio_hold_en(pin);
}
