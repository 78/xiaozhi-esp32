#include <cstdio>

#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "WifiConfigurationAp.h"
#include "Application.h"
#include "SystemInfo.h"
#include "SystemReset.h"
#include "BuiltinLed.h"
#include "WifiStation.h"

#define TAG "main"
#define STATS_TICKS         pdMS_TO_TICKS(1000)

extern "C" void app_main(void)
{
    // Check if the reset button is pressed
    SystemReset system_reset;
    system_reset.CheckButtons();

    // Initialize the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Try to connect to WiFi, if failed, launch the WiFi configuration AP
    auto& builtin_led = BuiltinLed::GetInstance();
    auto& wifi_station = WifiStation::GetInstance();
    builtin_led.SetBlue();
    builtin_led.StartContinuousBlink(100);
    wifi_station.Start();
    if (!wifi_station.IsConnected()) {
        builtin_led.SetBlue();
        builtin_led.Blink(1000, 500);
        WifiConfigurationAp::GetInstance().Start("Xiaozhi");
        return;
    }

    // Otherwise, launch the application
    Application::GetInstance().Start();

    // Dump CPU usage every 10 second
    while (true) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        // SystemInfo::PrintRealTimeStats(STATS_TICKS);
        int free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free heap size: %u minimal internal: %u", SystemInfo::GetFreeHeapSize(), free_sram);
    }
}
