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
// TextDisplay shows the use of the display instance reference as a function parameter

#ifndef _TextDisplay_H_
#define _TextDisplay_H_

#include "GxEPD2_GFX.h"

void helloWorld(GxEPD2_GFX& display);
void helloWorldForDummies(GxEPD2_GFX& display);
void helloFullScreenPartialMode(GxEPD2_GFX& display);
void helloArduino(GxEPD2_GFX& display);
void helloEpaper(GxEPD2_GFX& display);
void helloValue(GxEPD2_GFX& display, double v, int digits);
void showFont(GxEPD2_GFX& display, const char name[], const GFXfont* f);
void drawFont(GxEPD2_GFX& display, const char name[], const GFXfont* f);

#endif
