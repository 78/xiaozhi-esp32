#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>

#include "Application.h"
#include "SystemInfo.h"
#include "SystemReset.h"

#define TAG "main"

extern "C" void app_main(void)
{
#ifdef CONFIG_AUDIO_CODEC_ES8311_ES7210
    // Make GPIO15 HIGH to enable the 4G module
    gpio_config_t ml307_enable_config = {
        .pin_bit_mask = (1ULL << 15),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&ml307_enable_config);
    gpio_set_level(GPIO_NUM_15, 1);
#endif

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

    // Otherwise, launch the application
    Application::GetInstance().Start();

    // Dump CPU usage every 10 second
    while (true) {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        // SystemInfo::PrintRealTimeStats(pdMS_TO_TICKS(1000));
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);
    }
}
