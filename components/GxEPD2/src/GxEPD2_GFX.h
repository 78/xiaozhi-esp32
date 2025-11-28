// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Display Library based on Demo Example from Good Display: https://www.good-display.com/companyfile/32/
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#ifndef _GxEPD2_GFX_H_
#define _GxEPD2_GFX_H_

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
//#include <GFX.h>

#if defined(_GFX_H_)
#define GxEPD2_GFX_ROOT_CLASS GFX
#else
#include <Adafruit_GFX.h>
#define GxEPD2_GFX_ROOT_CLASS Adafruit_GFX
#endif

#include <GxEPD2_EPD.h>

class GxEPD2_GFX : public GxEPD2_GFX_ROOT_CLASS
{
  public:
    GxEPD2_GFX(GxEPD2_EPD& _epd2, int16_t w, int16_t h) : GxEPD2_GFX_ROOT_CLASS(w, h), epd2(_epd2) {};
    virtual uint16_t pages() = 0;
    virtual uint16_t pageHeight() = 0;
    virtual bool mirror(bool m) = 0;
    virtual void init(uint32_t serial_diag_bitrate = 0) = 0; // serial_diag_bitrate = 0 : disabled
    // init method with additional parameters:
    // initial false for re-init after processor deep sleep wake up, if display power supply was kept
    // this can be used to avoid the repeated initial full refresh on displays with fast partial update
    // NOTE: garbage will result on fast partial update displays, if initial full update is omitted after power loss
    // pulldown_rst_mode true for alternate RST handling to avoid feeding 5V through RST pin
    virtual void init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration = 20, bool pulldown_rst_mode = false) = 0;
    virtual void fillScreen(uint16_t color) = 0; // 0x0 black, >0x0 white, to buffer
    // display buffer content to screen, useful for full screen buffer
    virtual void display(bool partial_update_mode = false) = 0;
    // display part of buffer content to screen, useful for full screen buffer
    // displayWindow, use parameters according to actual rotation.
    // x and w should be multiple of 8, for rotation 0 or 2,
    // y and h should be multiple of 8, for rotation 1 or 3,
    // else window is increased as needed,
    // this is an addressing limitation of the e-paper controllers
    virtual void displayWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) = 0;
    virtual void setFullWindow() = 0;
    // setPartialWindow, use parameters according to actual rotation.
    // x and w should be multiple of 8, for rotation 0 or 2,
    // y and h should be multiple of 8, for rotation 1 or 3,
    // else window is increased as needed, 
    // this is an addressing limitation of the e-paper controllers
    virtual void setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) = 0;
    virtual void firstPage() = 0;
    virtual bool nextPage() = 0;
    virtual void drawPaged(void (*drawCallback)(const void*), const void* pv) = 0;
    virtual void drawInvertedBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color) = 0;
    //  Support for Bitmaps (Sprites) to Controller Buffer and to Screen
    virtual void clearScreen(uint8_t value = 0xFF) = 0; // init controller memory and screen (default white)
    virtual void writeScreenBuffer(uint8_t value = 0xFF) = 0; // init controller memory (default white)
    // write to controller memory, without screen refresh; x and w should be multiple of 8
    virtual void writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    virtual void writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    virtual void writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm) = 0;
    virtual void writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h) = 0; // default options false
    virtual void writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm) = 0;
    virtual void writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h) = 0; // default options false
    // write sprite of native data to controller memory, without screen refresh; x and w should be multiple of 8
    virtual void writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    // write to controller memory, with screen refresh; x and w should be multiple of 8
    virtual void drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    virtual void drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    virtual void drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm) = 0;
    virtual void drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h) = 0; // default options false
    virtual void drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm) = 0;
    virtual void drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h) = 0; // default options false
    // write sprite of native data to controller memory, with screen refresh; x and w should be multiple of 8
    virtual void drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    virtual void refresh(bool partial_update_mode = false) = 0; // screen refresh from controller memory to full screen
    virtual void refresh(int16_t x, int16_t y, int16_t w, int16_t h) = 0; // screen refresh from controller memory, partial screen
    virtual void powerOff() = 0; // turns off generation of panel driving voltages, avoids screen fading over time
    virtual void hibernate() = 0; // turns powerOff() and sets controller to deep sleep for minimum power use, ONLY if wakeable by RST (rst >= 0)
  public:
    GxEPD2_EPD& epd2;
};

#endif
