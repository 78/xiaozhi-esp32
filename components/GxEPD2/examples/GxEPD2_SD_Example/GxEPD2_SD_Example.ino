// GxEPD2_SD_Example : Display Library example for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
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
// NOTE for use with Waveshare ESP32 Driver Board:
// **** also need to select the constructor with the parameters for this board in GxEPD2_display_selection_new_style.h ****
//
// The Wavehare ESP32 Driver Board uses uncommon SPI pins for the FPC connector. It uses HSPI pins, but SCK and MOSI are swapped.
// To use HW SPI with the ESP32 Driver Board, HW SPI pins need be re-mapped in any case. Can be done using either HSPI or VSPI.
// Other SPI clients can either be connected to the same SPI bus as the e-paper, or to the other HW SPI bus, or through SW SPI.
// The logical configuration would be to use the e-paper connection on HSPI with re-mapped pins, and use VSPI for other SPI clients.
// VSPI with standard VSPI pins is used by the global SPI instance of the Arduino IDE ESP32 package.
//
// Alternately VSPI with re-mapped pins can be used with the ESP32 Driver Board FPC connector.
// This was used with the original example GxEPD2_WS_ESP32_Driver.ino.
// Then the standard HW SPI pins can be used for other clients through HSPI with re-mapped pins.
// This is not the prefered configuration, but also works. Available in this example for test.

// uncomment next line to use HSPI for EPD (and VSPI for SD), e.g. with Waveshare ESP32 Driver Board
//#define USE_HSPI_FOR_EPD
// alternately uncomment next line to use HSPI for SD (and VSPI for EPD), e.g. with Waveshare ESP32 Driver Board
//#define USE_HSPI_FOR_SD

// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable or disable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
//#include <GFX.h>
// Note: if you use this with ENABLE_GxEPD2_GFX 1:
//       uncomment it in GxEPD2_GFX.h too, or add #include <GFX.h> before any #include <GxEPD2_GFX.h>

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeMonoBold9pt7b.h>

#if defined(ESP32)

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#else

#include <SPI.h>
#include <SD.h>

#endif

// #define EPD_CS before #include "GxEPD2_display_selection_new_style.h" to override default
#if defined (ESP8266)
#define SD_CS SS  // e.g. for RobotDyn Wemos D1 mini SD board
#define EPD_CS D1 // alternative I use with RobotDyn Wemos D1 mini SD board
#endif

#if defined(ESP32)
#if defined(USE_HSPI_FOR_EPD) || defined(USE_HSPI_FOR_SD)
#define SD_CS SS
#define EPD_CS 15
SPIClass hspi(HSPI);
#else
#define SD_CS 2  // adapted to my wiring
#endif
#endif

#if defined(ARDUINO_ARCH_RP2040) && defined(ARDUINO_RASPBERRY_PI_PICO)
// Waveshare PhotoPainter
#define EPD_RST_PIN     12
#define EPD_DC_PIN      8
#define EPD_CS_PIN      9
#define EPD_BUSY_PIN    13
#define EPD_CLK_PIN     10  
#define EPD_MOSI_PIN    11
#define SD_CS_PIN       5
#define SD_CLK_PIN      2
#define SD_MOSI_PIN     3
#define SD_MISO_PIN     4
#define EPD_CS EPD_CS_PIN
#define SD_CS SD_CS_PIN
#if defined(__MBED__)
// MbedSPI(int miso, int mosi, int sck);
arduino::MbedSPI EPD_SPI(12, EPD_MOSI_PIN, EPD_CLK_PIN);
arduino::MbedSPI SD_SPI(SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
#else // package https://github.com/earlephilhower/arduino-pico
// SPIClassRP2040(spi_inst_t *spi, pin_size_t rx, pin_size_t cs, pin_size_t sck, pin_size_t tx);
SPIClassRP2040 EPD_SPI(spi1, 12, 13, 10, 11);
SPIClassRP2040 SD_SPI(spi0, SD_MISO_PIN, SD_CS_PIN, SD_CLK_PIN, SD_MOSI_PIN);
#endif
#endif

#if defined(__AVR)
#define SD_CS 6  // adapt to your wiring
#define EPD_CS SS // adapt to your wiring
// most AVR Arduinos have not enough RAM for use with SD and buffered graphics
// !!! use GxEPD2_SD_AVR_Example.ino instead !!!
//GxEPD2_154 display(/*CS=10*/ EPD_CS, /*DC=*/ 8, /*RST=*/ 9, /*BUSY=*/ 7);
#endif

// select the display class and display driver class in the following file (new style):
// don't forget to modify or override EPD_CS if needed
#include "GxEPD2_display_selection_new_style.h"

// note that BMP bitmaps are drawn at physical position in physical orientation of the screen

// function declaration with default parameter
void drawBitmapFromSD(const char *filename, int16_t x, int16_t y, bool with_color = true);

// bitmap drawing using buffered graphics, e.g. for small bitmaps or for GxEPD2_154c
// draws BMP bitmap according to set orientation
// partial_update selects refresh mode (not effective for GxEPD2_154c)
// overwrite = true does not clear buffer before drawing, use only if buffer is full height
void drawBitmapFromSD_Buffered(const char *filename, int16_t x, int16_t y, bool with_color = true, bool partial_update = false, bool overwrite = false);

void setup()
{
  delay(5000);
  Serial.begin(115200);
  Serial.println();
  Serial.println("GxEPD2_SD_Example");

#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
  hspi.begin(13, 12, 14, 15); // remap hspi for EPD (swap pins)
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#elif defined(ESP32) && defined(USE_HSPI_FOR_SD)
  SPI.begin(13, 12, 14, 15); // remap SPI for EPD
  hspi.begin(SCK, MISO, MOSI, SS); // remap hspi for SD
#endif

#if defined(ARDUINO_ARCH_RP2040) && defined(ARDUINO_RASPBERRY_PI_PICO)
  display.epd2.selectSPI(EPD_SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  // uncomment next line for Waveshare PhotoPainter module
  pinMode(16, OUTPUT); digitalWrite(16, HIGH); // power to the paper
#endif

  //display.init(115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
  display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse

  Serial.print("Initializing SD card...");
#if defined(ESP32) && defined(USE_HSPI_FOR_SD)
  if (!SD.begin(SD_CS, hspi))
  {
    Serial.println("SD failed!");
    return;
  }
#elif defined(ARDUINO_ARCH_RP2040) && defined(ARDUINO_RASPBERRY_PI_PICO)
  if (!SD.begin(SD_CS, SD_SPI))
  {
    Serial.println("SD failed!");
    return;
  }
#else
  if (!SD.begin(SD_CS))
  {
    Serial.println("SD failed!");
    return;
  }
#endif
  Serial.println("SD OK!");

  listFiles();

  if ((display.epd2.panel == GxEPD2::GDEW0154Z04) || (display.epd2.panel == GxEPD2::ACeP565) || (display.epd2.panel == GxEPD2::GDEY073D46) || (display.epd2.panel == GxEPD2::ACeP730) || false)
  {
    //drawBitmapsBuffered_200x200();
    //drawBitmapsBuffered_other();
    drawBitmapsBuffered_test();
    //drawBitmapsBuffered_7c();
  }
  else
  {
    drawBitmaps_200x200();
    drawBitmaps_other();
  }

  //drawBitmaps_test();
  //drawBitmapsBuffered_test();

  Serial.println("GxEPD2_SD_Example done");
}

void loop(void)
{
}

void listFiles()
{
  Serial.println("All Files on SD:");
  File root = SD.open("/");
  if (root)
  {
    if (root.isDirectory())
    {
      File file = root.openNextFile();
      while (file)
      {
        Serial.print("  ");
        Serial.print(file.name());
        spaces(20 - strlen(file.name()));
        Serial.print("  ");
        Serial.print(file.size());
        Serial.print(" bytes");
        Serial.println();
        file = root.openNextFile();
      }
      Serial.println("no more files...");
    } else Serial.print("Not a directory");
  } else Serial.print("failed to open root directory");
}

void spaces(int num)
{
  for (int i = 0; i < num; i++)
  {
    Serial.print(" ");
  }
}

void drawBitmaps_200x200()
{
  int16_t x = (display.width() - 200) / 2;
  int16_t y = (display.height() - 200) / 2;
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
  int16_t w2 = display.width() / 2;
  int16_t h2 = display.height() / 2;
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
  int16_t w2 = display.width() / 2;
  int16_t h2 = display.height() / 2;
  drawBitmapFromSD("betty_4.bmp", w2 - 102, h2 - 126);
  delay(2000);
  drawBitmapFromSD("bb4.bmp", 0, 0);
  delay(2000);
  drawBitmapFromSD("output5.bmp", 0, 0);
  delay(2000);
  drawBitmapFromSD("output6.bmp", 0, 0);
  delay(2000);
  drawBitmapFromSD("tractor_1.bmp", 0, 0);
  delay(2000);
  drawBitmapFromSD("tractor_4.bmp", 0, 0);
  delay(2000);
  //drawBitmapFromSD("tractor_8.bmp", 0, 0); // format 1: BI_RLE8 is not supported
  //delay(2000);
  drawBitmapFromSD("tractor_11.bmp", 0, 0);
  delay(2000);
  drawBitmapFromSD("tractor_44.bmp", 0, 0);
  delay(2000);
  drawBitmapFromSD("tractor_88.bmp", 0, 0);
  delay(2000);
}

void drawBitmapsBuffered_200x200()
{
  int16_t x = (display.width() - 200) / 2;
  int16_t y = (display.height() - 200) / 2;
  drawBitmapFromSD_Buffered("logo200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD_Buffered("first200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD_Buffered("second200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD_Buffered("third200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD_Buffered("fourth200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD_Buffered("fifth200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD_Buffered("sixth200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD_Buffered("seventh200x200.bmp", x, y);
  delay(2000);
  drawBitmapFromSD_Buffered("eighth200x200.bmp", x, y);
  delay(2000);
}

void drawBitmapsBuffered_other()
{
  int16_t w2 = display.width() / 2;
  int16_t h2 = display.height() / 2;
  drawBitmapFromSD_Buffered("parrot.bmp", w2 - 64, h2 - 80);
  delay(2000);
  drawBitmapFromSD_Buffered("betty_1.bmp", w2 - 100, h2 - 160);
  delay(2000);
  drawBitmapFromSD_Buffered("betty_4.bmp", w2 - 102, h2 - 126);
  delay(2000);
  drawBitmapFromSD_Buffered("marilyn_240x240x8.bmp", w2 - 120, h2 - 120);
  delay(2000);
  drawBitmapFromSD_Buffered("miniwoof.bmp", w2 - 60, h2 - 80);
  delay(2000);
  drawBitmapFromSD_Buffered("t200x200.bmp", w2 - 100, h2 - 100);
  delay(2000);
  drawBitmapFromSD_Buffered("test.bmp", w2 - 120, h2 - 160);
  delay(2000);
  drawBitmapFromSD_Buffered("tiger.bmp", w2 - 160, h2 - 120);
  delay(2000);
  drawBitmapFromSD_Buffered("tiger_178x160x4.bmp", w2 - 89, h2 - 80);
  delay(2000);
  drawBitmapFromSD_Buffered("tiger_240x317x4.bmp", w2 - 120, h2 - 160);
  delay(2000);
  drawBitmapFromSD_Buffered("tiger_320x200x24.bmp", w2 - 160, h2 - 100);
  delay(2000);
  drawBitmapFromSD_Buffered("tiger16T.bmp", w2 - 160, h2 - 120);
  delay(2000);
  drawBitmapFromSD_Buffered("woof.bmp", w2 - 120, h2 - 160);
  delay(2000);
  drawBitmapFromSD_Buffered("bitmap640x384_1.bmp", 0, 0);
  delay(2000);
}

void drawBitmapsBuffered_test()
{
  int16_t w2 = display.width() / 2;
  int16_t h2 = display.height() / 2;
  //drawBitmapFromSD_Buffered("betty_4.bmp", w2 - 102, h2 - 126);
  //delay(2000);
  drawBitmapFromSD_Buffered("bb4.bmp", w2 - 17, h2 - 9, false, true, true);
  delay(2000);
  drawBitmapFromSD_Buffered("rgb32.bmp", w2 - 64, h2 - 32);
  delay(2000);
  drawBitmapFromSD_Buffered("parrot.bmp", w2- 64, h2 - 80);
  delay(2000);
  drawBitmapFromSD_Buffered("5in65f3.bmp", w2 - 300, h2 - 224);
  delay(2000);
}

void drawBitmapsBuffered_7c()
{
  drawBitmapFromSD_Buffered("pic/building-7362300_1920_scale_output.bmp", 0, 0);
  delay(3000);
  drawBitmapFromSD_Buffered("pic/girl-3480900_1920_scale_output.bmp", 0, 0);
  delay(3000);
  drawBitmapFromSD_Buffered("pic/miniature-3589682_1920_scale_output.bmp", 0, 0);
  delay(3000);
  display.setRotation(1);
  drawBitmapFromSD_Buffered("pic/pexels-boys-in-bristol-photography-13318095_scale_output.bmp", 0, 0);
  delay(3000);
  drawBitmapFromSD_Buffered("pic/pexels-efe-ersoy-15686039_scale_output.bmp", 0, 0);
  delay(3000);
  drawBitmapFromSD_Buffered("pic/pexels-jill-burrow-6069730_scale_output.bmp", 0, 0);
  delay(3000);
  drawBitmapFromSD_Buffered("pic/pexels-yelena-odintsova-15792199_scale_output.bmp", 0, 0);
  delay(3000);
  drawBitmapFromSD_Buffered("pic/pexels-ольга-солодилова-15912787_scale_output.bmp", 0, 0);
  delay(3000);
  display.setRotation(0);
  drawBitmapFromSD_Buffered("pic/venice-2896591_1920_scale_output", 0, 0);
  delay(3000);
  drawBitmapFromSD_Buffered("pic/venice-2937352_1920_scale_output.bmp", 0, 0);
  delay(3000);
}

//static const uint16_t input_buffer_pixels = 20; // may affect performance
static const uint16_t input_buffer_pixels = 800; // may affect performance

static const uint16_t max_row_width = 1448; // for up to 6" display 1448x1072
static const uint16_t max_palette_pixels = 256; // for depth <= 8

uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
uint8_t output_row_mono_buffer[max_row_width / 8]; // buffer for at least one row of b/w bits
uint8_t output_row_color_buffer[max_row_width / 8]; // buffer for at least one row of color bits
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w
uint16_t rgb_palette_buffer[max_palette_pixels]; // palette buffer for depth <= 8 for buffered graphics, needed for 7-color display

void drawBitmapFromSD(const char *filename, int16_t x, int16_t y, bool with_color)
{
  File file;
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  uint32_t startTime = millis();
  if ((x >= display.epd2.WIDTH) || (y >= display.epd2.HEIGHT)) return;
  Serial.println();
  Serial.print("Loading image '");
  Serial.print(filename);
  Serial.println('\'');
#if defined(ESP32)
  file = SD.open(String("/") + filename, FILE_READ);
  if (!file)
  {
    Serial.print("File not found");
    return;
  }
#else
  file = SD.open(filename);
  if (!file)
  {
    Serial.print("File not found");
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
      Serial.print("File size: "); Serial.println(fileSize);
      Serial.print("Image Offset: "); Serial.println(imageOffset);
      Serial.print("Header size: "); Serial.println(headerSize);
      Serial.print("Bit Depth: "); Serial.println(depth);
      Serial.print("Image size: ");
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
      if ((x + w - 1) >= display.epd2.WIDTH)  w = display.epd2.WIDTH  - x;
      if ((y + h - 1) >= display.epd2.HEIGHT) h = display.epd2.HEIGHT - y;
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
          //file.seek(54); //palette is always @ 54
          file.seek(imageOffset - (4 << depth)); // 54 for regular, diff for colorsimportant
          for (uint16_t pn = 0; pn < (1 << depth); pn++)
          {
            blue  = file.read();
            green = file.read();
            red   = file.read();
            file.read();
            //Serial.print(red); Serial.print(" "); Serial.print(green); Serial.print(" "); Serial.println(blue);
            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
            if (0 == pn % 8) mono_palette_buffer[pn / 8] = 0;
            mono_palette_buffer[pn / 8] |= whitish << pn % 8;
            if (0 == pn % 8) color_palette_buffer[pn / 8] = 0;
            color_palette_buffer[pn / 8] |= colored << pn % 8;
          }
        }
        display.clearScreen();
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
          file.seek(rowPosition);
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
        Serial.print("loaded in "); Serial.print(millis() - startTime); Serial.println(" ms");
        display.refresh();
      }
    }
  }
  file.close();
  if (!valid)
  {
    Serial.println("bitmap format not handled.");
  }
}

void drawBitmapFromSD_Buffered(const char *filename, int16_t x, int16_t y, bool with_color, bool partial_update, bool overwrite)
{
  File file;
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  bool has_multicolors = (display.epd2.panel == GxEPD2::ACeP565) || (display.epd2.panel == GxEPD2::GDEY073D46);
  uint32_t startTime = millis();
  if ((x >= display.width()) || (y >= display.height())) return;
  Serial.println();
  Serial.print("Loading image '");
  Serial.print(filename);
  Serial.println('\'');
#if defined(ESP32)
  file = SD.open(String("/") + filename, FILE_READ);
  if (!file)
  {
    Serial.print("File not found");
    return;
  }
#else
  file = SD.open(filename);
  if (!file)
  {
    Serial.print("File not found");
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
      Serial.print("File size: "); Serial.println(fileSize);
      Serial.print("Image Offset: "); Serial.println(imageOffset);
      Serial.print("Header size: "); Serial.println(headerSize);
      Serial.print("Bit Depth: "); Serial.println(depth);
      Serial.print("Image size: ");
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
      if ((x + w - 1) >= display.width())  w = display.width()  - x;
      if ((y + h - 1) >= display.height()) h = display.height() - y;
      //if (w <= max_row_width) // handle with direct drawing
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
          //file.seek(54); //palette is always @ 54
          file.seek(imageOffset - (4 << depth)); //54 for regular, diff for colorsimportant
          for (uint16_t pn = 0; pn < (1 << depth); pn++)
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
            rgb_palette_buffer[pn] = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);
          }
        }
        if (partial_update) display.setPartialWindow(x, y, w, h);
        else display.setFullWindow();
        display.firstPage();
        do
        {
          //if (!overwrite) display.fillScreen(GxEPD_WHITE);
          uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
          for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
          {
            uint32_t in_remain = rowSize;
            uint32_t in_idx = 0;
            uint32_t in_bytes = 0;
            uint8_t in_byte = 0; // for depth <= 8
            uint8_t in_bits = 0; // for depth <= 8
            uint16_t color = GxEPD_WHITE;
            file.seek(rowPosition);
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
                  color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);
                  break;
                case 24:
                  blue = input_buffer[in_idx++];
                  green = input_buffer[in_idx++];
                  red = input_buffer[in_idx++];
                  whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                  colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                  color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);
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
                      color = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | ((blue & 0xF8) >> 3);
                    }
                    else // 565
                    {
                      blue  = (lsb & 0x1F) << 3;
                      green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                      red   = (msb & 0xF8);
                      color = (msb << 8) | lsb;
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
                    color = rgb_palette_buffer[pn];
                  }
                  break;
              }
              if (with_color && has_multicolors)
              {
                // keep color
              }
              else if (whitish)
              {
                color = GxEPD_WHITE;
              }
              else if (colored && with_color)
              {
                color = GxEPD_COLORED;
              }
              else
              {
                color = GxEPD_BLACK;
              }
              uint16_t yrow = y + (flip ? h - row - 1 : row);
              display.drawPixel(x + col, yrow, color);
            } // end pixel
          } // end line
          Serial.print("page loaded in "); Serial.print(millis() - startTime); Serial.println(" ms");
        }
        while (display.nextPage());
        Serial.print("loaded in "); Serial.print(millis() - startTime); Serial.println(" ms");
      }
    }
  }
  file.close();
  if (!valid)
  {
    Serial.println("bitmap format not handled.");
  }
}

uint16_t read16(File& f)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File& f)
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
