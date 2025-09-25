// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: these e-papers require 3.3V supply AND data lines!
//
// GxEPD2_it78_1872x1404 class is based on Demo Example from Waveshare for Raspberry PI https://github.com/waveshare/IT8951/archive/master.zip
// Controller: IT8951 : https://www.waveshare.com/w/upload/1/18/IT8951_D_V0.2.4.3_20170728.pdf
//
// The GxEPD2_it78_1872x1404 driver class supports the Waveshare e-Paper IT8951 Driver HAT connected with SPI for the ED078KC2 7.8" e-paper panel (parallel IF)
// https://www.waveshare.com/product/displays/e-paper/7.8inch-e-paper-hat.htm
// This Driver HAT requires 5V power supply but works with 3.3V data lines; requires both MOSI and MISO SPI lines.
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#include "GxEPD2_it78_1872x1404.h"

#define VCOM  1500 //e.g. -1.53 = 1530 = 0x5FA

//Built in I80 Command Code
#define IT8951_TCON_SYS_RUN      0x0001
#define IT8951_TCON_STANDBY      0x0002
#define IT8951_TCON_SLEEP        0x0003
#define IT8951_TCON_REG_RD       0x0010
#define IT8951_TCON_REG_WR       0x0011
#define IT8951_TCON_LD_IMG       0x0020
#define IT8951_TCON_LD_IMG_AREA  0x0021
#define IT8951_TCON_LD_IMG_END   0x0022

//I80 User defined command code
#define USDEF_I80_CMD_DPY_AREA     0x0034
#define USDEF_I80_CMD_GET_DEV_INFO 0x0302
#define USDEF_I80_CMD_DPY_BUF_AREA 0x0037
#define USDEF_I80_CMD_VCOM       0x0039

//Rotate mode
#define IT8951_ROTATE_0     0
#define IT8951_ROTATE_90    1
#define IT8951_ROTATE_180   2
#define IT8951_ROTATE_270   3

//Pixel mode , BPP - Bit per Pixel
#define IT8951_2BPP   0
#define IT8951_3BPP   1
#define IT8951_4BPP   2
#define IT8951_8BPP   3

//Endian Type
#define IT8951_LDIMG_L_ENDIAN   0
#define IT8951_LDIMG_B_ENDIAN   1

#define SYS_REG_BASE 0x0000
#define I80CPCR (SYS_REG_BASE + 0x04)
#define MCSR_BASE_ADDR 0x0200
#define LISAR (MCSR_BASE_ADDR + 0x0008)

GxEPD2_it78_1872x1404::GxEPD2_it78_1872x1404(int16_t cs, int16_t dc, int16_t rst, int16_t busy) :
  GxEPD2_EPD(cs, dc, rst, busy, LOW, 10000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate),
  _spi_settings(24000000, MSBFIRST, SPI_MODE0),
  _spi_settings_for_read(1000000, MSBFIRST, SPI_MODE0)
{
}

void GxEPD2_it78_1872x1404::init(uint32_t serial_diag_bitrate)
{
  init(serial_diag_bitrate, true, 20, false);
}

void GxEPD2_it78_1872x1404::init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration, bool pulldown_rst_mode)
{
  GxEPD2_EPD::init(serial_diag_bitrate, initial, reset_duration, pulldown_rst_mode);

  // we need a long reset pulse
  if (_rst >= 0)
  {
    digitalWrite(_rst, LOW);
    delay(200);
    digitalWrite(_rst, HIGH);
    delay(200);
    _waitWhileBusy("init reset_to_ready", reset_to_ready_time);
  }

  _writeCommand16(USDEF_I80_CMD_GET_DEV_INFO);
  _waitWhileBusy("GetIT8951SystemInfo", power_on_time);
  _readData16((uint16_t*)&IT8951DevInfo, sizeof(IT8951DevInfo) / 2);
  if (_diag_enabled)
  {
    //Show Device information of IT8951
    Serial.print("Panel(W,H) = ("); Serial.print(IT8951DevInfo.usPanelW); Serial.print(", "); Serial.print(IT8951DevInfo.usPanelH); Serial.println(")");
    Serial.print("Image Buffer Address = 0x"); Serial.println(uint32_t(IT8951DevInfo.usImgBufAddrL) | (uint32_t(IT8951DevInfo.usImgBufAddrH) << 16), HEX);
    //Show Firmware and LUT Version
    Serial.print("FW Version = "); Serial.println((char*)IT8951DevInfo.usFWVersion);
    Serial.print("LUT Version = "); Serial.println((char*)IT8951DevInfo.usLUTVersion);
  }
  //Set to Enable I80 Packed mode
  _IT8951WriteReg(I80CPCR, 0x0001);
  if (VCOM != _IT8951GetVCOM())
  {
    _IT8951SetVCOM(VCOM);
    if (_diag_enabled)
    {
      Serial.print("set VCOM = -"); Serial.println((float)_IT8951GetVCOM() / 1000);
    }
  }
  //Serial.print("set VCOM = -"); Serial.println((float)_IT8951GetVCOM() / 1000);
}

void GxEPD2_it78_1872x1404::clearScreen(uint8_t value)
{
  _initial_write = false; // initial full screen buffer clean done
  if (_initial_refresh) _Init_Full();
  else _Init_Part();
  _initial_refresh = false;
  _setPartialRamArea(0, 0, WIDTH, HEIGHT);
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  _transfer16(0x0000); // preamble for write data
  _waitWhileBusy2("clearScreen preamble", default_wait_time);
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT); i++)
  {
    SPI.transfer(value);
#if defined(ESP8266) || defined(ESP32)
    if (0 == i % 10000) yield();
#endif
  }
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
  _writeCommand16(IT8951_TCON_LD_IMG_END);
  _waitWhileBusy2("clearScreen load end", default_wait_time);
  _refresh(0, 0, WIDTH, HEIGHT, false);
}

void GxEPD2_it78_1872x1404::writeScreenBuffer(uint8_t value)
{
  if (_initial_refresh) clearScreen(value);
  else _writeScreenBuffer(value);
}

void GxEPD2_it78_1872x1404::_writeScreenBuffer(uint8_t value)
{
  _initial_write = false; // initial full screen buffer clean done
  if (!_using_partial_mode) _Init_Part();
  _setPartialRamArea(0, 0, WIDTH, HEIGHT);
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  _transfer16(0x0000); // preamble for write data
  _waitWhileBusy2("clearScreen preamble", default_wait_time);
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT); i++)
  {
    SPI.transfer(value);
#if defined(ESP8266) || defined(ESP32)
    if (0 == i % 10000) yield();
#endif
  }
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
  _writeCommand16(IT8951_TCON_LD_IMG_END);
  _waitWhileBusy2("_writeScreenBuffer load end", default_wait_time);
}

void GxEPD2_it78_1872x1404::writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
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
  if (!_using_partial_mode) _Init_Part();
  _setPartialRamArea(x1, y1, w1, h1);
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  _transfer16(0x0000); // preamble for write data
  _waitWhileBusy2("writeImage preamble", default_wait_time);
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data;
      // use wb, h of bitmap for index!
      uint32_t idx = mirror_y ? uint32_t(j + dx / 8) + uint32_t((h - 1 - (i + dy))) * uint32_t(wb) : uint32_t(j + dx / 8) + uint32_t(i + dy) * uint32_t(wb);
      if (pgm)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[idx]);
#else
        data = bitmap[idx];
#endif
      }
      else
      {
        data = bitmap[idx];
      }
      if (invert) data = ~data;
      _send8pixel(~data);
    }
#if defined(ESP8266) || defined(ESP32)
    yield();
#endif
  }
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
  _writeCommand16(IT8951_TCON_LD_IMG_END);
  _waitWhileBusy2("writeImage load end", default_wait_time);
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_it78_1872x1404::writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
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
  _setPartialRamArea(x1, y1, w1, h1);
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  _transfer16(0x0000); // preamble for write data
  _waitWhileBusy2("writeImage preamble", default_wait_time);
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data;
      // use wb_bitmap, h_bitmap of bitmap for index!
      uint32_t idx = mirror_y ? x_part / 8 + (j + dx / 8) + uint32_t((h_bitmap - 1 - (y_part + i + dy))) * uint32_t(wb_bitmap) : x_part / 8 + j + dx / 8 + uint32_t(y_part + i + dy) * uint32_t(wb_bitmap);
      if (pgm)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[idx]);
#else
        data = bitmap[idx];
#endif
      }
      else
      {
        data = bitmap[idx];
      }
      if (invert) data = ~data;
      _send8pixel(~data);
    }
  }
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
  _writeCommand16(IT8951_TCON_LD_IMG_END);
  _waitWhileBusy2("writeImage load end", default_wait_time);
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_it78_1872x1404::writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    writeImage(black, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_it78_1872x1404::writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                 int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    writeImagePart(black, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_it78_1872x1404::writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
    delay(1); // yield() to avoid WDT on ESP8266 and ESP32
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
    SPI.beginTransaction(_spi_settings);
    if (_cs >= 0) digitalWrite(_cs, LOW);
    _transfer16(0x0000); // preamble for write data
    _waitWhileBusy2("writeNative preamble", default_wait_time);
    for (int16_t i = 0; i < h1; i++)
    {
      for (int16_t j = 0; j < w1; j++)
      {
        uint8_t data;
        // use w, h of bitmap for index!
        uint32_t idx = mirror_y ? uint32_t(j + dx) + uint32_t((h - 1 - (i + dy))) * uint32_t(w) : uint32_t(j + dx) + uint32_t(i + dy) * uint32_t(w);
        if (pgm)
        {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
          data = pgm_read_byte(&data1[idx]);
#else
          data = data1[idx];
#endif
        }
        else
        {
          data = data1[idx];
        }
        if (invert) data = ~data;
        SPI.transfer(data);
      }
#if defined(ESP8266) || defined(ESP32)
      yield();
#endif
    }
    if (_cs >= 0) digitalWrite(_cs, HIGH);
    SPI.endTransaction();
    _writeCommand16(IT8951_TCON_LD_IMG_END);
    _waitWhileBusy2("writeNative load end", default_wait_time);
    delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  }
}

void GxEPD2_it78_1872x1404::drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
  _refresh(x, y, w, h, true);
}

void GxEPD2_it78_1872x1404::drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  _refresh(x, y, w, h, true);
}

void GxEPD2_it78_1872x1404::drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(black, color, x, y, w, h, invert, mirror_y, pgm);
  _refresh(x, y, w, h, true);
}

void GxEPD2_it78_1872x1404::drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  _refresh(x, y, w, h, true);
}

void GxEPD2_it78_1872x1404::drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
  _refresh(x, y, w, h, false);
}

void GxEPD2_it78_1872x1404::refresh(bool partial_update_mode)
{
  _refresh(0, 0, WIDTH, HEIGHT, partial_update_mode);
}

void GxEPD2_it78_1872x1404::refresh(int16_t x, int16_t y, int16_t w, int16_t h)
{
  _refresh(x, y, w, h, true);
}

void GxEPD2_it78_1872x1404::_refresh(int16_t x, int16_t y, int16_t w, int16_t h, bool partial_update_mode)
{
  //x -= x % 8; // byte boundary
  //w -= x % 8; // byte boundary
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  int16_t w1 = x + w < int16_t(WIDTH) ? w : int16_t(WIDTH) - x; // limit
  int16_t h1 = y + h < int16_t(HEIGHT) ? h : int16_t(HEIGHT) - y; // limit
  w1 -= x1 - x;
  h1 -= y1 - y;
  //Send I80 Display Command (User defined command of IT8951)
  _writeCommand16(USDEF_I80_CMD_DPY_AREA); //0x0034
  _waitWhileBusy2("refresh cmd", refresh_cmd_time);
  //Write arguments
  _writeData16(x1);
  _waitWhileBusy2("refresh x", refresh_par_time);
  _writeData16(y1);
  _waitWhileBusy2("refresh y", refresh_par_time);
  _writeData16(w1);
  _waitWhileBusy2("refresh w", refresh_par_time);
  _writeData16(h1);
  _waitWhileBusy2("refresh h", refresh_par_time);
  _writeData16(partial_update_mode ? 1 : 2); // mode
  _waitWhileBusy("refresh", full_refresh_time);
}

void GxEPD2_it78_1872x1404::powerOff(void)
{
  _PowerOff();
}

void GxEPD2_it78_1872x1404::hibernate()
{
  if (_power_is_on) _PowerOff();
  if (_rst >= 0)
  {
    // this does not work, does not reduce power, uses more than in stand by mode
    //delay(1000);
    //_IT8951Sleep();
    //delay(1000);
    //_hibernating = true;
  }
}

void GxEPD2_it78_1872x1404::_send8pixel(uint8_t data)
{
  for (uint8_t j = 0; j < 8; j++)
  {
    SPI.transfer(data & 0x80 ? 0x00 : 0xFF);
    data <<= 1;
  }
}

void GxEPD2_it78_1872x1404::_setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  //_IT8951WriteReg(LISAR + 2 , IT8951DevInfo.usImgBufAddrH);
  //_IT8951WriteReg(LISAR , IT8951DevInfo.usImgBufAddrL);
  uint16_t usArg[5];
  //usArg[0] = (IT8951_LDIMG_L_ENDIAN << 8 ) | (IT8951_8BPP << 4) | (IT8951_ROTATE_0);
  usArg[0] = (IT8951_LDIMG_B_ENDIAN << 8 ) | (IT8951_8BPP << 4) | (IT8951_ROTATE_0);
  usArg[1] = x;
  usArg[2] = y;
  usArg[3] = w;
  usArg[4] = h;
  _writeCommandData16(IT8951_TCON_LD_IMG_AREA , usArg , 5);
}

void GxEPD2_it78_1872x1404::_PowerOn()
{
  if (!_power_is_on)
  {
    _IT8951SystemRun();
    _waitWhileBusy("_PowerOn", power_on_time);
  }
  _power_is_on = true;
}

void GxEPD2_it78_1872x1404::_PowerOff()
{
  _IT8951StandBy();
  _waitWhileBusy("_PowerOff", power_off_time);
  _power_is_on = false;
  _using_partial_mode = false;
}

void GxEPD2_it78_1872x1404::_InitDisplay()
{
  // we need a long reset pulse
  if (_hibernating && (_rst >= 0))
  {
    digitalWrite(_rst, LOW);
    delay(200);
    digitalWrite(_rst, HIGH);
    delay(200);
  }
}

void GxEPD2_it78_1872x1404::_Init_Full()
{
  _InitDisplay();
  _PowerOn();
  _using_partial_mode = false;
}

void GxEPD2_it78_1872x1404::_Init_Part()
{
  _InitDisplay();
  _PowerOn();
  _using_partial_mode = true;
}

void GxEPD2_it78_1872x1404::_waitWhileBusy2(const char* comment, uint16_t busy_time)
{
  if (_busy >= 0)
  {
    unsigned long start = micros();
    while (1)
    {
      if (digitalRead(_busy) != _busy_level) break;
      delay(1);
      if (micros() - start > _busy_timeout)
      {
        Serial.println("Busy Timeout!");
        break;
      }
    }
    if (comment)
    {
#if !defined(DISABLE_DIAGNOSTIC_OUTPUT)
      if (_diag_enabled)
      {
        unsigned long elapsed = micros() - start;
        if (elapsed > diag_min_time * 1000)
        {
          Serial.print(comment);
          Serial.print(" : ");
          Serial.println(elapsed);
        }
      }
#endif
    }
    (void) start;
  }
  else delay(busy_time);
}

uint16_t GxEPD2_it78_1872x1404::_transfer16(uint16_t value)
{
  uint16_t rv = SPI.transfer(value >> 8) << 8;
  return (rv | SPI.transfer(value));
}

void GxEPD2_it78_1872x1404::_writeCommand16(uint16_t c)
{
  String s = String("_writeCommand16(0x") + String(c, HEX) + String(")");
  _waitWhileBusy2(s.c_str(), default_wait_time);
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  _transfer16(0x6000); // preamble for write command
  _waitWhileBusy2("_writeCommand16 preamble", default_wait_time);
  _transfer16(c);
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
  //_waitWhileBusy(s.c_str(), default_wait_time);
}

void GxEPD2_it78_1872x1404::_writeData16(uint16_t d)
{
  _waitWhileBusy2("_writeData16", default_wait_time);
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  _transfer16(0x0000); // preamble for write data
  _waitWhileBusy2("_writeData16 preamble", default_wait_time);
  _transfer16(d);
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}

void GxEPD2_it78_1872x1404::_writeData16(const uint16_t* d, uint32_t n)
{
  _waitWhileBusy2("_writeData16", default_wait_time);
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  _transfer16(0x0000); // preamble for write data
  _waitWhileBusy2("_writeData16 preamble", default_wait_time);
  for (uint32_t i = 0; i < n; i++)
  {
    _transfer16(*d++);
  }
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}

uint16_t GxEPD2_it78_1872x1404::_readData16()
{
  _waitWhileBusy2("_readData16", default_wait_time);
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  _transfer16(0x1000); // preamble for read data
  _waitWhileBusy2("_readData16 preamble", default_wait_time);
  _transfer16(0); // dummy
  _waitWhileBusy2("_readData16 dummy", default_wait_time);
  uint16_t rv = _transfer16(0);
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
  return rv;
}

void GxEPD2_it78_1872x1404::_readData16(uint16_t* d, uint32_t n)
{
  _waitWhileBusy2("_readData16", default_wait_time);
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  _transfer16(0x1000); // preamble for read data
  _waitWhileBusy2("_readData16 preamble", default_wait_time);
  _transfer16(0); // dummy
  _waitWhileBusy2("_readData16 dummy", default_wait_time);
  for (uint32_t i = 0; i < n; i++)
  {
    *d++ = _transfer16(0);
    //_waitWhileBusy("_readData16 data", default_wait_time);
  }
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}

void GxEPD2_it78_1872x1404::_writeCommandData16(uint16_t c, const uint16_t* d, uint16_t n)
{
  _writeCommand16(c);
  for (uint16_t i = 0; i < n; i++)
  {
    _writeData16(d[i]);
  }
}

void GxEPD2_it78_1872x1404::_IT8951SystemRun()
{
  _writeCommand16(IT8951_TCON_SYS_RUN);
}

void GxEPD2_it78_1872x1404::_IT8951StandBy()
{
  _writeCommand16(IT8951_TCON_STANDBY);
}

void GxEPD2_it78_1872x1404::_IT8951Sleep()
{
  _writeCommand16(IT8951_TCON_SLEEP);
}

uint16_t GxEPD2_it78_1872x1404::_IT8951ReadReg(uint16_t usRegAddr)
{
  uint16_t usData;

  //Send Cmd and Register Address
  _writeCommand16(IT8951_TCON_REG_RD);
  _writeData16(usRegAddr);
  //Read data from Host Data bus
  usData = _readData16();
  return usData;
}

void GxEPD2_it78_1872x1404::_IT8951WriteReg(uint16_t usRegAddr, uint16_t usValue)
{
  //Send Cmd , Register Address and Write Value
  _writeCommand16(IT8951_TCON_REG_WR);
  _writeData16(usRegAddr);
  _writeData16(usValue);
}

uint16_t GxEPD2_it78_1872x1404::_IT8951GetVCOM(void)
{
  uint16_t vcom;

  _writeCommand16(USDEF_I80_CMD_VCOM);
  _waitWhileBusy2("_IT8951GetVCOM", default_wait_time);
  _writeData16(0);
  //Read data from Host Data bus
  vcom = _readData16();
  return vcom;
}

void GxEPD2_it78_1872x1404::_IT8951SetVCOM(uint16_t vcom)
{
  _writeCommand16(USDEF_I80_CMD_VCOM);
  _waitWhileBusy2("_IT8951SetVCOM", default_wait_time);
  _writeData16(1);
  //Read data from Host Data bus
  _writeData16(vcom);
  _waitWhileBusy2("_IT8951SetVCOM", set_vcom_time);
}
