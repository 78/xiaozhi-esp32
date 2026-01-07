#ifndef EC11_ENCODER_H
#define EC11_ENCODER_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <functional>

class Ec11Encoder {
public:
    Ec11Encoder(gpio_num_t pin_a, gpio_num_t pin_b);
    ~Ec11Encoder();

    void Start();
    void SetCallback(std::function<void(int)> callback);

private:
    static void IRAM_ATTR gpio_isr_handler(void* arg);
    static void ec11_encoder_task(void* arg);

    gpio_num_t pin_a_;
    gpio_num_t pin_b_;
    SemaphoreHandle_t xSemaphore_ = NULL;
    int8_t last_state_ = 0;
    int16_t encoder_position_ = 0;
    int64_t last_isr_time_ = 0;
    std::function<void(int)> callback_;
    TaskHandle_t task_handle_ = NULL;
};

#endif // EC11_ENCODER_H