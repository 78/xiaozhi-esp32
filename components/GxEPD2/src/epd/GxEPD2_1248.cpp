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

#include "GxEPD2_1248.h"

#if defined(ESP32)
// general constructor for use with all parameters on ESP32, e.g. for Waveshare ESP32 driver board mounted on connection board
GxEPD2_1248::GxEPD2_1248(int16_t sck, int16_t miso, int16_t mosi,
                         int16_t cs_m1, int16_t cs_s1, int16_t cs_m2, int16_t cs_s2,
                         int16_t dc1, int16_t dc2, int16_t rst1, int16_t rst2,
                         int16_t busy_m1, int16_t busy_s1, int16_t busy_m2, int16_t busy_s2) :
  GxEPD2_EPD(cs_m1, dc1, rst1, busy_m1, LOW, 10000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate),
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
GxEPD2_1248::GxEPD2_1248(int16_t cs_m1, int16_t cs_s1, int16_t cs_m2, int16_t cs_s2,
                         int16_t dc1, int16_t dc2, int16_t rst1, int16_t rst2,
                         int16_t busy_m1, int16_t busy_s1, int16_t busy_m2, int16_t busy_s2) :
  GxEPD2_EPD(cs_m1, dc1, rst1, busy_m1, LOW, 10000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate),
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
GxEPD2_1248::GxEPD2_1248(int16_t cs_m1, int16_t cs_s1, int16_t cs_m2, int16_t cs_s2, int16_t dc, int16_t rst, int16_t busy) :
  GxEPD2_EPD(23, 25, 33, 32, LOW, 10000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate),
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

void GxEPD2_1248::init(uint32_t serial_diag_bitrate)
{
  init(serial_diag_bitrate, true, 20, false);
}

void GxEPD2_1248::init(uint32_t serial_diag_bitrate, bool initial, uint16_t reset_duration, bool pulldown_rst_mode)
{
  _initial_write = initial;
  _initial_refresh = initial;
  _using_partial_mode = false;
  if (serial_diag_bitrate > 0)
  {
    Serial.begin(serial_diag_bitrate);
    _diag_enabled = true;
    Serial.println(); Serial.println("GxEPD2_1248::init()");
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

void GxEPD2_1248::clearScreen(uint8_t value)
{
  writeScreenBuffer(value);
  refresh(true);
  writeScreenBufferAgain(value);
}

void GxEPD2_1248::writeScreenBuffer(uint8_t value)
{
  if (!_using_partial_mode) _Init_Part();
  if (_initial_write)
  {
    M1.writeScreenBuffer(0x10, value);
    S1.writeScreenBuffer(0x10, value);
    M2.writeScreenBuffer(0x10, value);
    S2.writeScreenBuffer(0x10, value);
  }
  M1.writeScreenBuffer(0x13, value);
  S1.writeScreenBuffer(0x13, value);
  M2.writeScreenBuffer(0x13, value);
  S2.writeScreenBuffer(0x13, value);
  _initial_write = false; // initial full screen buffer clean done
}

void GxEPD2_1248::writeScreenBufferAgain(uint8_t value)
{
  if (!_using_partial_mode) _Init_Part();
  M1.writeScreenBuffer(0x10, value);
  S1.writeScreenBuffer(0x10, value);
  M2.writeScreenBuffer(0x10, value);
  S2.writeScreenBuffer(0x10, value);
}

void GxEPD2_1248::writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  if (!_using_partial_mode) _Init_Part();
  S2.writeImagePart(0x13, bitmap, 0, 0, w, h, x, y, w, h, invert, mirror_y, pgm);
  M2.writeImagePart(0x13, bitmap, 0, 0, w, h, x - S2.WIDTH, y, w, h, invert, mirror_y, pgm);
  M1.writeImagePart(0x13, bitmap, 0, 0, w, h, x, y - S2.HEIGHT, w, h, invert, mirror_y, pgm);
  S1.writeImagePart(0x13, bitmap, 0, 0, w, h, x - M1.WIDTH, y - M2.HEIGHT, w, h, invert, mirror_y, pgm);
}

void GxEPD2_1248::writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                 int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  if (!_using_partial_mode) _Init_Part();
  S2.writeImagePart(0x13, bitmap, x_part, y_part, w, h, x, y, w, h, invert, mirror_y, pgm);
  M2.writeImagePart(0x13, bitmap, x_part, y_part, w, h, x - S2.WIDTH, y, w, h, invert, mirror_y, pgm);
  M1.writeImagePart(0x13, bitmap, x_part, y_part, w, h, x, y - S2.HEIGHT, w, h, invert, mirror_y, pgm);
  S1.writeImagePart(0x13, bitmap, x_part, y_part, w, h, x - M1.WIDTH, y - M2.HEIGHT, w, h, invert, mirror_y, pgm);
}

void GxEPD2_1248::writeImageAgain(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  if (!_using_partial_mode) _Init_Part();
  S2.writeImagePart(0x10, bitmap, 0, 0, w, h, x, y, w, h, invert, mirror_y, pgm);
  M2.writeImagePart(0x10, bitmap, 0, 0, w, h, x - S2.WIDTH, y, w, h, invert, mirror_y, pgm);
  M1.writeImagePart(0x10, bitmap, 0, 0, w, h, x, y - S2.HEIGHT, w, h, invert, mirror_y, pgm);
  S1.writeImagePart(0x10, bitmap, 0, 0, w, h, x - M1.WIDTH, y - M2.HEIGHT, w, h, invert, mirror_y, pgm);
}

void GxEPD2_1248::writeImagePartAgain(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                      int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  if (!_using_partial_mode) _Init_Part();
  S2.writeImagePart(0x10, bitmap, x_part, y_part, w, h, x, y, w, h, invert, mirror_y, pgm);
  M2.writeImagePart(0x10, bitmap, x_part, y_part, w, h, x - S2.WIDTH, y, w, h, invert, mirror_y, pgm);
  M1.writeImagePart(0x10, bitmap, x_part, y_part, w, h, x, y - S2.HEIGHT, w, h, invert, mirror_y, pgm);
  S1.writeImagePart(0x10, bitmap, x_part, y_part, w, h, x - M1.WIDTH, y - M2.HEIGHT, w, h, invert, mirror_y, pgm);
}

void GxEPD2_1248::writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    writeImage(black, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_1248::writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                 int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    writeImagePart(black, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_1248::writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    writeImage(data1, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_1248::drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
  writeImageAgain(bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_1248::drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
  writeImagePartAgain(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_1248::drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    drawImage(black, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_1248::drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                                int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    drawImagePart(black, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_1248::drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    drawImage(data1, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_1248::refresh(bool partial_update_mode)
{
  if (partial_update_mode) refresh(0, 0, WIDTH, HEIGHT);
  else
  {
    if (_using_partial_mode) _Init_Full();
    _Update_Full();
    _initial_refresh = false; // initial full update done
  }
}

void GxEPD2_1248::refresh(int16_t x, int16_t y, int16_t w, int16_t h)
{
  if (_initial_refresh) return refresh(false); // initial update needs be full update
  if (!_using_partial_mode) _Init_Part();
  _Update_Part();
}

void GxEPD2_1248::powerOff(void)
{
  _PowerOff();
}

void GxEPD2_1248::hibernate()
{
  _PowerOff();
  if (_rst >= 0)
  {
    _writeCommandAll(0x07); // deep sleep
    _writeDataAll(0xA5);    // check code
    _hibernating = true;
  }
}

void GxEPD2_1248::_reset(void)
{
  digitalWrite(_rst1, LOW);
  digitalWrite(_rst2, LOW);
  delay(200);
  digitalWrite(_rst1, HIGH);
  digitalWrite(_rst2, HIGH);
  delay(200);
  _hibernating = false;
}

void GxEPD2_1248::_initSPI()
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

void GxEPD2_1248::_setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  uint16_t xe = (x + w - 1) | 0x0007; // byte boundary inclusive (last byte)
  uint16_t ye = y + h - 1;
  x &= 0xFFF8; // byte boundary
  _writeCommand(0x90); // partial window
  _writeData(x / 256);
  _writeData(x % 256);
  _writeData(xe / 256);
  _writeData(xe % 256);
  _writeData(y / 256);
  _writeData(y % 256);
  _writeData(ye / 256);
  _writeData(ye % 256);
  _writeData(0x01);
}

void GxEPD2_1248::_PowerOn()
{
  if (!_power_is_on)
  {
    _writeCommandMaster(0x04);
    _waitWhileAnyBusy("_PowerOn", power_on_time);
  }
  _power_is_on = true;
}

void GxEPD2_1248::_PowerOff()
{
  if (_power_is_on)
  {
    _writeCommandMaster(0x02); // power off
    _waitWhileAnyBusy("_PowerOff", power_on_time);
  }
  _power_is_on = false;
  _using_partial_mode = false;
}

void GxEPD2_1248::_InitDisplay()
{
  if (_hibernating) _reset();
  //panel setting
  M1.writeCommand(0x00);
  M1.writeData(0x1f);  //KW-3f   KWR-2F  BWROTP 0f BWOTP 1f
  S1.writeCommand(0x00);
  S1.writeData(0x1f);
  M2.writeCommand(0x00);
  M2.writeData(0x13); // reverse scan
  S2.writeCommand(0x00);
  S2.writeData(0x13); // reverse scan
  // booster soft start
  M1.writeCommand(0x06);
  M1.writeData(0x17);  //A
  M1.writeData(0x17);  //B
  M1.writeData(0x39);  //C
  M1.writeData(0x17);
  M2.writeCommand(0x06);
  M2.writeData(0x17);
  M2.writeData(0x17);
  M2.writeData(0x39);
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
  _writeDataAll(0x21);  //Border KW
  //_writeDataAll(0x29); // LUTKW, N2OCP: copy new to old DOES NOT WORK
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

// experimental partial screen update LUTs with balanced charge
// LUTs are filled with zeroes

//#define T1 35 // charge balance pre-phase
//#define T2  0 // optional extension
//#define T3 35 // color change phase (b/w)
//#define T4  0 // optional extension for one color

//#define T1 20 // charge balance pre-phase
//#define T2 10 // optional extension
//#define T3 20 // color change phase (b/w)
//#define T4 10 // optional extension for one color

#define T1 25 // charge balance pre-phase
#define T2 10 // optional extension
#define T3 25 // color change phase (b/w)
#define T4 10 // optional extension for one color

const unsigned char GxEPD2_1248::lut_20_LUTC_partial[] PROGMEM =
{
  0x00, T1, T2, T3, T4, 1, // 00 00 00 00
};

const unsigned char GxEPD2_1248::lut_21_LUTWW_partial[] PROGMEM =
{ // 10 w
  0x00, T1, T2, T3, T4, 1, // 00 00 00 00
};

const unsigned char GxEPD2_1248::lut_22_LUTKW_partial[] PROGMEM =
{ // 10 w
  //0x48, T1, T2, T3, T4, 1, // 01 00 10 00
  0x5A, T1, T2, T3, T4, 1, // 01 01 10 10 more white
};

const unsigned char GxEPD2_1248::lut_23_LUTWK_partial[] PROGMEM =
{ // 01 b
  0x84, T1, T2, T3, T4, 1, // 10 00 01 00
  //0xA5, T1, T2, T3, T4, 1, // 10 10 01 01 more black
};

const unsigned char GxEPD2_1248::lut_24_LUTKK_partial[] PROGMEM =
{ // 01 b
  0x00, T1, T2, T3, T4, 1, // 00 00 00 00
};

const unsigned char GxEPD2_1248::lut_25_LUTBD_partial[] PROGMEM =
{
  0x00, T1, T2, T3, T4, 1, // 00 00 00 00
};

void GxEPD2_1248::_Init_Full()
{
  _InitDisplay();
  // LUT from OTP
  M1.writeCommand(0x00);
  M1.writeData(0x1f);  //KW-3f   KWR-2F  BWROTP 0f BWOTP 1f
  S1.writeCommand(0x00);
  S1.writeData(0x1f);
  M2.writeCommand(0x00);
  M2.writeData(0x13); // reverse scan
  S2.writeCommand(0x00);
  S2.writeData(0x13); // reverse scan
  _PowerOn();
  _using_partial_mode = false;
}

void GxEPD2_1248::_Init_Part()
{
  _InitDisplay();
  // LUT from registers
  M1.writeCommand(0x00);
  M1.writeData(0x3f);  //KW-3f   KWR-2F  BWROTP 0f BWOTP 1f
  S1.writeCommand(0x00);
  S1.writeData(0x3f);
  M2.writeCommand(0x00);
  M2.writeData(0x33);
  S2.writeCommand(0x00);
  S2.writeData(0x33);
  _writeCommandAll(0x82); // vcom_DC setting
  _writeDataAll (0x1C);
  _writeCommandAll(0x50); // VCOM AND DATA INTERVAL SETTING
  _writeDataAll(0x31);  //Border KW
  _writeDataAll(0x07);
  _writeCommandAll(0x20);
  _writeDataPGM_All(lut_20_LUTC_partial, sizeof(lut_20_LUTC_partial), 60 - sizeof(lut_20_LUTC_partial));
  _writeCommandAll(0x21);
  _writeDataPGM_All(lut_21_LUTWW_partial, sizeof(lut_21_LUTWW_partial), 60 - sizeof(lut_21_LUTWW_partial));
  _writeCommandAll(0x22);
  _writeDataPGM_All(lut_22_LUTKW_partial, sizeof(lut_22_LUTKW_partial), 60 - sizeof(lut_22_LUTKW_partial));
  _writeCommandAll(0x23);
  _writeDataPGM_All(lut_23_LUTWK_partial, sizeof(lut_23_LUTWK_partial), 60 - sizeof(lut_23_LUTWK_partial));
  _writeCommandAll(0x24);
  _writeDataPGM_All(lut_24_LUTKK_partial, sizeof(lut_24_LUTKK_partial), 60 - sizeof(lut_24_LUTKK_partial));
  _writeCommandAll(0x25);
  _writeDataPGM_All(lut_25_LUTBD_partial, sizeof(lut_25_LUTBD_partial), 60 - sizeof(lut_25_LUTBD_partial));
  _PowerOn();
  _using_partial_mode = true;
}

void GxEPD2_1248::_Update_Full()
{
  _writeCommandAll(0x12); //display refresh
  _waitWhileAnyBusy("_Update_Full", full_refresh_time);
}

void GxEPD2_1248::_Update_Part()
{
  _writeCommandAll(0x12); //display refresh
  _waitWhileAnyBusy("_Update_Part", partial_refresh_time);
}

void GxEPD2_1248::_writeCommandMaster(uint8_t c)
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

void GxEPD2_1248::_writeDataMaster(uint8_t d)
{
  SPI.beginTransaction(_spi_settings);
  digitalWrite(_cs_m1, LOW);
  digitalWrite(_cs_m2, LOW);
  SPI.transfer(d);
  digitalWrite(_cs_m1, HIGH);
  digitalWrite(_cs_m2, HIGH);
  SPI.endTransaction();
}

void GxEPD2_1248::_writeCommandAll(uint8_t c)
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

void GxEPD2_1248::_writeDataAll(uint8_t d)
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

void GxEPD2_1248::_writeDataPGM_All(const uint8_t* data, uint16_t n, int16_t fill_with_zeroes)
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

void GxEPD2_1248::_waitWhileAnyBusy(const char* comment, uint16_t busy_time)
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

void GxEPD2_1248::_getMasterTemperature()
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

void GxEPD2_1248::_readController(uint8_t cmd, uint8_t* data, uint16_t n, int8_t cs, int8_t dc)
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

GxEPD2_1248::ScreenPart::ScreenPart(uint16_t width, uint16_t height, bool rev_scan, int16_t cs, int16_t dc) :
  WIDTH(width), HEIGHT(height), _rev_scan(rev_scan),
  _cs(cs), _dc(dc), _spi_settings(4000000, MSBFIRST, SPI_MODE0)
{
}

void GxEPD2_1248::ScreenPart::writeScreenBuffer(uint8_t command, uint8_t value)
{
  writeCommand(command); // set current or previous
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    writeData(value);
  }
}

void GxEPD2_1248::ScreenPart::writeImagePart(uint8_t command, const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
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

void GxEPD2_1248::ScreenPart::writeCommand(uint8_t c)
{
  SPI.beginTransaction(_spi_settings);
  if (_dc >= 0) digitalWrite(_dc, LOW);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  SPI.transfer(c);
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  if (_dc >= 0) digitalWrite(_dc, HIGH);
  SPI.endTransaction();
}

void GxEPD2_1248::ScreenPart::writeData(uint8_t d)
{
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
  SPI.transfer(d);
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}

void GxEPD2_1248::ScreenPart::_setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
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

void GxEPD2_1248::ScreenPart::_startTransfer()
{
  SPI.beginTransaction(_spi_settings);
  if (_cs >= 0) digitalWrite(_cs, LOW);
}

void GxEPD2_1248::ScreenPart::_transfer(uint8_t value)
{
  SPI.transfer(value);
}

void GxEPD2_1248::ScreenPart::_endTransfer()
{
  if (_cs >= 0) digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}
