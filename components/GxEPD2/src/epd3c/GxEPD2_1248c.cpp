// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// based on Demo Example from Good Display:
// Panel: GDEY1248Z51 : https://www.good-display.com/product/422.html
// Controller: UC8179 : https://v4.cecdn.yun300.cn/100001_1909185148/UC8179.pdf
// Connection Module : e.g. DESPI-C1248 : https://buyepaper.com/products/connection-board-for-1248-inch-large-eaper-e-ink-display-demo-despi-c1248
// Demo Kit : e.g. DESPI-1248 : https://buy-lcd.com/products/demo-kit-driver-development-board-for-1248-inch-large-eaper-e-ink-display
// Waveshare Kit : https://www.waveshare.com/product/displays/e-paper/epaper-1/12.48inch-e-paper-module-b.htm
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

#include "GxEPD2_1248c.h"

#if defined(ESP32)
// general constructor for use with all parameters on ESP32, e.g. for Waveshare ESP32 driver board mounted on connection board
GxEPD2_1248c::GxEPD2_1248c(int16_t sck, int16_t miso, int16_t mosi,
                           int16_t cs_m1, int16_t cs_s1, int16_t cs_m2, int16_t cs_s2,
                           int16_t dc1, int16_t dc2, int16_t rst1, int16_t rst2,
                           int16_t busy_m1, int16_t busy_s1, int16_t busy_m2, int16_t busy_s2) :
  GxEPD2_EPD(cs_m1, dc1, rst1, busy_m1, LOW, 20000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate),
  _sck(sck), _miso(miso), _mosi(mosi), _dc1(dc1), _dc2(dc2), _rst1(rst1), _rst2(rst2),
  _cs_m1(cs_m1), _cs_s1(cs_s1), _cs_m2(cs_m2), _cs_s2(cs_s2),
  _busy_m1(busy_m1), _busy_s1(busy_s1), _busy_m2(busy_m2), _busy_s2(busy_s2),
  _temperature(20),
  M1(648, 492, false, cs_m1, dc1),
  S1(656, 492, false, cs_s1, dc1),
  M2(656, 492, true, cs_m2, dc2),
  S2(648, 492, true, cs_s2, dc2)
{
}
#endif

// general constructor for use with standard SPI pins, default SCK, MISO and MOSI
GxEPD2_1248c::GxEPD2_1248c(int16_t cs_m1, int16_t cs_s1, int16_t cs_m2, int16_t cs_s2,
                           int16_t dc1, int16_t dc2, int16_t rst1, int16_t rst2,
                           int16_t busy_m1, int16_t busy_s1, int16_t busy_m2, int16_t busy_s2) :
  GxEPD2_EPD(cs_m1, dc1, rst1, busy_m1, LOW, 20000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate),
  _sck(SCK), _miso(MISO), _mosi(MOSI), _dc1(dc1), _dc2(dc2), _rst1(rst1), _rst2(rst2),
  _cs_m1(cs_m1), _cs_s1(cs_s1), _cs_m2(cs_m2), _cs_s2(cs_s2),
  _busy_m1(busy_m1), _busy_s1(busy_s1), _busy_m2(busy_m2), _busy_s2(busy_s2),
  _temperature(20),
  M1(648, 492, false, cs_m1, dc1),
  S1(656, 492, false, cs_s1, dc1),
  M2(656, 492, true, cs_m2, dc2),
  S2(648, 492, true, cs_s2, dc2)
{
}

// constructor with minimal parameter set, standard SPI, dc1 and dc2, rst1 and rst2 to one pin, one busy used (can be -1)
GxEPD2_1248c::GxEPD2_1248c(int16_t cs_m1, int16_t cs_s1, int16_t cs_m2, int16_t cs_s2, int16_t dc, int16_t rst, int16_t busy) :
  GxEPD2_EPD(23, 25, 33, 32, LOW, 20000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate),
  _sck(SCK), _miso(MISO), _mosi(MOSI), _dc1(dc), _dc2(dc), _rst1(rst), _rst2(rst),
  _cs_m1(cs_m1), _cs_s1(cs_s1), _cs_m2(cs_m2), _cs_s2(cs_s2),
  _busy_m1(busy), _busy_s1(busy), _busy_m2(busy), _busy_s2(busy),
  _temperature(20),
  M1(648, 492, false, cs_m1, dc),
  S1(656, 492, false, cs_s1, dc),
  M2(656, 492, true, cs_m2, dc),
  S2(648, 492, true, cs_s2, dc)
{
}

void GxEPD2_1248c::init(uint32_t serial_diag_bitrate)
{
  init(serial_diag_bitrate, true, 20, false);
}

void GxEPD2_1248c::init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration, bool pulldown_rst_mode)
{
  _initial_write = initial;
  _initial_refresh = initial;
  _using_partial_mode = false;
  if (serial_diag_bitrate > 0)
  {
    Serial.begin(serial_diag_bitrate);
    _diag_enabled = true;
    Serial.println(); Serial.println("GxEPD2_1248c::init()");
  }
  pinMode(_cs_m1,  OUTPUT);
  pinMode(_cs_s1,  OUTPUT);
  pinMode(_cs_m2,  OUTPUT);
  pinMode(_cs_s2,  OUTPUT);
  pinMode(_dc1,  OUTPUT);
  pinMode(_dc2,  OUTPUT);
  pinMode(_rst1,  OUTPUT);
  pinMode(_rst2,  OUTPUT);
  pinMode(_busy_m1,  INPUT);
  pinMode(_busy_s1,  INPUT);
  pinMode(_busy_m2,  INPUT);
  pinMode(_busy_s2,  INPUT);
  digitalWrite(_cs_m1,  HIGH);
  digitalWrite(_cs_s1,  HIGH);
  digitalWrite(_cs_m2,  HIGH);
  digitalWrite(_cs_s2,  HIGH);
  _initSPI();
  _reset();
  // only relevant for full refresh, comment out if 20 is ok
  _getMasterTemperature();
}

void GxEPD2_1248c::clearScreen(uint8_t value)
{
  writeScreenBuffer(value);
  refresh(true);
}

void GxEPD2_1248c::_writeScreenBuffer(uint8_t command, uint8_t value)
{
  M1.writeScreenBuffer(command, value);
  S1.writeScreenBuffer(command, value);
  M2.writeScreenBuffer(command, value);
  S2.writeScreenBuffer(command, value);
}

void GxEPD2_1248c::writeScreenBuffer(uint8_t value)
{
  if (!_using_partial_mode) _Init_Part();
  _writeScreenBuffer(0x10, value); // black
  _writeScreenBuffer(0x13, ~value); // color
  _initial_write = false; // initial full screen buffer clean done
}

void GxEPD2_1248c::writeScreenBuffer(uint8_t black_value, uint8_t color_value)
{
  if (!_using_partial_mode) _Init_Part();
  _writeScreenBuffer(0x10, black_value); // black
  _writeScreenBuffer(0x13, color_value); // color
  _initial_write = false; // initial full screen buffer clean done
}

void GxEPD2_1248c::_writeImage(uint8_t command, const uint8_t* bitmap, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (bitmap)
  {
    S2.writeImagePart(command, bitmap, 0, 0, w, h, x, y, w, h, invert, mirror_y, pgm);
    M2.writeImagePart(command, bitmap, 0, 0, w, h, x - S2.WIDTH, y, w, h, invert, mirror_y, pgm);
    M1.writeImagePart(command, bitmap, 0, 0, w, h, x, y - S2.HEIGHT, w, h, invert, mirror_y, pgm);
    S1.writeImagePart(command, bitmap, 0, 0, w, h, x - M1.WIDTH, y - M2.HEIGHT, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_1248c::writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  if (!_using_partial_mode) _Init_Part();
  _writeImage(0x10, bitmap, x, y, w, h, invert, mirror_y, pgm); // black
  _writeScreenBuffer(0x13, 0x0); // color (white)
}

void GxEPD2_1248c::_writeImagePart(uint8_t command, const uint8_t* bitmap, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                   int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (bitmap)
  {
    S2.writeImagePart(command, bitmap, x_part, y_part, w, h, x, y, w, h, invert, mirror_y, pgm);
    M2.writeImagePart(command, bitmap, x_part, y_part, w, h, x - S2.WIDTH, y, w, h, invert, mirror_y, pgm);
    M1.writeImagePart(command, bitmap, x_part, y_part, w, h, x, y - S2.HEIGHT, w, h, invert, mirror_y, pgm);
    S1.writeImagePart(command, bitmap, x_part, y_part, w, h, x - M1.WIDTH, y - M2.HEIGHT, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_1248c::writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                  int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  if (!_using_partial_mode) _Init_Part();
  _writeImagePart(0x10, bitmap, x_part, y_part, w, h, x, y, w, h, invert, mirror_y, pgm); // black
}

void GxEPD2_1248c::writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  if (!_using_partial_mode) _Init_Part();
  _writeImage(0x10, black, x, y, w, h, invert, mirror_y, pgm);
  _writeImage(0x13, color, x, y, w, h, !invert, mirror_y, pgm);
}

void GxEPD2_1248c::writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                  int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  if (!_using_partial_mode) _Init_Part();
  _writeImagePart(0x10, black, x_part, y_part, w, h, x, y, w, h, invert, mirror_y, pgm);
  _writeImagePart(0x13, color, x_part, y_part, w, h, x, y, w, h, !invert, mirror_y, pgm);
}

void GxEPD2_1248c::writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(data1, data2, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_1248c::drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_1248c::drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                 int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_1248c::drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(black, color, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_1248c::drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                 int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_1248c::drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_1248c::refresh(bool partial_update_mode)
{
  if (partial_update_mode) refresh(0, 0, WIDTH, HEIGHT);
  else
  {
    if (_using_partial_mode) _Init_Full();
    _Update_Full();
    _initial_refresh = false; // initial full update done
  }
}

void GxEPD2_1248c::refresh(int16_t x, int16_t y, int16_t w, int16_t h)
{
  if (_initial_refresh) return refresh(false); // initial update needs be full update
  if (!_using_partial_mode) _Init_Part();
  _Update_Part();
}

void GxEPD2_1248c::powerOff(void)
{
  _PowerOff();
}

void GxEPD2_1248c::hibernate()
{
  _PowerOff();
  if (_rst >= 0)
  {
    _writeCommandAll(0x07); // deep sleep
    _writeDataAll(0xA5);    // check code
    _hibernating = true;
  }
}

void GxEPD2_1248c::_reset(void)
{
  digitalWrite(_rst1, LOW);
  digitalWrite(_rst2, LOW);
  delay(200);
  digitalWrite(_rst1, HIGH);
  digitalWrite(_rst2, HIGH);
  delay(200);
  _hibernating = false;
}

void GxEPD2_1248c::_initSPI()
{
#if defined(ESP32)
  if ((SCK != _sck) || (MISO != _miso) || (MOSI != _mosi))
  {
    SPI.end();
    SPI.begin(_sck, _miso, _mosi, _cs_m1);
  }
  else SPI.begin();
#else
  SPI.begin();
#endif
}

void GxEPD2_1248c::_PowerOn()
{
  if (!_power_is_on)
  {
    _writeCommandMaster(0x04);
    _waitWhileAnyBusy("_PowerOn", power_on_time);
  }
  _power_is_on = true;
}

void GxEPD2_1248c::_PowerOff()
{
  if (_power_is_on)
  {
    _writeCommandMaster(0x02); // power off
    _waitWhileAnyBusy("_PowerOff", power_on_time);
  }
  _power_is_on = false;
  _using_partial_mode = false;
}

void GxEPD2_1248c::_InitDisplay()
{
  if (_hibernating) _reset();
  //panel setting
  M1.writeCommand(0x00);
  M1.writeData(0x0f);  //KW-3f   KWR-2F  BWROTP 0f BWOTP 1f
  S1.writeCommand(0x00);
  S1.writeData(0x0f);
  M2.writeCommand(0x00);
  M2.writeData(0x03); // reverse scan
  S2.writeCommand(0x00);
  S2.writeData(0x03); // reverse scan
  // booster soft start
  M1.writeCommand(0x06);
  M1.writeData(0x27);  //A
  M1.writeData(0x27);  //B
  M1.writeData(0x18);  //C
  M1.writeData(0x17);
  M2.writeCommand(0x06);
  M2.writeData(0x27);
  M2.writeData(0x27);
  M2.writeData(0x18);
  M2.writeData(0x17);
  //resolution setting
  M1.writeCommand(0x61);
  M1.writeData(0x02);
  M1.writeData(0x88);  //source 648
  M1.writeData(0x01);  //gate 492
  M1.writeData(0xEC);
  S1.writeCommand(0x61);
  S1.writeData(0x02);
  S1.writeData(0x90);  //source 656
  S1.writeData(0x01);  //gate 492
  S1.writeData(0xEC);
  M2.writeCommand(0x61);
  M2.writeData(0x02);
  M2.writeData(0x90);  //source 656
  M2.writeData(0x01);  //gate 492
  M2.writeData(0xEC);
  S2.writeCommand(0x61);
  S2.writeData(0x02);
  S2.writeData(0x88);  //source 648
  S2.writeData(0x01);  //gate 492
  S2.writeData(0xEC);
  _writeCommandAll(0x15); //DUSPI
  _writeDataAll(0x20); // normal (single DIN) SPI
  _writeCommandAll(0x50); //Vcom and data interval setting
  _writeDataAll(0x11);  //Border KW
  _writeDataAll(0x07);

  _writeCommandAll(0x60); //TCON
  _writeDataAll(0x22);

  _writeCommandAll(0xE3);
  _writeDataAll(0x00);

  _writeCommandAll(0xe0); //Cascade setting
  _writeDataAll(0x03);

  _writeCommandAll(0xe5); //Force temperature
  _writeDataAll(_temperature);
}

void GxEPD2_1248c::_Init_Full()
{
  Serial.println("_Init_Full");
  _InitDisplay();
  // LUT from OTP
  M1.writeCommand(0x00);
  M1.writeData(0x0f);  //KW-3f   KWR-2F  BWROTP 0f BWOTP 1f
  S1.writeCommand(0x00);
  S1.writeData(0x0f);
  M2.writeCommand(0x00);
  M2.writeData(0x03); // reverse scan
  S2.writeCommand(0x00);
  S2.writeData(0x03); // reverse scan
  _PowerOn();
  _using_partial_mode = false;
}

void GxEPD2_1248c::_Init_Part()
{
  Serial.println("_Init_Part");
  _InitDisplay();
  // LUT from OTP
  M1.writeCommand(0x00);
  M1.writeData(0x0f);  //KW-3f   KWR-2F  BWROTP 0f BWOTP 1f
  S1.writeCommand(0x00);
  S1.writeData(0x0f);
  M2.writeCommand(0x00);
  M2.writeData(0x03); // reverse scan
  S2.writeCommand(0x00);
  S2.writeData(0x03); // reverse scan
  _PowerOn();
  _using_partial_mode = true;
}

void GxEPD2_1248c::_Update_Full()
{
  _writeCommandAll(0x12); //display refresh
  _waitWhileAnyBusy("_Update_Full", full_refresh_time);
}

void GxEPD2_1248c::_Update_Part()
{
  _writeCommandAll(0x12); //display refresh
  _waitWhileAnyBusy("_Update_Part", partial_refresh_time);
}

void GxEPD2_1248c::_writeCommandMaster(uint8_t c)
{
  SPI.beginTransaction(_spi_settings);
  digitalWrite(_dc1, LOW);
  digitalWrite(_dc2, LOW);
  digitalWrite(_cs_m1, LOW);
  digitalWrite(_cs_m2, LOW);
  SPI.transfer(c);
  digitalWrite(_cs_m1, HIGH);
  digitalWrite(_cs_m2, HIGH);
  digitalWrite(_dc1, HIGH);
  digitalWrite(_dc2, HIGH);
  SPI.endTransaction();
}

void GxEPD2_1248c::_writeDataMaster(uint8_t d)
{
  SPI.beginTransaction(_spi_settings);
  digitalWrite(_cs_m1, LOW);
  digitalWrite(_cs_m2, LOW);
  SPI.transfer(d);
  digitalWrite(_cs_m1, HIGH);
  digitalWrite(_cs_m2, HIGH);
  SPI.endTransaction();
}

void GxEPD2_1248c::_writeCommandAll(uint8_t c)
{
  SPI.beginTransaction(_spi_settings);
  digitalWrite(_dc1, LOW);
  digitalWrite(_dc2, LOW);
  digitalWrite(_cs_m1, LOW);
  digitalWrite(_cs_s1, LOW);
  digitalWrite(_cs_m2, LOW);
  digitalWrite(_cs_s2, LOW);
  SPI.transfer(c);
  digitalWrite(_cs_m1, HIGH);
  digitalWrite(_cs_s1, HIGH);
  digitalWrite(_cs_m2, HIGH);
  digitalWrite(_cs_s2, HIGH);
  digitalWrite(_dc1, HIGH);
  digitalWrite(_dc2, HIGH);
  SPI.endTransaction();
}

void GxEPD2_1248c::_writeDataAll(uint8_t d)
{
  SPI.beginTransaction(_spi_settings);
  digitalWrite(_cs_m1, LOW);
  digitalWrite(_cs_s1, LOW);
  digitalWrite(_cs_m2, LOW);
  digitalWrite(_cs_s2, LOW);
  SPI.transfer(d);
  digitalWrite(_cs_m1, HIGH);
  digitalWrite(_cs_s1, HIGH);
  digitalWrite(_cs_m2, HIGH);
  digitalWrite(_cs_s2, HIGH);
  SPI.endTransaction();
}

void GxEPD2_1248c::_writeDataPGM_All(const uint8_t* data, uint16_t n, int16_t fill_with_zeroes)
{
  SPI.beginTransaction(_spi_settings);
  digitalWrite(_cs_m1, LOW);
  digitalWrite(_cs_s1, LOW);
  digitalWrite(_cs_m2, LOW);
  digitalWrite(_cs_s2, LOW);
  for (uint16_t i = 0; i < n; i++)
  {
    SPI.transfer(pgm_read_byte(&*data++));
  }
  while (fill_with_zeroes > 0)
  {
    SPI.transfer(0x00);
    fill_with_zeroes--;
  }
  digitalWrite(_cs_m1, HIGH);
  digitalWrite(_cs_s1, HIGH);
  digitalWrite(_cs_m2, HIGH);
  digitalWrite(_cs_s2, HIGH);
  SPI.endTransaction();
}

void GxEPD2_1248c::_waitWhileAnyBusy(const char* comment, uint16_t busy_time)
{
  if (_busy_m1 >= 0)
  {
    delay(1); // add some margin to become active
    unsigned long start = micros();
    while (1)
    {
      delay(1); // add some margin to become active
      bool nb_m1 = _busy_m1 >= 0 ? _busy_level != digitalRead(_busy_m1) : true;
      bool nb_s1 = _busy_m1 >= 0 ? _busy_level != digitalRead(_busy_s1) : true;
      bool nb_m2 = _busy_m1 >= 0 ? _busy_level != digitalRead(_busy_m2) : true;
      bool nb_s2 = _busy_m1 >= 0 ? _busy_level != digitalRead(_busy_s2) : true;
      if (nb_m1 && nb_s1 && nb_m2 && nb_s2) break;
      delay(1);
      if (micros() - start > _busy_timeout)
      {
        Serial.println("Busy Timeout!");
        break;
      }
    }
    if (comment)
    {
      if (_diag_enabled)
      {
        unsigned long elapsed = micros() - start;
        Serial.print(comment);
        Serial.print(" : ");
        Serial.println(elapsed);
      }
    }
    (void) start;
  }
  else delay(busy_time);
}

void GxEPD2_1248c::_getMasterTemperature()
{
  uint8_t value = 0;
  M1.writeCommand(0x40);
  _waitWhileAnyBusy("getMasterTemperature", 300);
  SPI.end();
  pinMode(_mosi, INPUT);
  delay(100);
  digitalWrite(_sck, HIGH);
  pinMode(_sck, OUTPUT);
  digitalWrite(_cs_m1, LOW);
  pinMode(_mosi, INPUT);
  for (uint16_t i = 0; i < 8; i++)
  {
    digitalWrite(_sck, LOW);
    value <<= 1;
    delayMicroseconds(2);
    if (digitalRead(_mosi)) value |= 0x01;
    delayMicroseconds(2);
    digitalWrite(_sck, HIGH);
    delayMicroseconds(2);
  }
  digitalWrite(_cs_m1, HIGH);
  pinMode(_sck, INPUT);
  _temperature = value;
  _initSPI();
  if (_diag_enabled)
  {
    Serial.print("Master Temperature is "); Serial.println(value);
  }
}

void GxEPD2_1248c::_readController(uint8_t cmd, uint8_t* data, uint16_t n, int8_t cs, int8_t dc)
{
  if (cs < 0) cs = _cs_m1;
  if (dc < 0) dc = _dc1;
  SPI.beginTransaction(_spi_settings);
  digitalWrite(cs, LOW);
  digitalWrite(dc, LOW);
  SPI.transfer(cmd);
  digitalWrite(dc, HIGH);
  digitalWrite(cs, HIGH);
  SPI.endTransaction();
  _waitWhileAnyBusy("_readController", 300);
  SPI.end();
  pinMode(_mosi, INPUT);
  delay(100);
  digitalWrite(_sck, HIGH);
  pinMode(_sck, OUTPUT);
  digitalWrite(cs, LOW);
  pinMode(_mosi, INPUT);
  for (uint16_t j = 0; j < n; j++)
  {
    uint8_t value = 0;
    for (uint16_t i = 0; i < 8; i++)
    {
      digitalWrite(_sck, LOW);
      value <<= 1;
      delayMicroseconds(2);
      if (digitalRead(_mosi)) value |= 0x01;
      delayMicroseconds(2);
      digitalWrite(_sck, HIGH);
      delayMicroseconds(2);
    }
    data[j] = value;
  }
  digitalWrite(cs, HIGH);
  pinMode(_sck, INPUT);
  _initSPI();
}

GxEPD2_1248c::ScreenPart::ScreenPart(uint16_t width, uint16_t height, bool rev_scan, int16_t cs, int16_t dc) :
  WIDTH(width), HEIGHT(height), _rev_scan(rev_scan),
  _cs(cs), _dc(dc), _spi_settings(4000000, MSBFIRST, SPI_MODE0)
{
}

void GxEPD2_1248c::ScreenPart::writeScreenBuffer(uint8_t command, uint8_t value)
{
  writeCommand(command); // set current or previous
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    writeData(value);
  }
}

void GxEPD2_1248c::ScreenPart::writeImagePart(uint8_t command, const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
    int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  if ((w_bitmap < 0) || (h_bitmap < 0) || (w < 0) || (h < 0)) return;
  if ((x_part < 0) || (x_part >= w_bitmap)) return;
  if ((y_part < 0) || (y_part >= h_bitmap)) return;
  int32_t wb_bitmap = (w_bitmap + 7) / 8; // width bytes, bitmaps are padded
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
  //Serial.print(x); Serial.print(", "); Serial.print(y); Serial.print(", "); Serial.print(w); Serial.print(", "); Serial.println(h);
  //Serial.print("->"); Serial.print(x1); Serial.print(", "); Serial.print(y1); Serial.print(", "); Serial.print(w1); Serial.print(", "); Serial.println(h1);
  writeCommand(0x91); // partial in
  delay(1);
  _setPartialRamArea(x1, y1, w1, h1);
  writeCommand(command);
  _startTransfer();
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data;
      // use wb_bitmap, h_bitmap of bitmap for index!
      int32_t idx = mirror_y ? x_part / 8 + j + dx / 8 + ((h_bitmap - 1 - (y_part + i + dy))) * wb_bitmap : x_part / 8 + j + dx / 8 + (y_part + i + dy) * wb_bitmap;
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
      _transfer(data);
    }
  }
  _endTransfer();
  writeCommand(0x92); // partial out
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_1248c::ScreenPart::writeCommand(uint8_t c)
{
  SPI.beginTransaction(_spi_settings);
  if (_dc >= 0) digitalWrite(_dc, LOW);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  SPI.transfer(c);
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  if (_dc >= 0) digitalWrite(_dc, HIGH);
  SPI.endTransaction();
}

void GxEPD2_1248c::ScreenPart::writeData(uint8_t d)
{
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  SPI.transfer(d);
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}

void GxEPD2_1248c::ScreenPart::_setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  if (_rev_scan) x = WIDTH - w - x;
  uint16_t xe = (x + w - 1) | 0x0007; // byte boundary inclusive (last byte)
  uint16_t ye = y + h - 1;
  x &= 0xFFF8; // byte boundary
  writeCommand(0x90); // partial window
  writeData(x / 256);
  writeData(x % 256);
  writeData(xe / 256);
  writeData(xe % 256);
  writeData(y / 256);
  writeData(y % 256);
  writeData(ye / 256);
  writeData(ye % 256);
  writeData(0x01);
}

void GxEPD2_1248c::ScreenPart::_startTransfer()
{
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
}

void GxEPD2_1248c::ScreenPart::_transfer(uint8_t value)
{
  SPI.transfer(value);
}

void GxEPD2_1248c::ScreenPart::_endTransfer()
{
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}
