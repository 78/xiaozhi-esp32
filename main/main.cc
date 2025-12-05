// ===================== [wj] Start =====================
// @Author  : Wang Jian
// @Date    : 2025-09-26
// @Reason  : move Arduino.h to the first palce to avoid macro redefinition
#include <Arduino.h>
// ===================== [wj] End =====================
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
#include "ui/epd_renderer.h"
#include "board.h"
//===================[wj] Start===================
// @Author  : Wang Jian
// @Date    : 2025-11-3
// @Reason  : add includes for SPIFFS and SQLite3
#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"

extern "C" {
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_common.h"
#include "driver/sdspi_host.h"
}

#include "sqlite3.h"
//===================[wj] End======================

// ===================== [wj] Start =====================
// @Author  : Wang Jian
// @Date    : 2025-09-26
// @Reason  : add includes for EPD and GT30

#include "ui/epd_renderer.h"
#include "cat.h"
//==================== [wj] End =====================


#define TAG "main"

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
}





