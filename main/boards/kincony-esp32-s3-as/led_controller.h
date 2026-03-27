#ifndef _KINCONY_LED_CONTROLLER_H_
#define _KINCONY_LED_CONTROLLER_H_

#include "led/led.h"
#include "led/circular_strip.h"
#include <vector>

class KinconyLedController : public Led {
public:
    KinconyLedController(gpio_num_t bottom_gpio, gpio_num_t vertical_gpio);
    ~KinconyLedController();

    void OnStateChanged() override;
    void ShowRainbow();
    void TurnOff();

private:
    CircularStrip* bottom_strip_;
    CircularStrip* vertical_strip_;
    TaskHandle_t rainbow_task_ = nullptr;

    void RainbowTask();
    static void RainbowTaskWrapper(void* arg);
};

#endif // _KINCONY_LED_CONTROLLER_H_