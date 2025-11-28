// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: these e-papers require 3.3V supply AND data lines!
//
// based on Demo Example from Good Display: http://www.e-paper-display.com/download_list/downloadcategoryid=34&isMode=false.html
// Controller: IL0376F : http://www.e-paper-display.com/download_detail/downloadsId=541.html
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#include "GxEPD2_154c.h"

const uint8_t GxEPD2_154c::bw2grey[] =
{
  0b00000000, 0b00000011, 0b00001100, 0b00001111,
  0b00110000, 0b00110011, 0b00111100, 0b00111111,
  0b11000000, 0b11000011, 0b11001100, 0b11001111,
  0b11110000, 0b11110011, 0b11111100, 0b11111111,
};

GxEPD2_154c::GxEPD2_154c(int16_t cs, int16_t dc, int16_t rst, int16_t busy) :
  GxEPD2_EPD(cs, dc, rst, busy, LOW, 20000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate)
{
  _paged = false;
  _second_phase = false;
}

void GxEPD2_154c::init(uint32_t serial_diag_bitrate)
{
  GxEPD2_EPD::init(serial_diag_bitrate);
  _paged = false;
  _second_phase = false;
}

void GxEPD2_154c::init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration, bool pulldown_rst_mode)
{
  GxEPD2_EPD::init(serial_diag_bitrate, initial, reset_duration, pulldown_rst_mode);
  _paged = false;
  _second_phase = false;
}

void GxEPD2_154c::clearScreen(uint8_t value)
{
  clearScreen(value, 0xFF);
}

void GxEPD2_154c::clearScreen(uint8_t black_value, uint8_t color_value)
{
  _Init_Full();
  _writeCommand(0x10);
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _writeData(bw2grey[(black_value & 0xF0) >> 4]);
    _writeData(bw2grey[black_value & 0x0F]);
  }
  _writeCommand(0x13);
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _writeData(color_value);
  }
  _Update_Full();
}

void GxEPD2_154c::writeScreenBuffer(uint8_t value)
{
  writeScreenBuffer(value, 0xFF);
}

void GxEPD2_154c::writeScreenBuffer(uint8_t black_value, uint8_t color_value)
{
  _Init_Full();
  _writeCommand(0x10);
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _writeData(bw2grey[(black_value & 0xF0) >> 4]);
    _writeData(bw2grey[black_value & 0x0F]);
  }
  _writeCommand(0x13);
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _writeData(color_value);
  }
}

void GxEPD2_154c::writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(bitmap, NULL, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_154c::writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  //Serial.print("writeImage("); Serial.print(x); Serial.print(", "); Serial.print(y); Serial.print(", ");
  //Serial.print(w); Serial.print(", "); Serial.print(h); Serial.println(")");
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  if (_paged && (x == 0) && (w == int16_t(WIDTH)) && (h < int16_t(HEIGHT)))
  {
    //Serial.println("paged");
    if (!_second_phase)
    {
      for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(h) / 8; i++)
      {
        _writeData(bw2grey[(black[i] & 0xF0) >> 4]);
        _writeData(bw2grey[black[i] & 0x0F]);
      }
      if (y + h == HEIGHT) // last page
      {
        //Serial.println("phase 1 ended");
        _second_phase = true;
        _writeCommand(0x13);
      }
    }
    else
    {
      for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(h) / 8; i++)
      {
        _writeData(color[i]);
      }
      if (y + h == HEIGHT) // last page
      {
        //Serial.println("phase 2 ended");
        _second_phase = false;
        _paged = false;
      }
    }
  }
  else
  {
    _paged = false;
    uint16_t wb = (w + 7) / 8; // width bytes, bitmaps are padded
    x -= x % 8; // byte boundary
    w = wb * 8; // byte boundary
    if ((w <= 0) || (h <= 0)) return;
    _Init_Full();
    _writeCommand(0x10);
    for (int16_t i = 0; i < int16_t(HEIGHT); i++)
    {
      for (int16_t j = 0; j < int16_t(WIDTH); j += 8)
      {
        uint8_t data = 0xFF;
        if (black)
        {
          if ((j >= x) && (j <= x + w) && (i >= y) && (i < y + h))
          {
            uint16_t idx = mirror_y ? (j - x) / 8 + ((h - 1 - (i - y))) * wb : (j - x) / 8 + (i - y) * wb;
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
        }
        //_writeData(data);
        _writeData(bw2grey[(data & 0xF0) >> 4]);
        _writeData(bw2grey[data & 0x0F]);
      }
    }
    _writeCommand(0x13);
    for (int16_t i = 0; i < int16_t(HEIGHT); i++)
    {
      for (int16_t j = 0; j < int16_t(WIDTH); j += 8)
      {
        uint8_t data = 0xFF;
        if (color)
        {
          if ((j >= x) && (j <= x + w) && (i >= y) && (i < y + h))
          {
            uint16_t idx = mirror_y ? (j - x) / 8 + ((h - 1 - (i - y))) * wb : (j - x) / 8 + (i - y) * wb;
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
        }
        _writeData(data);
      }
    }
  }
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_154c::writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                 int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, NULL, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_154c::writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                 int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
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
  _Init_Full();
  _writeCommand(0x10);
  for (int16_t i = 0; i < int16_t(HEIGHT); i++)
  {
    for (int16_t j = 0; j < int16_t(WIDTH); j += 8)
    {
      uint8_t data = 0xFF;
      if (black)
      {
        if ((j >= x1) && (j <= x1 + w) && (i >= y1) && (i < y1 + h))
        {
          // use wb_bitmap, h_bitmap of bitmap for index!
          //int16_t idx = mirror_y ? x_part / 8 + j + dx / 8 + ((h_bitmap - 1 - (y_part + i + dy))) * wb_bitmap : x_part / 8 + j + dx / 8 + (y_part + i + dy) * wb_bitmap;
          uint16_t idx = mirror_y ? (x_part + j - x1) / 8 + ((h_bitmap - 1 - (y_part + i - y1))) * wb_bitmap : (x_part + j - x1) / 8 + (y_part + i - y1) * wb_bitmap;
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
      }
      //_writeData(data);
      _writeData(bw2grey[(data & 0xF0) >> 4]);
      _writeData(bw2grey[data & 0x0F]);
    }
  }
  _writeCommand(0x13);
  for (int16_t i = 0; i < int16_t(HEIGHT); i++)
  {
    for (int16_t j = 0; j < int16_t(WIDTH); j += 8)
    {
      uint8_t data = 0xFF;
      if (color)
      {
        if ((j >= x1) && (j <= x1 + w) && (i >= y1) && (i < y1 + h))
        {
          // use wb_bitmap, h_bitmap of bitmap for index!
          uint16_t idx = mirror_y ? (x_part + j - x1) / 8 + ((h_bitmap - 1 - (y_part + i - y1))) * wb_bitmap : (x_part + j - x1) / 8 + (y_part + i - y1) * wb_bitmap;
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
      }
      _writeData(data);
    }
  }
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_154c::writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    writeImage(data1, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_154c::drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_154c::drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_154c::drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(black, color, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_154c::drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_154c::drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_154c::refresh(bool partial_update_mode)
{
  if (_paged) return;
  _Update_Full();
}

void GxEPD2_154c::refresh(int16_t x, int16_t y, int16_t w, int16_t h)
{
  _Update_Full();
}

void GxEPD2_154c::powerOff()
{
  _PowerOff();
}

void GxEPD2_154c::hibernate()
{
  _PowerOff();
  if (_rst >= 0)
  {
    // check if it supports this command!
    _writeCommand(0x07); // deep sleep
    _writeData(0xA5);    // check code
    _hibernating = true;
  }
}

void GxEPD2_154c::_setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
}

void GxEPD2_154c::_PowerOn()
{
  if (!_power_is_on)
  {
    _writeCommand(0x04);
    _waitWhileBusy("_PowerOn", power_on_time);
  }
  _power_is_on = true;
}

void GxEPD2_154c::_PowerOff()
{
  if (_power_is_on)
  {
    _writeCommand(0x50);
    _writeData(0x17);    //BD floating
    _writeCommand(0x82);     //to solve Vcom drop
    _writeData(0x00);
    _writeCommand(0x01);     //power setting
    _writeData(0x02);    //gate switch to external
    _writeData(0x00);
    _writeData(0x00);
    _writeData(0x00);
    delay(1500);     //delay 1.5S
    _writeCommand(0x02); // power off
    //_waitWhileBusy("_PowerOff", power_off_time); // causes busy timeout
    _power_is_on = false;
  }
}

void GxEPD2_154c::setPaged()
{
  _paged = true;
  _second_phase = false;
  _Init_Full();
  _writeCommand(0x10);
}

void GxEPD2_154c::_InitDisplay()
{
  if (_hibernating) _reset();
  _writeCommand(0x01);
  _writeData(0x07);
  _writeData(0x00);
  _writeData(0x08);
  _writeData(0x00);
  _writeCommand(0x06);
  _writeData(0x07);
  _writeData(0x07);
  _writeData(0x07);
  _PowerOn(); //power on needed here!
  _writeCommand(0x00);
  _writeData(0xcf);
  _writeCommand(0x50);
  _writeData(0x37);
  _writeCommand(0x30);
  _writeData(0x39);
  _writeCommand(0x61);
  _writeData(0xC8);
  _writeData(0x00);
  _writeData(0xC8);
  _writeCommand(0x82);
  _writeData(0x0E);
}

const uint8_t GxEPD2_154c::lut_20_vcom0[] PROGMEM = {  0x0E  , 0x14 , 0x01 , 0x0A , 0x06 , 0x04 , 0x0A , 0x0A , 0x0F , 0x03 , 0x03 , 0x0C , 0x06 , 0x0A , 0x00 };
const uint8_t GxEPD2_154c::lut_21_w[] PROGMEM = {  0x0E  , 0x14 , 0x01 , 0x0A , 0x46 , 0x04 , 0x8A , 0x4A , 0x0F , 0x83 , 0x43 , 0x0C , 0x86 , 0x0A , 0x04 };
const uint8_t GxEPD2_154c::lut_22_b[] PROGMEM = {  0x0E  , 0x14 , 0x01 , 0x8A , 0x06 , 0x04 , 0x8A , 0x4A , 0x0F , 0x83 , 0x43 , 0x0C , 0x06 , 0x4A , 0x04 };
const uint8_t GxEPD2_154c::lut_23_g1[] PROGMEM = { 0x8E  , 0x94 , 0x01 , 0x8A , 0x06 , 0x04 , 0x8A , 0x4A , 0x0F , 0x83 , 0x43 , 0x0C , 0x06 , 0x0A , 0x04 };
const uint8_t GxEPD2_154c::lut_24_g2[] PROGMEM = { 0x8E  , 0x94 , 0x01 , 0x8A , 0x06 , 0x04 , 0x8A , 0x4A , 0x0F , 0x83 , 0x43 , 0x0C , 0x06 , 0x0A , 0x04 };
const uint8_t GxEPD2_154c::lut_25_vcom1[] PROGMEM = {  0x03  , 0x1D , 0x01 , 0x01 , 0x08 , 0x23 , 0x37 , 0x37 , 0x01 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 };
const uint8_t GxEPD2_154c::lut_26_red0[] PROGMEM = { 0x83  , 0x5D , 0x01 , 0x81 , 0x48 , 0x23 , 0x77 , 0x77 , 0x01 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 };
const uint8_t GxEPD2_154c::lut_27_red1[] PROGMEM = { 0x03  , 0x1D , 0x01 , 0x01 , 0x08 , 0x23 , 0x37 , 0x37 , 0x01 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 };

void GxEPD2_154c::_Init_Full()
{
  _InitDisplay();
  _writeCommand(0x20);
  _writeDataPGM(lut_20_vcom0, sizeof(lut_20_vcom0));
  _writeCommand(0x21);
  _writeDataPGM(lut_21_w, sizeof(lut_21_w));
  _writeCommand(0x22);
  _writeDataPGM(lut_22_b, sizeof(lut_22_b));
  _writeCommand(0x23);
  _writeDataPGM(lut_23_g1, sizeof(lut_23_g1));
  _writeCommand(0x24);
  _writeDataPGM(lut_24_g2, sizeof(lut_24_g2));
  _writeCommand(0x25);
  _writeDataPGM(lut_25_vcom1, sizeof(lut_25_vcom1));
  _writeCommand(0x26);
  _writeDataPGM(lut_26_red0, sizeof(lut_26_red0));
  _writeCommand(0x27);
  _writeDataPGM(lut_27_red1, sizeof(lut_27_red1));
  _PowerOn();
}

void GxEPD2_154c::_Init_Part()
{
  _InitDisplay();
  _PowerOn();
}

void GxEPD2_154c::_Update_Full()
{
  _writeCommand(0x12); //display refresh
  _waitWhileBusy("_Update_Full", full_refresh_time);
}

void GxEPD2_154c::_Update_Part()
{
  _writeCommand(0x12); //display refresh
  _waitWhileBusy("_Update_Part", partial_refresh_time);
}
