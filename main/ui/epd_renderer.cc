#include "ui/epd_renderer.h"
#include <Arduino.h>
#include "esp_log.h"

static const char* TAG = "EpdRenderer";

// Use C wrappers from DrawMixedString.cc so this TU doesn't need GxEPD2 headers/macros.
extern "C" void drawMixedString_init();
extern "C" void drawMixedString_fillScreen(int color);
extern "C" void drawMixedString_drawText(const char* utf8, int x, int y);
extern "C" void drawMixedString_display(bool partial);

namespace EpdRenderer {
    bool Available() { return true; }
    void Init() {
        // Initialize using the centralized wrapper in DrawMixedString.cc. That TU
        // includes GxEPD2 headers and performs SPI/pin initialization.
        drawMixedString_init();
        // fill white (literal value avoids depending on GxEPD2 macros here)
        drawMixedString_fillScreen(0xFFFF);
    }
    void Clear() {
        drawMixedString_fillScreen(0xFFFF);
    }
    void FillAndDraw(const std::string &utf8, int x, int y) {
        drawMixedString_drawText(utf8.c_str(), x, y);
    }
    void Draw(const std::string &utf8, int x, int y) {
        drawMixedString_drawText(utf8.c_str(), x, y);
    }
    void Display(bool partial) {
        drawMixedString_display(partial);
    }
}
