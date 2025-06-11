#pragma once
#include <mutex>
#include <inttypes.h>
#include <driver/gpio.h>
#include <esp_log.h>

class GpioManager {
public:
    enum class GpioMode {
        INPUT,
        OUTPUT,
        INPUT_PULLUP,
        INPUT_PULLDOWN
    };

    static void SetLevel(gpio_num_t gpio, uint32_t level) {
        std::lock_guard<std::mutex> lock(mutex_);
        ESP_ERROR_CHECK(gpio_set_level(gpio, level));
        ESP_LOGD("GpioManager", "Set GPIO %d level: %d", static_cast<int>(gpio), static_cast<int>(level));
    }

    static int GetLevel(gpio_num_t gpio) {
        std::lock_guard<std::mutex> lock(mutex_);
        int level = gpio_get_level(gpio);
        ESP_LOGD("GpioManager", "Get GPIO %d level: %d", static_cast<int>(gpio), level);
        return level;
    }

    static void Config(gpio_num_t gpio, GpioMode mode) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        gpio_config_t config = {};
        config.pin_bit_mask = (1ULL << gpio);
        
        switch(mode) {
            case GpioMode::INPUT:
                config.mode = GPIO_MODE_INPUT;
                config.pull_up_en = GPIO_PULLUP_DISABLE;
                config.pull_down_en = GPIO_PULLDOWN_DISABLE;
                break;
            case GpioMode::OUTPUT:
                config.mode = GPIO_MODE_OUTPUT;
                break;
            case GpioMode::INPUT_PULLUP:
                config.mode = GPIO_MODE_INPUT;
                config.pull_up_en = GPIO_PULLUP_ENABLE;
                break;
            case GpioMode::INPUT_PULLDOWN:
                config.mode = GPIO_MODE_INPUT;
                config.pull_down_en = GPIO_PULLDOWN_ENABLE;
                break;
        }
        
        ESP_ERROR_CHECK(gpio_config(&config));
        ESP_LOGI("GpioManager", "Configured GPIO %d mode: %d", gpio, static_cast<int>(mode));
    }

private:
    static std::mutex mutex_;
};

std::mutex GpioManager::mutex_;