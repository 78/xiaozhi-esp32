// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: these e-papers require 3.3V supply AND data lines!
//
// based on Demo Example from Good Display: http://www.e-paper-display.com/download_list/downloadcategoryid=34&isMode=false.html
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2
//
// This is a modified class GxEPD2_EPD that allows to use SW SPI with GxEPD2 and to read from DIN pin
// read the README.MD or README.txt
//
// To use SW SPI with GxEPD2:
// - copy the files GxEPD2_EPD.h and GxEPD2_EPD.cpp from the subdirectoy .src to the .src directory of the library.
// - add the special call to the added init method BEFORE the normal init method:
//   display.epd2.init(SW_SCK, SW_MOSI, 115200, true, 20, false); // define or replace SW_SCK, SW_MOSI
//   display.init(115200); // needed to init upper level


#ifndef _GxEPD2_EPD_H_
#define _GxEPD2_EPD_H_

#include <Arduino.h>
#include <SPI.h>

#include <GxEPD2.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"

class GxEPD2_EPD
{
  public:
    // attributes
    const uint16_t WIDTH;
    const uint16_t HEIGHT;
    const GxEPD2::Panel panel;
    const bool hasColor;
    const bool hasPartialUpdate;
    const bool hasFastPartialUpdate;
    // constructor
    GxEPD2_EPD(int16_t cs, int16_t dc, int16_t rst, int16_t busy, int16_t busy_level, uint32_t busy_timeout,
               uint16_t w, uint16_t h, GxEPD2::Panel p, bool c, bool pu, bool fpu);
    virtual void init(uint32_t serial_diag_bitrate = 0); // serial_diag_bitrate = 0 : disabled
    virtual void init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration = 20, bool pulldown_rst_mode = false);
    virtual void init(int16_t sck, int16_t mosi, uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration = 20, bool pulldown_rst_mode = false);
    virtual void end(); // release SPI and control pins
    //  Support for Bitmaps (Sprites) to Controller Buffer and to Screen
    virtual void clearScreen(uint8_t value) = 0; // init controller memory and screen (default white)
    virtual void writeScreenBuffer(uint8_t value) = 0; // init controller memory (default white)
    // write to controller memory, without screen refresh; x and w should be multiple of 8
    virtual void writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    virtual void writeImageForFullRefresh(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false)
    {
      // writeImage is independent from refresh mode for most controllers, exception e.g. SSD1681
      writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
    }
    virtual void writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    //    virtual void writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    //    virtual void writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
    //                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    // write sprite of native data to controller memory, without screen refresh; x and w should be multiple of 8
    //    virtual void writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    // for differential update: set current and previous buffers equal (for fast partial update to work correctly)
    virtual void writeScreenBufferAgain(uint8_t value = 0xFF) // init controller memory (default white)
    {
      // most controllers with differential update do switch buffers on refresh, can use:
      writeScreenBuffer(value);
    }
    virtual void writeImageAgain(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false)
    {
      // most controllers with differential update do switch buffers on refresh, can use:
      writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
    }
    virtual void writeImagePartAgain(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                     int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false)
    {
      // most controllers with differential update do switch buffers on refresh, can use:
      writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
    }
    // write to controller memory, with screen refresh; x and w should be multiple of 8
    //    virtual void drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    //    virtual void drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
    //                               int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    //    virtual void drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    //    virtual void drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
    //                               int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    // write sprite of native data to controller memory, with screen refresh; x and w should be multiple of 8
    //    virtual void drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false) = 0;
    virtual void refresh(bool partial_update_mode = false) = 0; // screen refresh from controller memory to full screen
    virtual void refresh(int16_t x, int16_t y, int16_t w, int16_t h) = 0; // screen refresh from controller memory, partial screen
    virtual void powerOff() = 0; // turns off generation of panel driving voltages, avoids screen fading over time
    virtual void hibernate() = 0; // turns powerOff() and sets controller to deep sleep for minimum power use, ONLY if wakeable by RST (rst >= 0)
    virtual void setPaged() {}; // for GxEPD2_154c paged workaround
    static inline uint16_t gx_uint16_min(uint16_t a, uint16_t b)
    {
      return (a < b ? a : b);
    };
    static inline uint16_t gx_uint16_max(uint16_t a, uint16_t b)
    {
      return (a > b ? a : b);
    };
    void selectSPI(SPIClass& spi, SPISettings spi_settings){};
  protected:
    void _reset();
    void _writeDataPGM(const uint8_t* data, uint16_t n, int16_t fill_with_zeroes = 0);
    void _writeDataPGM_sCS(const uint8_t* data, uint16_t n, int16_t fill_with_zeroes = 0);
    void _writeCommandData(const uint8_t* pCommandData, uint8_t datalen);
    void _writeCommandDataPGM(const uint8_t* pCommandData, uint8_t datalen);
    void _startTransfer();
    void _transfer(uint8_t value); 
    void _endTransfer();
    void _beginTransaction(const SPISettings& settings);
    void _spi_write(uint8_t data);
    void _endTransaction();
  public:
    void _waitWhileBusy(const char* comment = 0, uint16_t busy_time = 5000);
    void _writeCommand(uint8_t c);
    void _writeData(uint8_t d);
    void _writeData(const uint8_t* data, uint16_t n);
    uint8_t _readData();
    void _readData(uint8_t* data, uint16_t n);
  protected:
    int16_t _sck, _mosi;
    int16_t _cs, _dc, _rst, _busy, _busy_level;
    uint32_t _busy_timeout;
    bool _diag_enabled, _pulldown_rst_mode;
    SPIClass* _pSPIx;
    SPISettings _spi_settings;
    bool _initial_write, _initial_refresh;
    bool _power_is_on, _using_partial_mode, _hibernating;
    bool _init_display_done;
    uint16_t _reset_duration;
};

#endif
