#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>

#include "application.h"
#include "system_info.h"

#define TAG "main"


esp_err_t tfcard_ret = ESP_FAIL;

extern "C" void app_main(void)
{
    // Initialize the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    tfcard_ret = fm_sdcard_init();

    xTaskCreatePinnedToCore(&esp_lvgl_adapter_init, "esp_lvgl_adapter_init task", 1024 * 5, NULL, 15, NULL, 1);
    vTaskDelay(1000);
    // label_ask_set_text("可以唤醒我啦");
    Application::GetInstance().Start();
    // if (tfcard_ret == ESP_OK)
    // {
    //     while (1)
    //     {
    //         switch (biaoqing)
    //         {
    //         case 0:
    //             play_change(FACE_STATIC);
    //             break;

    //         case 1:
    //             play_change(FACE_HAPPY);

    //             break;
    //         case 2:
    //             play_change(FACE_ANGRY);

    //             break;
    //         case 3:
    //             play_change(FACE_BAD);

    //             break;
    //         case 4:
    //             play_change(FACE_FEAR);

    //             break;
    //         case 5:
    //             play_change(FACE_NOGOOD);

    //             break;
    //         default:
    //             break;
    //         }
    //         biaoqing = 0;
    //         vTaskDelay(100 / portTICK_PERIOD_MS);
    //     }
    // }
    // Dump CPU usage every 10 second
    while (true)
    {

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        // SystemInfo::PrintRealTimeStats(pdMS_TO_TICKS(1000));
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "Free internal: %u minimal internal: %u", free_sram, min_free_sram);
    }
}
