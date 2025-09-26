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

#include <GxEPD2_BW.h>
#include "DrawMixedString.h"
#include "cat.h"
//==================== [wj] End =====================

#define TAG "main"

extern "C" void app_main(void)
{
//===================== [wj] Start =====================
// @Author  : Wang Jian
// @Date    : 2025-09-26
// @Reason  : add EPD and GT30 initialization code
        initArduino();
        //initialization EPD SPI bus SCK MISO MOSI 
        SPI.begin(SPI_PIN_NUM_CLK,SPI_NUM_MISO,SPI_PIN_NUM_MOSI);
 
        pinMode(EPD_PIN_NUM_CS, OUTPUT);
        pinMode(EPD_PIN_NUM_DC, OUTPUT);
        pinMode(EPD_PIN_NUM_RST, OUTPUT);
        pinMode(EPD_PIN_NUM_BUSY, INPUT);
        pinMode(GT30_PIN_NUM_CS, OUTPUT);
        //GT30 Initialization
        uint8_t rt = gt30_init();
        ESP_LOGW(TAG, "GT30 INIT, ret=%d", rt);
        display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
        ESP_LOGW(TAG, "1:Display.init");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        display.fillScreen(GxEPD_WHITE); // white background
        ESP_LOGW(TAG, "2:fillScreen");
        display.setRotation(0);//LANDSCAPE); //PORTRAIT
        display.firstPage();
    do {
        drawBitmapMixedString("北国风光，千里冰封,万里雪飘。", 0, 20);
        drawBitmapMixedString("望长城内外，惟余莽莽;大河上下,顿失滔滔。", 0, 40);
        drawBitmapMixedString("山舞银蛇，原驰蜡象,欲与天公试比高。", 0, 60);
        drawBitmapMixedString("my name is mao ze dong！", 0, 80);
        drawBitmapMixedString("This is my 沁园春雪？", 0, 100);
        display.drawBitmap(100, 80, gImage_mzd, 200, 200, GxEPD_BLACK);
    } while (display.nextPage());

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
