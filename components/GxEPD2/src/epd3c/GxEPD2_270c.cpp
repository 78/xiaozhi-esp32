// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: these e-papers require 3.3V supply AND data lines!
//
// based on Demo Example from Good Display: http://www.e-paper-display.com/download_list/downloadcategoryid=34&isMode=false.html
// Controller: IL91874 : http://www.e-paper-display.com/download_detail/downloadsId=539.html
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#include "GxEPD2_270c.h"

GxEPD2_270c::GxEPD2_270c(int16_t cs, int16_t dc, int16_t rst, int16_t busy) :
  GxEPD2_EPD(cs, dc, rst, busy, LOW, 20000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate)
{
}

void GxEPD2_270c::clearScreen(uint8_t value)
{
  clearScreen(value, 0xFF);
}

void GxEPD2_270c::clearScreen(uint8_t black_value, uint8_t red_value)
{
  _initial_write = false; // initial full screen buffer clean done
  _Init_Part();
  _setPartialRamArea_270c(0x14, 0, 0, WIDTH, HEIGHT);
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _writeData(~black_value);
  }
  _setPartialRamArea_270c(0x15, 0, 0, WIDTH, HEIGHT);
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _writeData(~red_value);
  }
  refresh(0, 0, WIDTH, HEIGHT);
}

void GxEPD2_270c::writeScreenBuffer(uint8_t value)
{
  writeScreenBuffer(value, 0xFF);
}

void GxEPD2_270c::writeScreenBuffer(uint8_t black_value, uint8_t color_value)
{
  _initial_write = false; // initial full screen buffer clean done
  _Init_Part();
  _setPartialRamArea_270c(0x14, 0, 0, WIDTH, HEIGHT);
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _writeData(~black_value);
  }
  _setPartialRamArea_270c(0x15, 0, 0, WIDTH, HEIGHT);
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _writeData(~color_value);
  }
}

void GxEPD2_270c::writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(bitmap, NULL, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_270c::writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  int16_t wb = (w + 7) / 8; // width bytes, bitmaps are padded
  x -= x % 8; // byte boundary
  w = wb * 8; // byte boundary
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  int16_t w1 = x + w < int16_t(WIDTH) ? w : int16_t(WIDTH) - x; // limit
  int16_t h1 = y + h < int16_t(HEIGHT) ? h : int16_t(HEIGHT) - y; // limit
  int16_t dx = x1 - x;
  int16_t dy = y1 - y;
  w1 -= dx;
  h1 -= dy;
  if ((w1 <= 0) || (h1 <= 0)) return;
  _Init_Part();
  _setPartialRamArea_270c(0x14, x1, y1, w1, h1);
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data = 0xFF;
      if (black)
      {
        // use wb, h of bitmap for index!
        int16_t idx = mirror_y ? j + dx / 8 + ((h - 1 - (i + dy))) * wb : j + dx / 8 + (i + dy) * wb;
        if (pgm)
        {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
          data = pgm_read_byte(&black[idx]);
#else
          data = black[idx];
#endif
        }
        else
        {
          data = black[idx];
        }
        if (invert) data = ~data;
      }
      _writeData(~data);
    }
  }
  _setPartialRamArea_270c(0x15, x1, y1, w1, h1);
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data = 0xFF;
      if (color)
      {
        // use wb, h of bitmap for index!
        int16_t idx = mirror_y ? j + dx / 8 + ((h - 1 - (i + dy))) * wb : j + dx / 8 + (i + dy) * wb;
        if (pgm)
        {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
          data = pgm_read_byte(&color[idx]);
#else
          data = color[idx];
#endif
        }
        else
        {
          data = color[idx];
        }
        if (invert) data = ~data;
      }
      _writeData(~data);
    }
  }
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_270c::writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                 int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, NULL, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_270c::writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                 int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  if ((w_bitmap < 0) || (h_bitmap < 0) || (w < 0) || (h < 0)) return;
  if ((x_part < 0) || (x_part >= w_bitmap)) return;
  if ((y_part < 0) || (y_part >= h_bitmap)) return;
  int16_t wb_bitmap = (w_bitmap + 7) / 8; // width bytes, bitmaps are padded
  x_part -= x_part % 8; // byte boundary
  w = w_bitmap - x_part < w ? w_bitmap - x_part : w; // limit
  h = h_bitmap - y_part < h ? h_bitmap - y_part : h; // limit
  x -= x % 8; // byte boundary
  w = 8 * ((w + 7) / 8); // byte boundary, bitmaps are padded
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  int16_t w1 = x + w < int16_t(WIDTH) ? w : int16_t(WIDTH) - x; // limit
  int16_t h1 = y + h < int16_t(HEIGHT) ? h : int16_t(HEIGHT) - y; // limit
  int16_t dx = x1 - x;
  int16_t dy = y1 - y;
  w1 -= dx;
  h1 -= dy;
  if ((w1 <= 0) || (h1 <= 0)) return;
  if (!_using_partial_mode) _Init_Part();
  _setPartialRamArea_270c(0x14, x1, y1, w1, h1);
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data;
      // use wb_bitmap, h_bitmap of bitmap for index!
      int16_t idx = mirror_y ? x_part / 8 + j + dx / 8 + ((h_bitmap - 1 - (y_part + i + dy))) * wb_bitmap : x_part / 8 + j + dx / 8 + (y_part + i + dy) * wb_bitmap;
      if (pgm)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&black[idx]);
#else
        data = black[idx];
#endif
      }
      else
      {
        data = black[idx];
      }
      if (invert) data = ~data;
      _writeData(~data);
    }
  }
  _setPartialRamArea_270c(0x15, x1, y1, w1, h1);
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data = 0xFF;
      if (color)
      {
        // use wb_bitmap, h_bitmap of bitmap for index!
        int16_t idx = mirror_y ? x_part / 8 + j + dx / 8 + ((h_bitmap - 1 - (y_part + i + dy))) * wb_bitmap : x_part / 8 + j + dx / 8 + (y_part + i + dy) * wb_bitmap;
        if (pgm)
        {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
          data = pgm_read_byte(&color[idx]);
#else
          data = color[idx];
#endif
        }
        else
        {
          data = color[idx];
        }
        if (invert) data = ~data;
      }
      _writeData(~data);
    }
  }
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_270c::writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    writeImage(data1, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_270c::drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_270c::drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_270c::drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(black, color, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_270c::drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_270c::drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_270c::refresh(bool partial_update_mode)
{
  if (partial_update_mode) refresh(0, 0, WIDTH, HEIGHT);
  else _Update_Full();
}

void GxEPD2_270c::refresh(int16_t x, int16_t y, int16_t w, int16_t h)
{
  // intersection with screen
  int16_t w1 = x < 0 ? w + x : w; // reduce
  int16_t h1 = y < 0 ? h + y : h; // reduce
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  w1 = x1 + w1 < int16_t(WIDTH) ? w1 : int16_t(WIDTH) - x1; // limit
  h1 = y1 + h1 < int16_t(HEIGHT) ? h1 : int16_t(HEIGHT) - y1; // limit
  if ((w1 <= 0) || (h1 <= 0)) return;
  // make x1, w1 multiple of 8
  w1 += x1 % 8;
  if (w1 % 8 > 0) w1 += 8 - w1 % 8;
  x1 -= x1 % 8;
  //_refreshWindow(x1, y1, w1, h1);
  w1 = (w1 + 7 + (x1 % 8)) & 0xfff8; // byte boundary exclusive (round up)
  h1 = gx_uint16_min(h1, 256); // strange controller error
  _writeCommand(0x16);
  _writeData(x1 >> 8);
  _writeData(x1 & 0xf8);
  _writeData(y1 >> 8);
  _writeData(y1 & 0xff);
  _writeData(w1 >> 8);
  _writeData(w1 & 0xf8);
  _writeData(h1 >> 8);
  _writeData(h1 & 0xff);
  _waitWhileBusy("refresh", partial_refresh_time);
}

void GxEPD2_270c::powerOff()
{
  _PowerOff();
}

void GxEPD2_270c::hibernate()
{
  _PowerOff();
  if (_rst >= 0)
  {
    _writeCommand(0x07); // deep sleep
    _writeData(0xA5);    // check code
    _hibernating = true;
  }
}

void GxEPD2_270c::_setPartialRamArea_270c(uint8_t cmd, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  w = (w + 7 + (x % 8)) & 0xfff8; // byte boundary exclusive (round up)
  _writeCommand(cmd);
  _writeData(x >> 8);
  _writeData(x & 0xf8);
  _writeData(y >> 8);
  _writeData(y & 0xff);
  _writeData(w >> 8);
  _writeData(w & 0xf8);
  _writeData(h >> 8);
  _writeData(h & 0xff);
}

void GxEPD2_270c::_PowerOn()
{
  if (!_power_is_on)
  {
    _writeCommand(0x04);
    _waitWhileBusy("_PowerOn", power_on_time);
  }
  _power_is_on = true;
}

void GxEPD2_270c::_PowerOff()
{
  if (_power_is_on)
  {
    _writeCommand(0x02); // power off
    _waitWhileBusy("_PowerOff", power_off_time);
    _power_is_on = false;
  }
}

void GxEPD2_270c::_InitDisplay()
{
  if (_hibernating) _reset();
  _writeCommand(0x01);
  _writeData (0x03);
  _writeData (0x00);
  _writeData (0x2b);
  _writeData (0x2b);
  _writeData (0x09);
  _writeCommand(0x06);
  _writeData (0x07);
  _writeData (0x07);
  _writeData (0x17);
  _writeCommand(0xF8);
  _writeData (0x60);
  _writeData (0xA5);
  _writeCommand(0xF8);
  _writeData (0x89);
  _writeData (0xA5);
  _writeCommand(0xF8);
  _writeData (0x90);
  _writeData (0x00);
  _writeCommand(0xF8);
  _writeData (0x93);
  _writeData (0x2A);
  _writeCommand(0xF8);
  _writeData (0x73);
  _writeData (0x41);
  _writeCommand(0x16);
  _writeData(0x00);
  //_writeCommand(0x04);
  //_waitWhileBusy("_wakeUp Power On");
  _writeCommand(0x00);
  _writeData(0xaf); // by register LUT
  _writeCommand(0x30);
  _writeData (0x3a);
  _writeCommand(0x61);
  _writeData (0x00);
  _writeData (0xb0); //176
  _writeData (0x01);
  _writeData (0x08); //264
  _writeCommand(0x82);
  _writeData (0x12);
  _writeCommand(0X50);
  _writeData(0x87);
}

const uint8_t GxEPD2_270c::lut_20_vcomDC[] PROGMEM =
{
  0x00  , 0x00,
  0x00  , 0x1A  , 0x1A  , 0x00  , 0x00  , 0x01,
  0x00  , 0x0A  , 0x0A  , 0x00  , 0x00  , 0x08,
  0x00  , 0x0E  , 0x01  , 0x0E  , 0x01  , 0x10,
  0x00  , 0x0A  , 0x0A  , 0x00  , 0x00  , 0x08,
  0x00  , 0x04  , 0x10  , 0x00  , 0x00  , 0x05,
  0x00  , 0x03  , 0x0E  , 0x00  , 0x00  , 0x0A,
  0x00  , 0x23  , 0x00  , 0x00  , 0x00  , 0x01
};
//R21H
const uint8_t GxEPD2_270c::lut_21[] PROGMEM = {
  0x90  , 0x1A  , 0x1A  , 0x00  , 0x00  , 0x01,
  0x40  , 0x0A  , 0x0A  , 0x00  , 0x00  , 0x08,
  0x84  , 0x0E  , 0x01  , 0x0E  , 0x01  , 0x10,
  0x80  , 0x0A  , 0x0A  , 0x00  , 0x00  , 0x08,
  0x00  , 0x04  , 0x10  , 0x00  , 0x00  , 0x05,
  0x00  , 0x03  , 0x0E  , 0x00  , 0x00  , 0x0A,
  0x00  , 0x23  , 0x00  , 0x00  , 0x00  , 0x01
};
//R22H  r
const uint8_t GxEPD2_270c::lut_22_red[] PROGMEM = {
  0xA0  , 0x1A  , 0x1A  , 0x00  , 0x00  , 0x01,
  0x00  , 0x0A  , 0x0A  , 0x00  , 0x00  , 0x08,
  0x84  , 0x0E  , 0x01  , 0x0E  , 0x01  , 0x10,
  0x90  , 0x0A  , 0x0A  , 0x00  , 0x00  , 0x08,
  0xB0  , 0x04  , 0x10  , 0x00  , 0x00  , 0x05,
  0xB0  , 0x03  , 0x0E  , 0x00  , 0x00  , 0x0A,
  0xC0  , 0x23  , 0x00  , 0x00  , 0x00  , 0x01
};
//R23H  w
const uint8_t GxEPD2_270c::lut_23_white[] PROGMEM = {
  0x90  , 0x1A  , 0x1A  , 0x00  , 0x00  , 0x01,
  0x40  , 0x0A  , 0x0A  , 0x00  , 0x00  , 0x08,
  0x84  , 0x0E  , 0x01  , 0x0E  , 0x01  , 0x10,
  0x80  , 0x0A  , 0x0A  , 0x00  , 0x00  , 0x08,
  0x00  , 0x04  , 0x10  , 0x00  , 0x00  , 0x05,
  0x00  , 0x03  , 0x0E  , 0x00  , 0x00  , 0x0A,
  0x00  , 0x23  , 0x00  , 0x00  , 0x00  , 0x01
};
//R24H  b
const uint8_t GxEPD2_270c::lut_24_black[] PROGMEM = {
  0x90  , 0x1A  , 0x1A  , 0x00  , 0x00  , 0x01,
  0x20  , 0x0A  , 0x0A  , 0x00  , 0x00  , 0x08,
  0x84  , 0x0E  , 0x01  , 0x0E  , 0x01  , 0x10,
  0x10  , 0x0A  , 0x0A  , 0x00  , 0x00  , 0x08,
  0x00  , 0x04  , 0x10  , 0x00  , 0x00  , 0x05,
  0x00  , 0x03  , 0x0E  , 0x00  , 0x00  , 0x0A,
  0x00  , 0x23  , 0x00  , 0x00  , 0x00  , 0x01
};

void GxEPD2_270c::_Init_Full()
{
  _InitDisplay();
  _writeCommand(0x20); //vcom
  _writeDataPGM_sCS(lut_20_vcomDC, sizeof(lut_20_vcomDC));
  _writeCommand(0x21); //ww --
  _writeDataPGM_sCS(lut_21, sizeof(lut_21));
  _writeCommand(0x22); //bw r
  _writeDataPGM_sCS(lut_22_red, sizeof(lut_22_red));
  _writeCommand(0x23); //wb w
  _writeDataPGM_sCS(lut_23_white, sizeof(lut_23_white));
  _writeCommand(0x24); //bb b
  _writeDataPGM_sCS(lut_24_black, sizeof(lut_24_black));
  _PowerOn();
}

void GxEPD2_270c::_Init_Part()
{
  _InitDisplay();
  _writeCommand(0x20); //vcom
  _writeDataPGM_sCS(lut_20_vcomDC, sizeof(lut_20_vcomDC));
  _writeCommand(0x21); //ww --
  _writeDataPGM_sCS(lut_21, sizeof(lut_21));
  _writeCommand(0x22); //bw r
  _writeDataPGM_sCS(lut_22_red, sizeof(lut_22_red));
  _writeCommand(0x23); //wb w
  _writeDataPGM_sCS(lut_23_white, sizeof(lut_23_white));
  _writeCommand(0x24); //bb b
  _writeDataPGM_sCS(lut_24_black, sizeof(lut_24_black));
  _PowerOn();
}

void GxEPD2_270c::_Update_Full()
{
  _writeCommand(0x12); //display refresh
  _waitWhileBusy("_Update_Full", full_refresh_time);
}

void GxEPD2_270c::_Update_Part()
{
  _writeCommand(0x12); //display refresh
  _waitWhileBusy("_Update_Part", partial_refresh_time);
}
