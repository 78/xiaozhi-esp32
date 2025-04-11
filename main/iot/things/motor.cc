/*
 * @Date: 2025-03-11 21:34:00
 * @LastEditors: zhouke
 * @LastEditTime: 2025-03-11 23:15:11
 * @FilePath: \xiaozhi-esp32\main\iot\things\motor.cc
 */
#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <driver/gpio.h>
#include <esp_log.h>

#define TAG "Motor"

namespace iot {

// 这里仅定义 Lamp 的属性和方法，不包含具体的实现
class Motor : public Thing {
private:
    gpio_num_t gpio_num1_ = GPIO_NUM_3;
    gpio_num_t gpio_num2_ = GPIO_NUM_10;
    bool power1_ = false;
    bool power2_ = false;

    void InitializeGpio() {

        gpio_config_t config1 = {
            .pin_bit_mask =  (1ULL << gpio_num1_) ,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };

        gpio_config_t config2 = {
            .pin_bit_mask =  (1ULL << gpio_num2_) ,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config1));
        ESP_ERROR_CHECK(gpio_config(&config2));
        gpio_set_level(gpio_num1_, 0);
        gpio_set_level(gpio_num2_, 0);
    }

public:
    Motor() : Thing("Motor", "一组电机,包括电机1和电机2,可分别控制开关") {
        InitializeGpio();

        // 定义设备的属性
        properties_.AddBooleanProperty("power1", "电机1是否打开", [this]() -> bool {
            return power1_;
        });

        properties_.AddBooleanProperty("power2", "电机2是否打开", [this]() -> bool {
            return power2_;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("TurnOn1", "打开电机1", ParameterList(), [this](const ParameterList& parameters) {
            power1_ = true;
            gpio_set_level(gpio_num1_, 1);
            ESP_LOGI(TAG, "打开电机1");
        });

        methods_.AddMethod("TurnOn2", "打开电机2", ParameterList(), [this](const ParameterList& parameters) {
            power2_ = true;
            gpio_set_level(gpio_num2_, 1);
            ESP_LOGI(TAG, "打开电机2");
        });


        methods_.AddMethod("TurnOff1", "关闭电机1", ParameterList(), [this](const ParameterList& parameters) {
            power1_ = false;
            gpio_set_level(gpio_num1_, 0);
            ESP_LOGI(TAG, "关闭电机1");
        });

        methods_.AddMethod("TurnOff2", "关闭电机2", ParameterList(), [this](const ParameterList& parameters) {
            power2_ = false;
            gpio_set_level(gpio_num2_, 0);
            ESP_LOGI(TAG, "关闭电机2");
        });
    }
};

} // namespace iot

DECLARE_THING(Motor);
