#ifndef BUTTON_H_
#define BUTTON_H_

#include <driver/gpio.h>
#include <iot_button.h>
#include <functional>

class Button {
public:
    Button(gpio_num_t gpio_num);
    ~Button();

    void OnPress(std::function<void()> callback);
    void OnLongPress(std::function<void()> callback);
    void OnClick(std::function<void()> callback);
    void OnDoubleClick(std::function<void()> callback);
private:
    gpio_num_t gpio_num_;
    button_handle_t button_handle_;


    std::function<void()> on_press_;
    std::function<void()> on_long_press_;
    std::function<void()> on_click_;
    std::function<void()> on_double_click_;
};

#endif // BUTTON_H_
