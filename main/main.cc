#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>

#include "Application.h"
#include "SystemInfo.h"
#include "SystemReset.h"
#include "lvgl.h"
#include "lv_gui.h"
#include "esp_lcd_touch_cst816s.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_ops.h"
#include "esp_io_expander_tca9554.h"
#include "esp_freertos_hooks.h"
#include "avi_player.h"
#include "lv_demos.h"
#include "esp_netif.h"
#include "file_manager.h"
#include "my_esp_lvgl_port.h"

#define TAG "main"

#define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554A (ESP_IO_EXPANDER_I2C_TCA9554A_ADDRESS_000)
#define BSP_IO_EXPANDER_I2C_ADDRESS_TCA9554 (ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000)

#define I2C_SCL_IO (GPIO_NUM_18)
#define I2C_SDA_IO (GPIO_NUM_17)
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

    // tfcard_ret = fm_sdcard_init();

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
