#pragma once
#include <string>

namespace EpdRenderer {
    // Returns true if native DrawMixedString-based EPD rendering is available
    bool Available();

    // Draw text onto existing buffer (no clear)
    void DrawText(const std::string &utf8, int x, int y);
    // Draw a bitmap into the buffer (no refresh)
    void DrawBitmap(const uint8_t* data, int x, int y, int w, int h, int color);
    // Refresh a specific window on the display
    void DisplayWindow(int x, int y, int w, int h, bool partial = true);
    // Do a partial update of the display (true => partial)
    void Display(bool partial=true);
    // Set the partial window for paged drawing (mirrors GxEPD2::setPartialWindow)
    void setPartialWindow(int x, int y, int w, int h);

    // Clear the whole screen (white)
    void Clear();
    // Initialize EPD hardware (GT30 OCR and display). Safe to call even if EPD not available.
    void Init();
    // Enable or disable selectFastFullUpdate on underlying panel (if supported)
    void SelectFastFullUpdate(bool enable);
    // Paged drawing APIs mirroring GxEPD2:
    void FirstPage();
    bool NextPage();
    void SetCursor(int x, int y);
    void Print(const std::string &s);
}
