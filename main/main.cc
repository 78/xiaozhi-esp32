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
//===================== [wj] Start =====================
// @Author  : Wang Jian
// @Date    : 2025-09-26
// @Reason  : add EPD and GT30 initialization code
        // Initialize EPD and GT30 via EpdRenderer helper. If EPD not available this is a no-op.
        EpdRenderer::Init();
        if (EpdRenderer::Available()) {
            EpdRenderer::FillAndDraw("北国风光，千里冰封,万里雪飘。", 0, 20);
            EpdRenderer::Draw("望长城内外，惟余莽莽;大河上下,顿失滔滔。", 0, 40);
            EpdRenderer::Draw("山舞银蛇，原驰蜡象,欲与天公试比高。", 0, 60);
            EpdRenderer::Draw("my name is mao ze dong！", 0, 80);
            EpdRenderer::Draw("This is my 沁园春雪？", 0, 100);
            // TODO: drawBitmap of embedded image via EpdRenderer when needed
            EpdRenderer::Display(true);
        }

//===================== [wj] End =====================

    
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
