#include "ui/epd_renderer.h"
#include <Arduino.h>
#include "esp_log.h"
#include "display.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "EpdRenderer";

// Use C wrappers from DrawMixedString.cc so this TU doesn't need GxEPD2 headers/macros.
extern "C" void drawMixedString_init();
extern "C" void drawMixedString_fillScreen(int color);
extern "C" void drawMixedString_drawText(const char* utf8, int x, int y);
extern "C" void drawMixedString_display(bool partial);
extern "C" void drawMixedString_drawBitmap(int x, int y, const uint8_t* data, int w, int h, int color);
extern "C" void drawMixedString_displayWindow(int x, int y, int w, int h, bool partial);
extern "C" int drawMixedString_width();
extern "C" int drawMixedString_height();
extern "C" void drawMixedString_selectFastFullUpdate(bool enable);
extern "C" void drawMixedString_firstPage();
extern "C" bool drawMixedString_nextPage();
extern "C" void drawMixedString_setCursor(int x, int y);
extern "C" void drawMixedString_print(const char* s);
extern "C" void drawMixedString_setPartialWindow(int x, int y, int w, int h);

namespace EpdRenderer {
    bool Available() {
        return 1;
    }
    void Init() {
        static bool inited = false;
        if (inited) return;
        // Initialize using the centralized wrapper in DrawMixedString.cc. That TU
        // includes GxEPD2 headers and performs SPI/pin initialization.
        ESP_LOGI(TAG, "EpdRenderer::Init() - calling drawMixedString_init");
        drawMixedString_init();
        inited = true;
        ESP_LOGI(TAG, "EpdRenderer::Init() - drawMixedString_init returned; Display dims: w=%d h=%d", drawMixedString_width(), drawMixedString_height());
        // fill white (literal value avoids depending on GxEPD2 macros here)
        drawMixedString_fillScreen(0xFFFF);
    }
    void Clear() {
        drawMixedString_fillScreen(0xFFFF);
    }

    void DrawText(const std::string &utf8, int x, int y) {
        drawMixedString_drawText(utf8.c_str(), x, y);
    }
    void DrawBitmap(const uint8_t* data, int x, int y, int w, int h, int color) {

            drawMixedString_drawBitmap(x, y, data, w, h, color);
    }
    void DisplayWindow(int x, int y, int w, int h, bool partial) {
            drawMixedString_displayWindow(x, y, w, h, partial);
    }

    void setPartialWindow(int x, int y, int w, int h) {
            drawMixedString_setPartialWindow(x, y, w, h);
        
    }

    //快速刷新 true 慢速刷新 false
    void Display(bool partial) {
        drawMixedString_display(partial);
    }

    void SelectFastFullUpdate(bool enable) {
        ESP_LOGI(TAG, "EpdRenderer::SelectFastFullUpdate(%d)", enable);
        drawMixedString_selectFastFullUpdate(enable);
    }

    void FirstPage() {

            drawMixedString_firstPage();
        
    }

    bool NextPage() {

      return  drawMixedString_nextPage();
      
    }

    void SetCursor(int x, int y) {

            drawMixedString_setCursor(x, y);
        
    }

    void Print(const std::string &s) {

            drawMixedString_print(s.c_str());
}


}