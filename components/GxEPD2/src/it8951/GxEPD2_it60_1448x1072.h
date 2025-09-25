// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: these e-papers require 3.3V supply AND data lines!
//
// GxEPD2_it60_1448x1072 class is based on Demo Example from Waveshare for Raspberry PI https://github.com/waveshare/IT8951/archive/master.zip
// Controller: IT8951 : https://www.waveshare.com/w/upload/1/18/IT8951_D_V0.2.4.3_20170728.pdf
//
// The GxEPD2_it60_1448x1072 driver class supports the Waveshare e-Paper IT8951 Driver HAT connected with SPI for the ED060SCT 6" e-paper panel (parallel IF)
// https://www.waveshare.com/product/mini-pc/raspberry-pi/hats/6inch-e-paper-hat.htm
// This Driver HAT requires 5V power supply but works with 3.3V data lines; requires both MOSI and MISO SPI lines.
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#ifndef _GxEPD2_it60_1448x1072_H_
#define _GxEPD2_it60_1448x1072_H_

#include "../GxEPD2_EPD.h"

class GxEPD2_it60_1448x1072 : public GxEPD2_EPD
{
  public:
    // attributes
    static const uint16_t WIDTH = 1448;
    static const uint16_t WIDTH_VISIBLE = WIDTH;
    static const uint16_t HEIGHT = 1072;
    static const GxEPD2::Panel panel = GxEPD2::ED060KC1;
    static const bool hasColor = false;
    static const bool hasPartialUpdate = true;
    static const bool hasFastPartialUpdate = true;
    static const uint16_t reset_to_ready_time = 1800; // ms, e.g. 1688856us
    static const uint16_t power_on_time = 10; // ms, e.g. 3060us
    static const uint16_t power_off_time = 250; // ms, e.g. 214106us
    static const uint16_t full_refresh_time = 650; // ms, e.g. 608149us
    static const uint16_t partial_refresh_time = 350; // ms, e.g. 330933us
    static const uint16_t refresh_cmd_time = 10; // ms, e.g. 6109us
    static const uint16_t refresh_par_time = 2; // ms, e.g. 1921us
    static const uint16_t default_wait_time = 1; // ms, default busy check, needed?
    static const uint16_t diag_min_time = 3; // ms, e.g. > refresh_par_time
    static const uint16_t set_vcom_time = 500; // ms, e.g. 408377us
    // constructor
    GxEPD2_it60_1448x1072(int16_t cs, int16_t dc, int16_t rst, int16_t busy);
    // methods (virtual)
    void init(uint32_t serial_diag_bitrate = 0); // serial_diag_bitrate = 0 : disabled
    void init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration = 20, bool pulldown_rst_mode = false);
    //  Support for Bitmaps (Sprites) to Controller Buffer and to Screen
    void clearScreen(uint8_t value = 0x33); // init controller memory and screen (default white)
    void writeScreenBuffer(uint8_t value = 0x33); // init controller memory (default white)
    // write to controller memory, without screen refresh; x and w should be multiple of 8
    void writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // write sprite of native data to controller memory, without screen refresh; x and w should be multiple of 8
    void writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // write to controller memory, with screen refresh; x and w should be multiple of 8
    void drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // write sprite of native data to controller memory, with screen refresh; x and w should be multiple of 8
    void drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void refresh(bool partial_update_mode = false); // screen refresh from controller memory to full screen
    void refresh(int16_t x, int16_t y, int16_t w, int16_t h); // screen refresh from controller memory, partial screen
    void powerOff(); // turns off generation of panel driving voltages, avoids screen fading over time
    void hibernate(); // turns powerOff() and sets controller to deep sleep for minimum power use, ONLY if wakeable by RST (rst >= 0)
  private:
    struct IT8951DevInfoStruct
    {
      uint16_t usPanelW;
      uint16_t usPanelH;
      uint16_t usImgBufAddrL;
      uint16_t usImgBufAddrH;
      uint16_t usFWVersion[8];   //16 Bytes String
      uint16_t usLUTVersion[8];   //16 Bytes String
    };
    IT8951DevInfoStruct IT8951DevInfo;
    SPISettings _spi_settings;
    SPISettings _spi_settings_for_read;
  private:
    void _writeScreenBuffer(uint8_t value);
    void _refresh(int16_t x, int16_t y, int16_t w, int16_t h, bool partial_update_mode);
    void _send8pixel(uint8_t data);
    void _setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void _PowerOn();
    void _PowerOff();
    void _InitDisplay();
    void _Init_Full();
    void _Init_Part();
    // IT8951
    void _waitWhileBusy2(const char* comment = 0, uint16_t busy_time = 5000);
    uint16_t _transfer16(uint16_t value);
    void _writeCommand16(uint16_t c);
    void _writeData16(uint16_t d);
    void _writeData16(const uint16_t* d, uint32_t n);
    uint16_t _readData16();
    void _readData16(uint16_t* d, uint32_t n);
    void _writeCommandData16(uint16_t c, const uint16_t* d, uint16_t n);
    void _IT8951SystemRun();
    void _IT8951StandBy();
    void _IT8951Sleep();
    uint16_t _IT8951ReadReg(uint16_t usRegAddr);
    void _IT8951WriteReg(uint16_t usRegAddr, uint16_t usValue);
    uint16_t _IT8951GetVCOM(void);
    void _IT8951SetVCOM(uint16_t vcom);
};

#endif
