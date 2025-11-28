// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// based on Demo Example from Good Display: http://www.e-paper-display.com/download_detail/downloadsId=808.html
// Panel: GDEH075Z90 : http://www.e-paper-display.com/products_detail/productId=535.html
// Controller: SSD1677 : http://www.e-paper-display.com/SSD1677Specification.pdf
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#include "GxEPD2_750c_Z90.h"

GxEPD2_750c_Z90::GxEPD2_750c_Z90(int16_t cs, int16_t dc, int16_t rst, int16_t busy) :
  GxEPD2_EPD(cs, dc, rst, busy, HIGH, 25000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate)
{
}

void GxEPD2_750c_Z90::clearScreen(uint8_t value)
{
  clearScreen(value, 0xFF);
}

void GxEPD2_750c_Z90::clearScreen(uint8_t black_value, uint8_t color_value)
{
  writeScreenBuffer(black_value, color_value);
  _Update_Full();
}

void GxEPD2_750c_Z90::writeScreenBuffer(uint8_t value)
{
  writeScreenBuffer(value, 0xFF);
}

void GxEPD2_750c_Z90::writeScreenBuffer(uint8_t black_value, uint8_t color_value)
{
  _initial_write = false; // initial full screen buffer clean done
  _Init_Full();
  _setPartialRamArea(0, 0, WIDTH, HEIGHT);
  _writeCommand(0x24);
  _startTransfer();
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _transfer(black_value);
  }
  _endTransfer();
  _writeCommand(0x26);
  _startTransfer();
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _transfer(~color_value);
  }
  _endTransfer();
}

void GxEPD2_750c_Z90::writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(bitmap, NULL, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_750c_Z90::writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  uint16_t wb = (w + 7) / 8; // width bytes, bitmaps are padded
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
  _setPartialRamArea(x1, y1, w1, h1);
  _writeCommand(0x24);
  _startTransfer();
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data = 0xFF;
      if (black)
      {
        // use wb, h of bitmap for index!
        uint16_t idx = mirror_y ? j + dx / 8 + uint16_t((h - 1 - (i + dy))) * wb : j + dx / 8 + uint16_t(i + dy) * wb;
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
      _transfer(data);
    }
  }
  _endTransfer();
  _writeCommand(0x26);
  _startTransfer();
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data = 0xFF;
      if (color)
      {
        // use wb, h of bitmap for index!
        uint16_t idx = mirror_y ? j + dx / 8 + uint16_t((h - 1 - (i + dy))) * wb : j + dx / 8 + uint16_t(i + dy) * wb;
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
      _transfer(~data);
    }
  }
  _endTransfer();
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_750c_Z90::writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                     int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, NULL, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_750c_Z90::writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                     int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  if ((w_bitmap < 0) || (h_bitmap < 0) || (w < 0) || (h < 0)) return;
  if ((x_part < 0) || (x_part >= w_bitmap)) return;
  if ((y_part < 0) || (y_part >= h_bitmap)) return;
  uint16_t wb_bitmap = (w_bitmap + 7) / 8; // width bytes, bitmaps are padded
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
  _setPartialRamArea(x1, y1, w1, h1);
  _writeCommand(0x24);
  _startTransfer();
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data;
      // use wb_bitmap, h_bitmap of bitmap for index!
      uint16_t idx = mirror_y ? x_part / 8 + j + dx / 8 + uint16_t((h_bitmap - 1 - (y_part + i + dy))) * wb_bitmap : x_part / 8 + j + dx / 8 + uint16_t(y_part + i + dy) * wb_bitmap;
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
      _transfer(data);
    }
  }
  _endTransfer();
  _writeCommand(0x26);
  _startTransfer();
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data = 0xFF;
      if (color)
      {
        // use wb_bitmap, h_bitmap of bitmap for index!
        uint16_t idx = mirror_y ? x_part / 8 + j + dx / 8 + uint16_t((h_bitmap - 1 - (y_part + i + dy))) * wb_bitmap : x_part / 8 + j + dx / 8 + uint16_t(y_part + i + dy) * wb_bitmap;
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
      _transfer(~data);
    }
  }
  _endTransfer();
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_750c_Z90::writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    writeImage(data1, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_750c_Z90::drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_750c_Z90::drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                    int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_750c_Z90::drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(black, color, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_750c_Z90::drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                    int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_750c_Z90::drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_750c_Z90::refresh(bool partial_update_mode)
{
  if (partial_update_mode) refresh(0, 0, WIDTH, HEIGHT);
  else _Update_Full();
}

void GxEPD2_750c_Z90::refresh(int16_t x, int16_t y, int16_t w, int16_t h)
{
  _Update_Part();
}

void GxEPD2_750c_Z90::powerOff()
{
  _PowerOff();
}

void GxEPD2_750c_Z90::hibernate()
{
  _PowerOff();
  if (_rst >= 0)
  {
    _writeCommand(0x10); // deep sleep
    _writeData(0x11);    // deep sleep
    _hibernating = true;
  }
}

void GxEPD2_750c_Z90::_setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  //Serial.print("_setPartialRamArea("); Serial.print(x); Serial.print(", "); Serial.print(y); Serial.print(", "); Serial.print(w); Serial.print(", "); Serial.print(h); Serial.println(")");
  _writeCommand(0x11);  // Data entry mode
  _writeData(entry_mode & 0x03);
  switch (entry_mode & 0x03)
  {
    case 0x00: // x decrement, y decrement
      _setRamArea(WIDTH - x - 1, WIDTH - w - x, HEIGHT - y - 1, HEIGHT - h - y);
      _setRamPointer(WIDTH - x - 1, HEIGHT - y - 1);
      break;
    case 0x01: // x increment, y decrement
      _setRamArea(x, x + w - 1, HEIGHT - y - 1, HEIGHT - h - y);
      _setRamPointer(x, HEIGHT - y - 1);
      break;
    case 0x02: // x decrement, y incrment
      _setRamArea(WIDTH - x - 1, WIDTH - w - x, y, y + h - 1);
      _setRamPointer(WIDTH - x - 1, y);
      break;
    case 0x03: // x increment, y increment
      _setRamArea(x, x + w - 1, y, y + h - 1);
      _setRamPointer(x, y);
  }
}

void GxEPD2_750c_Z90::_setRamArea(uint16_t xs, uint16_t xe, uint16_t ys, uint16_t ye)
{
  //Serial.print("_setRamArea("); Serial.print(xs); Serial.print(", "); Serial.print(ys); Serial.print(", "); Serial.print(xe); Serial.print(", "); Serial.print(ye); Serial.println(")");
  _writeCommand(0x44); // Set RAM X address Start/End position
  _writeData(xs % 256);
  _writeData(xs / 256);
  _writeData(xe % 256);
  _writeData(xe / 256);
  _writeCommand(0x45); // Set RAM Y address Start/End position
  _writeData(ys % 256);
  _writeData(ys / 256);
  _writeData(ye % 256);
  _writeData(ye / 256);
}

void GxEPD2_750c_Z90::_setRamPointer(uint16_t xs, uint16_t ys)
{
  //Serial.print("_setRamPointer("); Serial.print(xs); Serial.print(", "); Serial.print(ys); Serial.println(")");
  _writeCommand(0x4E); // Set RAM X Address Counter
  _writeData(xs % 256);
  _writeData(xs / 256);
  _writeCommand(0x4F); // Set RAM Y Address Counter
  _writeData(ys % 256);
  _writeData(ys / 256);
}

void GxEPD2_750c_Z90::_PowerOn()
{
  if (!_power_is_on)
  {
    _writeCommand(0x22);
    _writeData(0xc0);
    _writeCommand(0x20);
    _waitWhileBusy("_PowerOn", power_on_time);
  }
  _power_is_on = true;
}

void GxEPD2_750c_Z90::_PowerOff()
{
  if (_power_is_on)
  {
    _writeCommand(0x22);
    _writeData(0xc3);
    _writeCommand(0x20);
    _waitWhileBusy("_PowerOff", power_off_time);
  }
  _power_is_on = false;
  _using_partial_mode = false;
}

void GxEPD2_750c_Z90::_InitDisplay()
{
  if (_hibernating) _reset();
  _writeCommand(0x12); //SWRESET
  _waitWhileBusy(0, power_on_time);
  _writeCommand(0x0C); // Soft start setting
  _writeData(0xAE);
  _writeData(0xC7);
  _writeData(0xC3);
  _writeData(0xC0);
  _writeData(0x40);
  _writeCommand(0x01); // Set MUX as 527
  _writeData(0x0F);
  _writeData(0x02);
  _writeData(0x00);
  _setPartialRamArea(0, 0, WIDTH, HEIGHT);
  _writeCommand(0x3C); // VBD
  _writeData(0x01);    // LUT1, for white
  _writeCommand(0x18); // Temperature Sensor Selection
  _writeData(0x80);    // internal temperature sensor
  _writeCommand(0x22); // Display Update Sequence Options
  _writeData(0xB1);    // Load Temperature and waveform setting.
  _writeCommand(0x20); // Master Activation
  _waitWhileBusy(0, power_on_time);
}

void GxEPD2_750c_Z90::_Init_Full()
{
  _InitDisplay();
}

void GxEPD2_750c_Z90::_Init_Part()
{
  _InitDisplay();
}

void GxEPD2_750c_Z90::_Update_Full()
{
  _writeCommand(0x22); // Display Update Sequence Options
  _writeData(0xC7);    //
  _writeCommand(0x20); // Master Activation
  _waitWhileBusy("_Update_Full", full_refresh_time);
  _power_is_on = false;
}

void GxEPD2_750c_Z90::_Update_Part()
{
  _writeCommand(0x22); // Display Update Sequence Options
  _writeData(0xC7);    //
  _writeCommand(0x20); // Master Activation
  _waitWhileBusy("_Update_Part", partial_refresh_time);
  _power_is_on = false;
}
