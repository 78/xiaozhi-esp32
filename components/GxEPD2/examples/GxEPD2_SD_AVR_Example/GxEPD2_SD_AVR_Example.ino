// GxEPD2_SD_AVR_Example : Display Library example for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Display Library based on Demo Example from Good Display: https://www.good-display.com/companyfile/32/
//
// BMP handling code extracts taken from: https://github.com/prenticedavid/MCUFRIEND_kbv/tree/master/examples/showBMP_kbv_Uno
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2
//
// note that BMP bitmaps are drawn at physical position in physical orientation of the screen

// Supporting Arduino Forum Topics (closed, read only):
// Good Display ePaper for Arduino: https://forum.arduino.cc/t/good-display-epaper-for-arduino/419657
// Waveshare e-paper displays with SPI: https://forum.arduino.cc/t/waveshare-e-paper-displays-with-spi/467865
//
// Add new topics in https://forum.arduino.cc/c/using-arduino/displays/23 for new questions and issues

// see GxEPD2_wiring_examples.h for wiring suggestions and examples

// **** NOTE that the mapping suggestion may need modification depending on SD board used! ****
// ********************************************************************************************
//
//#define SD_CS SS  // e.g. for RobotDyn Wemos D1 mini SD board
//#define EPD_CS D1 // alternative I use with RobotDyn Wemos D1 mini SD board

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
//#include <GFX.h>
// Note: if you use this with ENABLE_GxEPD2_GFX 1:
//       uncomment it in GxEPD2_GFX.h too, or add #include <GFX.h> before any #include <GxEPD2_GFX.h>

// includes needed to include driver classes
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
//#include <Fonts/FreeMonoBold9pt7b.h>

#if defined(ESP32)

// has support for FAT32 support with long filenames
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#define SdFile File
#define seekSet seek

#else

// include SdFat for FAT32 support with long filenames; available through Library Manager
#include <SdFat.h>
SdFat SD;

#endif

// select the display driver class (only one) for your  panel
//#define GxEPD2_DRIVER_CLASS GxEPD2_102     // GDEW0102T4   80x128, UC8175, (WFT0102CZA2)
//#define GxEPD2_DRIVER_CLASS GxEPD2_150_BN  // DEPG0150BN 200x200, SSD1681, (FPC8101), TTGO T5 V2.4.1 .
//#define GxEPD2_DRIVER_CLASS GxEPD2_154     // GDEP015OC1  200x200, IL3829, (WFC0000CZ07), no longer available.
//#define GxEPD2_DRIVER_CLASS GxEPD2_154_D67 // GDEH0154D67 200x200, SSD1681, (HINK-E154A07-A1).
//#define GxEPD2_DRIVER_CLASS GxEPD2_154_T8  // GDEW0154T8  152x152, UC8151 (IL0373), (WFT0154CZ17).
//#define GxEPD2_DRIVER_CLASS GxEPD2_154_M09 // GDEW0154M09 200x200, JD79653A, (WFT0154CZB3).
//#define GxEPD2_DRIVER_CLASS GxEPD2_154_M10 // GDEW0154M10 152x152, UC8151D, (WFT0154CZ17).
//#define GxEPD2_DRIVER_CLASS GxEPD2_154_GDEY0154D67 // GDEY0154D67 200x200, SSD1681, (FPC-B001 20.05.21).
//#define GxEPD2_DRIVER_CLASS GxEPD2_213     // GDE0213B1   122x250, IL3895, (HINK-E0213-G01), phased out
//#define GxEPD2_DRIVER_CLASS GxEPD2_213_B72 // GDEH0213B72 122x250, SSD1675A (IL3897), (HINK-E0213A22-A0 SLH1852).
//#define GxEPD2_DRIVER_CLASS GxEPD2_213_B73 // GDEH0213B73 122x250, SSD1675B, (HINK-E0213A22-A0 SLH1914).
//#define GxEPD2_DRIVER_CLASS GxEPD2_213_B74 // GDEM0213B74 122x250, SSD1680, FPC-7528B).
//#define GxEPD2_DRIVER_CLASS GxEPD2_213_flex // GDEW0213I5F 104x212, UC8151 (IL0373), (WFT0213CZ16).
//#define GxEPD2_DRIVER_CLASS GxEPD2_213_M21 // GDEW0213M21 104x212, UC8151 (IL0373), (WFT0213CZ16).
//#define GxEPD2_DRIVER_CLASS GxEPD2_213_T5D // GDEW0213T5D 104x212, UC8151D, (WFT0213CZ16).
//#define GxEPD2_DRIVER_CLASS GxEPD2_213_BN // DEPG0213BN  122x250, SSD1680, (FPC-7528B), TTGO T5 V2.4.1, V2.3.1 .
//#define GxEPD2_DRIVER_CLASS GxEPD2_213_GDEY0213B74 // GDEY0213B74 122x250, SSD1680, (FPC-A002 20.04.08).
//#define GxEPD2_DRIVER_CLASS GxEPD2_260     // GDEW026T0   152x296, UC8151 (IL0373), (WFT0154CZ17).
//#define GxEPD2_DRIVER_CLASS GxEPD2_260_M01 // GDEW026M01  152x296, UC8151 (IL0373), (WFT0260CZB2).
//#define GxEPD2_DRIVER_CLASS GxEPD2_266_BN // DEPG0266BN   152x296, SSD1680, (FPC7510), TTGO T5 V2.66, TTGO T5 V2.4.1 .
//#define GxEPD2_DRIVER_CLASS GxEPD2_266_GDEY0266T90 // GDEY0266T90 152x296, SSD1680, (FPC-A003 HB).
//#define GxEPD2_DRIVER_CLASS GxEPD2_270     // GDEW027W3   176x264, EK79652 (IL91874), (WFI0190CZ22) ?
//#define GxEPD2_DRIVER_CLASS GxEPD2_270_GDEY027T91 // GDEY027T91 176x264, SSD1680, (FB).
//#define GxEPD2_DRIVER_CLASS GxEPD2_290     // GDEH029A1   128x296, SSD1608 (IL3820), (E029A01-FPC-A1 SYX1553).
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_T5  // GDEW029T5   128x296, UC8151 (IL0373), (WFT0290CZ10).
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_T5D // GDEW029T5D  128x296, UC8151D, (WFT0290CZ10).
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_I6FD // GDEW029I6FD  128x296, UC8151D, (WFT0290CZ10).
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_T94 // GDEM029T94  128x296, SSD1680, (FPC-7519 rev.b).
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_T94_V2 // GDEM029T94  128x296, SSD1680, (FPC-7519 rev.b), Waveshare 2.9" V2 variant .
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_BS // DEPG0290BS  128x296, SSD1680, (FPC-7519 rev.b).
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_M06 // GDEW029M06  128x296, UC8151D, (WFT0290CZ10).
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_GDEY029T94 // GDEY029T94 128x296, SSD1680, (FPC-A005 20.06.15).
//#define GxEPD2_DRIVER_CLASS GxEPD2_310_GDEQ031T10 // GDEQ031T10 240x320, UC8253, (no inking, backside mark KEGMO 3100)
//#define GxEPD2_DRIVER_CLASS GxEPD2_371     // GDEW0371W7  240x416, UC8171 (IL0324), (missing)
//#define GxEPD2_DRIVER_CLASS GxEPD2_370_TC1 // ED037TC1  280x480, SSD1677, (ICA-FU-20 ichia 2029), Waveshare 3.7".
//#define GxEPD2_DRIVER_CLASS GxEPD2_420     // GDEW042T2   400x300, UC8176 (IL0398), (WFT042CZ15).
//#define GxEPD2_DRIVER_CLASS GxEPD2_420_M01 // GDEW042M01  400x300, UC8176 (IL0398), (WFT042CZ15).
//#define GxEPD2_DRIVER_CLASS GxEPD2_420_GDEY042T81 // GDEY042T81 400x300, SSD1683 (no inking).
//#define GxEPD2_DRIVER_CLASS GxEPD2_426_GDEQ0426T82 // GDEQ0426T82 480x800, SSD1677 (P426010-MF1-A).
//#define GxEPD2_DRIVER_CLASS GxEPD2_583     // GDEW0583T7  600x448, UC8159c (IL0371), (missing)
//#define GxEPD2_DRIVER_CLASS GxEPD2_583_T8  // GDEW0583T8  648x480, EK79655 (GD7965), (WFT0583CZ61).
//#define GxEPD2_DRIVER_CLASS GxEPD2_583_GDEQ0583T31 // GDEQ0583T31  648x480, UC8179, (P583010-MF1-B).
//#define GxEPD2_DRIVER_CLASS GxEPD2_750     // GDEW075T8   640x384, UC8159c (IL0371), (WF0583CZ09)
//#define GxEPD2_DRIVER_CLASS GxEPD2_750_T7  // GDEW075T7   800x480, EK79655 (GD7965), (WFT0583CZ61)
//#define GxEPD2_DRIVER_CLASS GxEPD2_750_GDEY075T7  // GDEY075T7  800x480, UC8179 (GD7965), (FPC-C001 20.08.20).
//#define GxEPD2_DRIVER_CLASS GxEPD2_1160_T91 // GDEH116T91 960x640, SSD1677, (none or hidden).
//#define GxEPD2_DRIVER_CLASS GxEPD2_1248     // GDEW1248T3 1304x984, UC8179, (WFT1248BZ23,WFT1248BZ24)
// 3-color e-papers
//#define GxEPD2_DRIVER_CLASS GxEPD2_154c     // GDEW0154Z04 200x200, IL0376F, (WFT0000CZ04), no longer available
//#define GxEPD2_DRIVER_CLASS GxEPD2_154_Z90c // GDEH0154Z90 200x200, SSD1681, (HINK-E154A07-A1)
//#define GxEPD2_DRIVER_CLASS GxEPD2_213c     // GDEW0213Z16 104x212, UC8151 (IL0373), (WFT0213CZ16)
//#define GxEPD2_DRIVER_CLASS GxEPD2_213_Z19c // GDEH0213Z19 104x212, UC8151D, (HINK-E0213A20-A2 2020-11-19)
//#define GxEPD2_DRIVER_CLASS GxEPD2_213_Z98c // GDEY0213Z98 122x250, SSD1680, (FPC-A002 20.04.08)
//#define GxEPD2_DRIVER_CLASS GxEPD2_266c     // GDEY0266Z90 152x296, SSD1680, (FPC-7510)
//#define GxEPD2_DRIVER_CLASS GxEPD2_270c     // GDEW027C44  176x264, IL91874, (WFI0190CZ22)
//#define GxEPD2_DRIVER_CLASS GxEPD2_290c     // GDEW029Z10  128x296, UC8151 (IL0373), (WFT0290CZ10)
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_Z13c // GDEH029Z13  128x296, UC8151D, (HINK-E029A10-A3 20160809)
//#define GxEPD2_DRIVER_CLASS GxEPD2_290_C90c // GDEM029C90  128x296, SSD1680, (FPC-7519 rev.b)
//#define GxEPD2_DRIVER_CLASS GxEPD2_420c     // GDEW042Z15  400x300, UC8176 (IL0398), (WFT0420CZ15)
//#define GxEPD2_DRIVER_CLASS GxEPD2_420c_Z21 // GDEQ042Z21  400x300, UC8276, (hidden)
//#define GxEPD2_DRIVER_CLASS GxEPD2_583c     // GDEW0583Z21 600x448, UC8159c (IL0371), (missing)
//#define GxEPD2_DRIVER_CLASS GxEPD2_583c_Z83 // GDEW0583Z83 648x480, EK79655 (GD7965), (WFT0583CZ61)
//#define GxEPD2_DRIVER_CLASS GxEPD2_750c     // GDEW075Z09  640x384, UC8159c (IL0371), (WF0583CZ09)
//#define GxEPD2_DRIVER_CLASS GxEPD2_750c_Z08 // GDEW075Z08  800x480, EK79655 (GD7965), (WFT0583CZ61)
//#define GxEPD2_DRIVER_CLASS GxEPD2_750c_Z90 // GDEH075Z90  880x528, SSD1677, (HINK-E075A07-A0)
//#define GxEPD2_DRIVER_CLASS GxEPD2_1248c    // GDEY1248Z51 1304x984, UC8179, (WFT1248BZ23,WFT1248BZ24)
// 4-color e-paper
//#define GxEPD2_DRIVER_CLASS GxEPD2_266c_GDEY0266F51H // GDEY0266F51H 184x360, JD79667 (FPC-H006 22.04.02)
//#define GxEPD2_DRIVER_CLASS GxEPD2_290c_GDEY029F51H // GDEY029F51H 168x384, JD79667 (FPC-H004 22.03.24)
//#define GxEPD2_DRIVER_CLASS GxEPD2_300c     // Waveshare 3.00" 4-color
//#define GxEPD2_DRIVER_CLASS GxEPD2_420c_GDEY0420F51 // GDEY0420F51 400x300, HX8717 (no inking)
//#define GxEPD2_DRIVER_CLASS GxEPD2_437c     // Waveshare 4.37" 4-color
// 7-color e-paper
//#define GxEPD2_DRIVER_CLASS GxEPD2_565c // Waveshare 5.65" 7-color
//#define GxEPD2_DRIVER_CLASS GxEPD2_730c_GDEY073D46 // GDEY073D46 800x480 7-color, (N-FPC-001 2021.11.26)
//#define GxEPD2_DRIVER_CLASS GxEPD2_730c_ACeP_730 // Waveshare 7.3" 7-color

#if defined(__AVR)
#define SD_CS 6  // adapt to your wiring
#define EPD_CS SS // adapt to your wiring
GxEPD2_DRIVER_CLASS display(/*CS=10*/ EPD_CS, /*DC=*/ 8, /*RST=*/ 9, /*BUSY=*/ 7);
#endif

// non-AVR board can also be used with GxEPD2 base display classes, e.g. for SD bitmap drawing
// uncomment your display for your board in the following included header file and adapt to your mapping

#include "GxEPD2_SD_AVR_boards_added.h"

// note that BMP bitmaps are drawn at physical position in physical orientation of the screen

// function declaration with default parameter
void drawBitmapFromSD(const char *filename, int16_t x, int16_t y, bool with_color = true);

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("GxEPD2_SD_AVR_Example"));

  display.init(115200);

  Serial.print(F("Initializing SD card..."));
  if (!SD.begin(SD_CS))
  {
    Serial.println(F("SD failed!"));
    return;
  }
  Serial.println(F("SD OK!"));

  drawBitmaps_200x200();
  drawBitmaps_other();

  //drawBitmaps_test();

  display.hibernate();

  Serial.println(F("GxEPD2_SD_AVR_Example done"));
}

void loop(void)
{
}

void drawBitmaps_200x200()
{
  int16_t x = (int16_t(display.WIDTH) - 200) / 2;
  int16_t y = (int16_t(display.HEIGHT) - 200) / 2;
  drawBitmapFromSD("logo200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD("first200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD("second200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD("third200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD("fourth200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD("fifth200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD("sixth200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD("seventh200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD("eighth200x200.bmp", x, y);
  delay(2000);
}

void drawBitmaps_other()
{
  int16_t w2 = display.WIDTH / 2;
  int16_t h2 = display.HEIGHT / 2;
  drawBitmapFromSD("parrot.bmp", w2 - 64, h2 - 80);
  delay(2000);
  drawBitmapFromSD("betty_1.bmp", w2 - 100, h2 - 160);
  delay(2000);
  drawBitmapFromSD("betty_4.bmp", w2 - 102, h2 - 126);
  delay(2000);
  drawBitmapFromSD("marilyn_240x240x8.bmp", w2 - 120, h2 - 120);
  delay(2000);
  drawBitmapFromSD("miniwoof.bmp", w2 - 60, h2 - 80);
  delay(2000);
  drawBitmapFromSD("t200x200.bmp", w2 - 100, h2 - 100);
  delay(2000);
  drawBitmapFromSD("test.bmp", w2 - 120, h2 - 160);
  delay(2000);
  drawBitmapFromSD("tiger.bmp", w2 - 160, h2 - 120);
  delay(2000);
  drawBitmapFromSD("tiger_178x160x4.bmp", w2 - 89, h2 - 80);
  delay(2000);
  drawBitmapFromSD("tiger_240x317x4.bmp", w2 - 120, h2 - 160);
  delay(2000);
  drawBitmapFromSD("tiger_320x200x24.bmp", w2 - 160, h2 - 100);
  delay(2000);
  drawBitmapFromSD("tiger16T.bmp", w2 - 160, h2 - 120);
  delay(2000);
  drawBitmapFromSD("woof.bmp", w2 - 120, h2 - 160);
  delay(2000);
  drawBitmapFromSD("bitmap640x384_1.bmp", 0, 0);
  delay(2000);
}

void drawBitmaps_test()
{
  int16_t w2 = display.WIDTH / 2;
  int16_t h2 = display.HEIGHT / 2;
  drawBitmapFromSD("betty_4.bmp", w2 - 102, h2 - 126);
  delay(2000);
}

static const uint16_t input_buffer_pixels = 20; // may affect performance

static const uint16_t max_row_width = 640; // for up to 7.5" display
static const uint16_t max_palette_pixels = 256; // for depth <= 8

uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
uint8_t output_row_mono_buffer[max_row_width / 8]; // buffer for at least one row of b/w bits
uint8_t output_row_color_buffer[max_row_width / 8]; // buffer for at least one row of color bits
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w

void drawBitmapFromSD(const char *filename, int16_t x, int16_t y, bool with_color)
{
  SdFile file;
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  uint32_t startTime = millis();
  //Serial.println(); Serial.print(x); Serial.print(" "); Serial.print(y); Serial.print(" "); Serial.println(filename);
  if ((x >= int16_t(display.WIDTH)) || (y >= int16_t(display.HEIGHT))) return;
  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');
#if defined(ESP32)
  file = SD.open(String("/") + filename, FILE_READ);
  if (!file)
  {
    Serial.print(F("File not found"));
    return;
  }
#else
  if (!file.open(filename, FILE_READ))
  {
    Serial.print(F("File not found"));
    return;
  }
#endif
  // Parse BMP header
  if (read16(file) == 0x4D42) // BMP signature
  {
    uint32_t fileSize = read32(file);
    uint32_t creatorBytes = read32(file); (void)creatorBytes; //unused
    uint32_t imageOffset = read32(file); // Start of image data
    uint32_t headerSize = read32(file);
    uint32_t width  = read32(file);
    int32_t height = (int32_t) read32(file);
    uint16_t planes = read16(file);
    uint16_t depth = read16(file); // bits per pixel
    uint32_t format = read32(file);
    if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
    {
      Serial.print(F("File size: ")); Serial.println(fileSize);
      Serial.print(F("Image Offset: ")); Serial.println(imageOffset);
      Serial.print(F("Header size: ")); Serial.println(headerSize);
      Serial.print(F("Bit Depth: ")); Serial.println(depth);
      Serial.print(F("Image size: "));
      Serial.print(width);
      Serial.print('x');
      Serial.println(height);
      // BMP rows are padded (if needed) to 4-byte boundary
      uint32_t rowSize = (width * depth / 8 + 3) & ~3;
      if (depth < 8) rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;
      if (height < 0)
      {
        height = -height;
        flip = false;
      }
      uint16_t w = width;
      uint16_t h = height;
      if ((x + w - 1) >= int16_t(display.WIDTH))  w = int16_t(display.WIDTH)  - x;
      if ((y + h - 1) >= int16_t(display.HEIGHT)) h = int16_t(display.HEIGHT) - y;
      if (w <= max_row_width) // handle with direct drawing
      {
        valid = true;
        uint8_t bitmask = 0xFF;
        uint8_t bitshift = 8 - depth;
        uint16_t red, green, blue;
        bool whitish = false;
        bool colored = false;
        if (depth == 1) with_color = false;
        if (depth <= 8)
        {
          if (depth < 8) bitmask >>= depth;
          //file.seekSet(54); //palette is always @ 54
          file.seekSet(imageOffset - (4 << depth)); // 54 for regular, diff for colorsimportant
          for (int16_t pn = 0; pn < (1 << depth); pn++)
          {
            blue  = file.read();
            green = file.read();
            red   = file.read();
            file.read();
            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
            if (0 == pn % 8) mono_palette_buffer[pn / 8] = 0;
            mono_palette_buffer[pn / 8] |= whitish << pn % 8;
            if (0 == pn % 8) color_palette_buffer[pn / 8] = 0;
            color_palette_buffer[pn / 8] |= colored << pn % 8;
          }
        }
        //display.clearScreen();
        display.writeScreenBuffer();
        uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
        for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
        {
          uint32_t in_remain = rowSize;
          uint32_t in_idx = 0;
          uint32_t in_bytes = 0;
          uint8_t in_byte = 0; // for depth <= 8
          uint8_t in_bits = 0; // for depth <= 8
          uint8_t out_byte = 0xFF; // white (for w%8!=0 border)
          uint8_t out_color_byte = 0xFF; // white (for w%8!=0 border)
          uint32_t out_idx = 0;
          file.seekSet(rowPosition);
          for (uint16_t col = 0; col < w; col++) // for each pixel
          {
            // Time to read more pixel data?
            if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
            {
              in_bytes = file.read(input_buffer, in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain);
              in_remain -= in_bytes;
              in_idx = 0;
            }
            switch (depth)
            {
              case 32:
                blue = input_buffer[in_idx++];
                green = input_buffer[in_idx++];
                red = input_buffer[in_idx++];
                in_idx++; // skip alpha
                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                break;
              case 24:
                blue = input_buffer[in_idx++];
                green = input_buffer[in_idx++];
                red = input_buffer[in_idx++];
                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                break;
              case 16:
                {
                  uint8_t lsb = input_buffer[in_idx++];
                  uint8_t msb = input_buffer[in_idx++];
                  if (format == 0) // 555
                  {
                    blue  = (lsb & 0x1F) << 3;
                    green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                    red   = (msb & 0x7C) << 1;
                  }
                  else // 565
                  {
                    blue  = (lsb & 0x1F) << 3;
                    green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                    red   = (msb & 0xF8);
                  }
                  whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                  colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                }
                break;
              case 1:
              case 2:
              case 4:
              case 8:
                {
                  if (0 == in_bits)
                  {
                    in_byte = input_buffer[in_idx++];
                    in_bits = 8;
                  }
                  uint16_t pn = (in_byte >> bitshift) & bitmask;
                  whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                  colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
                  in_byte <<= depth;
                  in_bits -= depth;
                }
                break;
            }
            if (whitish)
            {
              // keep white
            }
            else if (colored && with_color)
            {
              out_color_byte &= ~(0x80 >> col % 8); // colored
            }
            else
            {
              out_byte &= ~(0x80 >> col % 8); // black
            }
            if ((7 == col % 8) || (col == w - 1)) // write that last byte! (for w%8!=0 border)
            {
              output_row_color_buffer[out_idx] = out_color_byte;
              output_row_mono_buffer[out_idx++] = out_byte;
              out_byte = 0xFF; // white (for w%8!=0 border)
              out_color_byte = 0xFF; // white (for w%8!=0 border)
            }
          } // end pixel
          uint16_t yrow = y + (flip ? h - row - 1 : row);
          display.writeImage(output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);
        } // end line
        Serial.print(F("loaded in ")); Serial.print(millis() - startTime); Serial.println(F(" ms"));
        display.refresh();
      }
    }
  }
  file.close();
  if (!valid)
  {
    Serial.println(F("bitmap format not handled."));
  }
}

uint16_t read16(SdFile& f)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(SdFile& f)
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
