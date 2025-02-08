#ifndef BUTTON_H_
#define BUTTON_H_

#include <driver/gpio.h>
#include <iot_button.h>
#include <functional>

class Button {
public:
    Button(gpio_num_t gpio_num, bool active_high = false, uint16_t short_press_time = 50);
    ~Button();

    void OnPressDown(std::function<void()> callback);
    void OnPressUp(std::function<void()> callback);
    void OnLongPress(std::function<void()> callback);
    void OnClick(std::function<void()> callback);
    void OnDoubleClick(std::function<void()> callback);
    void OnThreeClick(std::function<void()> callback);
    void OnFourClick(std::function<void()> callback);
    int getButtonLevel() const;
private:
    gpio_num_t gpio_num_;
    button_handle_t button_handle_ = nullptr;


    std::function<void()> on_press_down_;
    std::function<void()> on_press_up_;
    std::function<void()> on_long_press_;
    std::function<void()> on_click_;
    std::function<void()> on_double_click_;
    std::function<void()> on_three_click_;
    std::function<void()> on_four_click_;
};

#endif // BUTTON_H_
