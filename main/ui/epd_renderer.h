#pragma once
#include <string>

namespace EpdRenderer {
    // Returns true if native DrawMixedString-based EPD rendering is available
    bool Available();
    // Fill screen and draw UTF8 text at x,y (coordinates in pixels)
    void FillAndDraw(const std::string &utf8, int x, int y);
    // Draw text onto existing buffer (no clear)
    void Draw(const std::string &utf8, int x, int y);
    // Do a partial update of the display (true => partial)
    void Display(bool partial=true);
    // Clear the whole screen (white)
    void Clear();
    // Initialize EPD hardware (GT30 OCR and display). Safe to call even if EPD not available.
    void Init();
}
