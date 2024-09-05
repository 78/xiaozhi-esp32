#include <cstdio>

#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "WifiConfigurationAp.h"
#include "Application.h"
#include "SystemInfo.h"

#define TAG "main"
#define STATS_TICKS         pdMS_TO_TICKS(1000)

extern "C" void app_main(void)
{
    // Initialize the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Configure GPIO1 as INPUT, reset NVS flash if the button is pressed
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = 1ULL << 1;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    if (gpio_get_level(GPIO_NUM_1) == 0) {
        ESP_LOGI(TAG, "Button is pressed, reset NVS flash");
        nvs_flash_erase();
    }

    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Get the WiFi configuration
    nvs_handle_t nvs_handle;
    ret = nvs_open("wifi", NVS_READONLY, &nvs_handle);

    // If the WiFi configuration is not found, launch the WiFi configuration AP
    if (ret != ESP_OK) {
        auto app = new WifiConfigurationAp();
        app->Start();
        return;
    }
    nvs_close(nvs_handle);
    
    // Otherwise, launch the application
    auto app = new Application();
    app->Start();

    // Dump CPU usage every 10 second
    while (true) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        // SystemInfo::PrintRealTimeStats(STATS_TICKS);
        int free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free heap size: %u minimal internal: %u", SystemInfo::GetFreeHeapSize(), free_sram);
    }
}
