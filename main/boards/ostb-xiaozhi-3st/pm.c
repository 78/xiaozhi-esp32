
#include "esp_wifi.h"
#include "config.h"
#include "esp_freertos_hooks.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/rtc_io.h"

// Note: Only specific GPIOs can be used as deep sleep wake-up sources on the ESP32: 0, 2, 4, 12-15, 25-27, 32-39
static const char *TAG = "PM";

void pm_wakeup(void)
{
    // Check if wakeup was caused by Deep-Sleep
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGI(TAG, "Wake up from EXT0 (GPIO%d)", WAKEUP_BUTTON_GPIO);
    } else if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) {
        ESP_LOGI(TAG, "Wake up from EXT1");
    } else {
        ESP_LOGI(TAG, "Power-on or reset");
    }
}

void pm_low_power_shutdown(void)
{  
    // Configure RTC GPIO as wakeup source
    rtc_gpio_init(WAKEUP_BUTTON_GPIO);
    rtc_gpio_set_direction(WAKEUP_BUTTON_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(WAKEUP_BUTTON_GPIO);
    rtc_gpio_pulldown_dis(WAKEUP_BUTTON_GPIO);

    esp_sleep_enable_ext0_wakeup(WAKEUP_BUTTON_GPIO, 0);
    
    ESP_LOGI(TAG, "Entering deep sleep, will wake up on GPIO%d low level", WAKEUP_BUTTON_GPIO);
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_deep_sleep_start(); // Enter Deep-Sleep
}
