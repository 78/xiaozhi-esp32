// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// based on Demo Example from Good Display:
// Panel: GDEW1248T3 : http://www.e-paper-display.com/products_detail/productId=414.html
// Controller: IL0326 : http://www.e-paper-display.com/download_detail/downloadsId=768.html
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#ifndef _GxEPD2_1248_H_
#define _GxEPD2_1248_H_

#include "../GxEPD2_EPD.h"

class GxEPD2_1248 : public GxEPD2_EPD
{
  public:
    // attributes
    static const uint16_t WIDTH = 1304;
    static const uint16_t WIDTH_VISIBLE = WIDTH;
    static const uint16_t HEIGHT = 984;
    static const GxEPD2::Panel panel = GxEPD2::GDEW1248T3;
    static const bool hasColor = false;
    static const bool hasPartialUpdate = true;
    static const bool hasFastPartialUpdate = true; // set this false to force full refresh always
    static const uint16_t power_on_time = 200; // ms, e.g. 131001us
    static const uint16_t power_off_time = 50; // ms, e.g. 41001us
    static const uint16_t full_refresh_time = 4600; // ms, e.g. 4579001us
    static const uint16_t partial_refresh_time = 1600; // ms, e.g. 1525001us
    // constructors
#if defined(ESP32)
    // general constructor for use with all parameters on ESP32, e.g. for Waveshare ESP32 driver board mounted on connection board
    GxEPD2_1248(int16_t sck, int16_t miso, int16_t mosi,
                int16_t cs_m1, int16_t cs_s1, int16_t cs_m2, int16_t cs_s2,
                int16_t dc1, int16_t dc2, int16_t rst1, int16_t rst2,
                int16_t busy_m1, int16_t busy_s1, int16_t busy_m2, int16_t busy_s2);
#endif
    // general constructor for use with standard SPI pins, default SCK, MISO and MOSI
    GxEPD2_1248(int16_t cs_m1, int16_t cs_s1, int16_t cs_m2, int16_t cs_s2,
                int16_t dc1, int16_t dc2, int16_t rst1, int16_t rst2,
                int16_t busy_m1, int16_t busy_s1, int16_t busy_m2, int16_t busy_s2);
    // constructor with minimal parameter set, standard SPI, dc1 and dc2, rst1 and rst2 to one pin, one busy used (can be -1)
    GxEPD2_1248(int16_t cs_m1, int16_t cs_s1, int16_t cs_m2, int16_t cs_s2, int16_t dc, int16_t rst, int16_t busy);
    // methods (virtual)
    void init(uint32_t serial_diag_bitrate = 0); // serial_diag_bitrate = 0 : disabled
    void init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration = 20, bool pulldown_rst_mode = false);
    //  Support for Bitmaps (Sprites) to Controller Buffer and to Screen
    void clearScreen(uint8_t value = 0xFF); // init controller memory and screen (default white)
    void writeScreenBuffer(uint8_t value = 0xFF); // init controller memory (default white)
    void writeScreenBufferAgain(uint8_t value = 0xFF); // init previous buffer controller memory (default white)
    // write to controller memory, without screen refresh; x and w should be multiple of 8
    void writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // for differential update: set current and previous buffers equal (for fast partial update to work correctly)
    void writeImageAgain(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImagePartAgain(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
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
    void _reset();
    void _initSPI();
    void _setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void _PowerOn();
    void _PowerOff();
    void _InitDisplay();
    void _Init_Full();
    void _Init_Part();
    void _Update_Full();
    void _Update_Part();
    void _writeCommandMaster(uint8_t c);
    void _writeDataMaster(uint8_t d);
    void _writeCommandAll(uint8_t c);
    void _writeDataAll(uint8_t d);
    void _writeDataPGM_All(const uint8_t* data, uint16_t n, int16_t fill_with_zeroes = 0);
    void _waitWhileAnyBusy(const char* comment = 0, uint16_t busy_time = 5000);
    void _getMasterTemperature();
  private:
    friend class GDEW1248T3_OTP;
    void _readController(uint8_t cmd, uint8_t* data, uint16_t n, int8_t cs = -1, int8_t dc = -1);
  private:
    int16_t _sck, _miso, _mosi, _dc1, _dc2, _rst1, _rst2;
    int16_t _cs_m1, _cs_s1, _cs_m2, _cs_s2;
    int16_t _busy_m1, _busy_s1, _busy_m2, _busy_s2;
    int8_t _temperature;
    static const unsigned char lut_20_LUTC_partial[];
    static const unsigned char lut_21_LUTWW_partial[];
    static const unsigned char lut_22_LUTKW_partial[];
    static const unsigned char lut_23_LUTWK_partial[];
    static const unsigned char lut_24_LUTKK_partial[];
    static const unsigned char lut_25_LUTBD_partial[];
  private:
    class ScreenPart
    {
      public:
        ScreenPart(uint16_t width, uint16_t height, bool rev_scan, int16_t cs, int16_t dc);
        void writeScreenBuffer(uint8_t command, uint8_t value = 0xFF); // init controller memory current (default white)
        void writeImagePart(uint8_t command, const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                            int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
        void writeCommand(uint8_t c);
        void writeData(uint8_t d);
      private:
        void _setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
        void _startTransfer();
        void _transfer(uint8_t value);
        void _endTransfer();
      public:
        const uint16_t WIDTH, HEIGHT;
      private:
        bool _rev_scan;
        int16_t _cs, _dc;
        const SPISettings _spi_settings;
    };
    ScreenPart M1, S1, M2, S2;
};

#endif
