#include "gpio_manager.h"
#include <esp_log.h>

#define TAG "GpioManager"

GpioManager& GpioManager::GetInstance() {
    static GpioManager instance;
    return instance;
}

bool GpioManager::Initialize() {
    if (initialized_) {
        ESP_LOGW(TAG, "GpioManager already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing GpioManager...");
    
    // 初始化GPIO输出
    gpio_config_t io_conf = {};

    // LED灯环控制
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED_RING_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // 状态指示灯
    io_conf.pin_bit_mask = (1ULL << STATUS_LED_GPIO);
    gpio_config(&io_conf);

    initialized_ = true;
    ESP_LOGI(TAG, "GpioManager initialized successfully");
    return true;
}

void GpioManager::SetLedRing(bool state) {
    if (!initialized_) {
        ESP_LOGE(TAG, "GpioManager not initialized");
        return;
    }
    gpio_set_level(LED_RING_GPIO, state ? 1 : 0);
}

void GpioManager::SetStatusLed(bool state) {
    if (!initialized_) {
        ESP_LOGE(TAG, "GpioManager not initialized");
        return;
    }
    gpio_set_level(STATUS_LED_GPIO, state ? 1 : 0);
}
