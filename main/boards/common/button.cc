#include "button.h"

#include <esp_log.h>

static const char* TAG = "Button";  // 定义日志标签

// Button类的构造函数
Button::Button(gpio_num_t gpio_num, bool active_high) : gpio_num_(gpio_num) {
    // 如果GPIO号为GPIO_NUM_NC（未连接），则直接返回
    if (gpio_num == GPIO_NUM_NC) {
        return;
    }
    
    // 配置按钮参数
    button_config_t button_config = {
        .type = BUTTON_TYPE_GPIO,  // 按钮类型为GPIO
        .long_press_time = 1000,   // 长按时间为1000ms
        .short_press_time = 50,    // 短按时间为50ms
        .gpio_button_config = {
            .gpio_num = gpio_num,  // GPIO号
            .active_level = static_cast<uint8_t>(active_high ? 1 : 0)  // 有效电平（高电平或低电平）
        }
    };
    
    // 创建按钮句柄
    button_handle_ = iot_button_create(&button_config);
    if (button_handle_ == NULL) {
        ESP_LOGE(TAG, "Failed to create button handle");  // 如果创建失败，记录错误日志
        return;
    }
}

// Button类的析构函数
Button::~Button() {
    // 如果按钮句柄存在，则删除按钮句柄
    if (button_handle_ != NULL) {
        iot_button_delete(button_handle_);
    }
}

// 设置按下事件回调函数
void Button::OnPressDown(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_press_down_ = callback;  // 保存回调函数
    iot_button_register_cb(button_handle_, BUTTON_PRESS_DOWN, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);  // 获取Button对象
        if (button->on_press_down_) {
            button->on_press_down_();  // 调用回调函数
        }
    }, this);
}

// 设置释放事件回调函数
void Button::OnPressUp(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_press_up_ = callback;  // 保存回调函数
    iot_button_register_cb(button_handle_, BUTTON_PRESS_UP, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);  // 获取Button对象
        if (button->on_press_up_) {
            button->on_press_up_();  // 调用回调函数
        }
    }, this);
}

// 设置长按事件回调函数
void Button::OnLongPress(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_long_press_ = callback;  // 保存回调函数
    iot_button_register_cb(button_handle_, BUTTON_LONG_PRESS_START, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);  // 获取Button对象
        if (button->on_long_press_) {
            button->on_long_press_();  // 调用回调函数
        }
    }, this);
}

// 设置单击事件回调函数
void Button::OnClick(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_click_ = callback;  // 保存回调函数
    iot_button_register_cb(button_handle_, BUTTON_SINGLE_CLICK, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);  // 获取Button对象
        if (button->on_click_) {
            button->on_click_();  // 调用回调函数
        }
    }, this);
}

// 设置双击事件回调函数
void Button::OnDoubleClick(std::function<void()> callback) {
    if (button_handle_ == nullptr) {
        return;
    }
    on_double_click_ = callback;  // 保存回调函数
    iot_button_register_cb(button_handle_, BUTTON_DOUBLE_CLICK, [](void* handle, void* usr_data) {
        Button* button = static_cast<Button*>(usr_data);  // 获取Button对象
        if (button->on_double_click_) {
            button->on_double_click_();  // 调用回调函数
        }
    }, this);
}