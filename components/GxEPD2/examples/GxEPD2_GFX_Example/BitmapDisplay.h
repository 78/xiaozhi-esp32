// Display Library example for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Display Library based on Demo Example from Good Display: https://www.good-display.com/companyfile/32/
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2
//
// Purpose: show uses of GxEPD2_GFX base class for references to a display instance
//
// BitmapDisplay shows the use of the display instance reference as a class member

#ifndef _BitmapDisplay_H_
#define _BitmapDisplay_H_

#include "GxEPD2_GFX.h"

class BitmapDisplay
{
  private:
    GxEPD2_GFX& display;
  public:
    BitmapDisplay(GxEPD2_GFX& _display) : display(_display) {};
    void drawBitmaps();
  private:
    void drawBitmaps200x200();
    void drawBitmaps104x212();
    void drawBitmaps128x250();
    void drawBitmaps128x296();
    void drawBitmaps152x296();
    void drawBitmaps176x264();
    void drawBitmaps240x416();
    void drawBitmaps400x300();
    void drawBitmaps640x384();
    void drawBitmaps800x480();
    void drawBitmaps800x600();
    void drawBitmaps3c200x200();
    void drawBitmaps3c104x212();
    void drawBitmaps3c128x296();
    void drawBitmaps3c176x264();
    void drawBitmaps3c400x300();
    void drawBitmaps3c800x480();
};

#endif
