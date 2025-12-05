#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "system_info.h"

#define TAG "main"

void task_monitor(void *pvParameters)
{
    const int max_tasks = 30;
    TaskStatus_t task_list[max_tasks];
    UBaseType_t task_count;
    uint32_t total_runtime;

    while (1)
    {
        task_count = uxTaskGetSystemState(task_list, max_tasks, &total_runtime);

        ESP_LOGI(TAG, "----- Task Stack Monitor -----");

        for (int i = 0; i < task_count; i++)
        {
            UBaseType_t hw = task_list[i].usStackHighWaterMark;  // đơn vị: words (4 bytes)
#if ( configTASKLIST_INCLUDE_COREID == 1 )
            ESP_LOGI(TAG,
                     "%-16s | Free: %4u bytes | Prio: %2u | Core: %u",
                     task_list[i].pcTaskName,
                     hw,
                     task_list[i].uxCurrentPriority,
                     task_list[i].xCoreID
            );
#else
            ESP_LOGI(TAG,
                     "%-16s | Free: %4u bytes | Prio: %2u",
                     task_list[i].pcTaskName,
                     hw,
                     task_list[i].uxCurrentPriority
            );
#endif
        }

        ESP_LOGI(TAG, "--------------------------------");

        vTaskDelay(pdMS_TO_TICKS(5000));  // 5s
    }
}

extern "C" void app_main(void)
{
    // Initialize the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Launch the application
    auto& app = Application::GetInstance();
    app.Start();

    // xTaskCreatePinnedToCore(task_monitor, "TaskMonitor", 1024 * 3, NULL, 1, NULL, tskNO_AFFINITY);
}
