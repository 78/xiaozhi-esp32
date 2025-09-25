// Display Library example for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Display Library based on Demo Example from Good Display: https://www.good-display.com/companyfile/32/
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

// Supporting Arduino Forum Topics (closed, read only):
// Good Display ePaper for Arduino: https://forum.arduino.cc/t/good-display-epaper-for-arduino/419657
// Waveshare e-paper displays with SPI: https://forum.arduino.cc/t/waveshare-e-paper-displays-with-spi/467865
//
// Add new topics in https://forum.arduino.cc/c/using-arduino/displays/23 for new questions and issues

// see GxEPD2_wiring_examples.h for wiring suggestions and examples

// NOTE for use with Waveshare ESP32 Driver Board:
// **** also need to select the constructor with the parameters for this board in GxEPD2_display_selection_new_style.h ****
//
// The Wavehare ESP32 Driver Board uses uncommon SPI pins for the FPC connector. It uses HSPI pins, but SCK and MOSI are swapped.
// To use HW SPI with the ESP32 Driver Board, HW SPI pins need be re-mapped in any case. Can be done using either HSPI or VSPI.
// Other SPI clients can either be connected to the same SPI bus as the e-paper, or to the other HW SPI bus, or through SW SPI.
// The logical configuration would be to use the e-paper connection on HSPI with re-mapped pins, and use VSPI for other SPI clients.
// VSPI with standard VSPI pins is used by the global SPI instance of the Arduino IDE ESP32 package.

// uncomment next line to use HSPI for EPD (and e.g VSPI for SD), e.g. with Waveshare ESP32 Driver Board
//#define USE_HSPI_FOR_EPD

// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable or disable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
//#include <GFX.h>
// Note: if you use this with ENABLE_GxEPD2_GFX 1:
//       uncomment it in GxEPD2_GFX.h too, or add #include <GFX.h> before any #include <GxEPD2_GFX.h>

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

// select the display constructor line in one of the following files (old style):
#include "GxEPD2_display_selection.h"
#include "GxEPD2_display_selection_added.h"

// or select the display class and display driver class in the following file (new style):
#include "GxEPD2_display_selection_new_style.h"

//#define WITHOUT_BITMAPS
#ifndef WITHOUT_BITMAPS

#if !defined(__AVR) && !defined(STM32F1xx)

// note 16.11.2019: the compiler may exclude code based on constant if statements (display.epd2.panel == constant),
//                  therefore bitmaps may get optimized out by the linker

// comment out unused bitmaps to reduce code space used
#include "bitmaps/Bitmaps80x128.h"  // 1.02" b/w
#include "bitmaps/Bitmaps152x152.h" // 1.54" b/w
#include "bitmaps/Bitmaps200x200.h" // 1.54" b/w
#include "bitmaps/Bitmaps104x212.h" // 2.13" b/w flexible GDEW0213I5F
#include "bitmaps/Bitmaps128x250.h" // 2.13" b/w
#include "bitmaps/Bitmaps128x296.h" // 2.9"  b/w
#include "bitmaps/Bitmaps168x384.h" // 2.9"  b/w
#include "bitmaps/Bitmaps152x296.h" // 2.6"  b/w
#include "bitmaps/Bitmaps176x264.h" // 2.7"  b/w
#include "bitmaps/Bitmaps240x360.h" // 3.1" b/w
#include "bitmaps/Bitmaps240x416.h" // 3.71"  b/w
#include "bitmaps/Bitmaps400x300.h" // 4.2"  b/w
#include "bitmaps/Bitmaps648x480.h" // 5.38"  b/w
#include "bitmaps/Bitmaps640x384.h" // 7.5"  b/w
#include "bitmaps/Bitmaps800x276.h" // 5.79" b/w
#include "bitmaps/Bitmaps800x480.h" // 7.5"  b/w
#include "bitmaps/Bitmaps960x640.h" // 10.2"  b/w
#include "bitmaps/Bitmaps960x680.h" // 13.3"  b/w
// 3-color
#include "bitmaps/Bitmaps3c200x200.h" // 1.54" b/w/r
#include "bitmaps/Bitmaps3c104x212.h" // 2.13" b/w/r
#include "bitmaps/Bitmaps3c128x250.h" // 2.13" b/w/r
#include "bitmaps/Bitmaps3c128x296.h" // 2.9"  b/w/r
#include "bitmaps/Bitmaps3c152x296.h" // 2.66" b/w/r
#include "bitmaps/Bitmaps3c176x264.h" // 2.7"  b/w/r
#include "bitmaps/Bitmaps3c400x300.h" // 4.2"  b/w/r
#if defined(ESP8266) || defined(ESP32) || defined(ARDUINO_ARCH_RP2040)
#include "bitmaps/Bitmaps3c800x276.h" // 5.79" b/w/r
#include "bitmaps/Bitmaps3c648x480.h" // 5.83" b/w/r
#include "bitmaps/Bitmaps3c800x480.h" // 7.5"  b/w/r
#include "bitmaps/Bitmaps3c880x528.h" // 7.5"  b/w/r
#include "bitmaps/Bitmaps3c960x640.h" // 11.6" b/w/r
#include "bitmaps/Bitmaps3c960x680.h" // 13.3" b/w/r
#include "bitmaps/WS_Bitmaps800x600.h" // 6.0"  grey
// 4-color
#include "bitmaps/Bitmaps4c128x250.h" // 2.13" 4-color
#include "bitmaps/Bitmaps4c184x360.h" // 2.66" 4-color
#include "bitmaps/Bitmaps4c168x384.h" // 2.9" 4-color
#include "bitmaps/WS_Bitmaps4c168x168.h" // 4.37" 4-color
#include "bitmaps/WS_Bitmaps4c168x400.h" // 3.00" 4-color
#include "bitmaps/Bitmaps4c400x300.h" // 4.2" 4-color
#include "bitmaps/Bitmaps4c792x272.h" // 5.79" 4-color
#include "bitmaps/Bitmaps4c960x640.h" // 11.6" 4-color
// 7-color
#include "bitmaps/Bitmaps7c600x448.h" // 5.65" 7-color
#include "bitmaps/WS_Bitmaps7c192x143.h" // 5.65" 7-color
#include "bitmaps/WS_Bitmaps7c300x180.h" // 7.3" 7-color
#endif
#if defined(ESP32) || defined(ESP_PLATFORM) || defined(ARDUINO_ARCH_RP2040)
#include "bitmaps/Bitmaps1304x984.h" // 12.48" b/w
#include "bitmaps/Bitmaps1360x480.h" // 10.85" b/w
#include "bitmaps/Bitmaps3c1304x984.h" // 12.48" b/w/r
#include "bitmaps/Bitmaps7c800x480.h" // 7.3" 7-color
#endif

#else

// select only one to fit in code space
//#include "bitmaps/Bitmaps80x128.h"  // 1.02" b/w
#include "bitmaps/Bitmaps200x200.h" // 1.54" b/w
//#include "bitmaps/Bitmaps104x212.h" // 2.13" b/w flexible GDEW0213I5F
//#include "bitmaps/Bitmaps128x250.h" // 2.13" b/w
//#include "bitmaps/Bitmaps128x296.h" // 2.9"  b/w
#include "bitmaps/Bitmaps168x384.h" // 2.9"  b/w
//#include "bitmaps/Bitmaps176x264.h" // 2.7"  b/w
////#include "bitmaps/Bitmaps400x300.h" // 4.2"  b/w // not enough code space
////#include "bitmaps/Bitmaps640x384.h" // 7.5"  b/w // not enough code space
////#include "bitmaps/Bitmaps800x480.h" // 7.5"  b/w // not enough code space
// 3-color
//#include "bitmaps/Bitmaps3c200x200.h" // 1.54" b/w/r
//#include "bitmaps/Bitmaps3c104x212.h" // 2.13" b/w/r
#include "bitmaps/Bitmaps3c128x250.h" // 2.13" b/w/r
//#include "bitmaps/Bitmaps3c128x296.h" // 2.9"  b/w/r
//#include "bitmaps/Bitmaps3c176x264.h" // 2.7"  b/w/r
////#include "bitmaps/Bitmaps3c400x300.h" // 4.2"  b/w/r // not enough code space

#endif
#endif

#if defined(ARDUINO_ARCH_RP2040) && (defined(ARDUINO_RASPBERRY_PI_PICO) || defined(ARDUINO_RASPBERRY_PI_PICO_W))
#if defined(__MBED__)
// SPI pins used by GoodDisplay DESPI-PICO. note: steals standard I2C pins PIN_WIRE_SDA (6), PIN_WIRE_SCL (7)
// uncomment next line for use with GoodDisplay DESPI-PICO. // MbedSPI(int miso, int mosi, int sck);
arduino::MbedSPI SPIn(4, 7, 6); // need be valid pins for same SPI channel, else fails blinking 4 long 4 short
// uncomment next line for use with my proto board. // MbedSPI(int miso, int mosi, int sck);
//arduino::MbedSPI SPIn(4, 3, 2); // need be valid pins for same SPI channel, else fails blinking 4 long 4 short
// uncomment next line for use with Waveshare Pico-ePaper-2.9. // MbedSPI(int miso, int mosi, int sck);
// note: doesn't work with Waveshare PhotoPainter, conflict on pin 12. use philhower package instead.
//arduino::MbedSPI SPIn(12, 11, 10); // need be valid pins for same SPI channel, else fails blinking 4 long 4 short
#else // package https://github.com/earlephilhower/arduino-pico
// SPIClassRP2040(spi_inst_t *spi, pin_size_t rx, pin_size_t cs, pin_size_t sck, pin_size_t tx);
// uncomment next line for use with my proto board.
//SPIClassRP2040 SPIn(spi0, 4, 5, 2, 3); // need be valid pins for same SPI channel, else fails blinking 4 long 4 short
// uncomment next line for use with Waveshare Pico-ePaper-2.9 or Waveshare PhotoPainter module
SPIClassRP2040 SPIn(spi1, 12, 13, 10, 11); // need be valid pins for same SPI channel, else fails blinking 4 long 4 short
#endif
#endif

#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
SPIClass hspi(HSPI);
#endif

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");
  delay(100);
#if defined(ARDUINO_ARCH_RP2040) && (defined(ARDUINO_RASPBERRY_PI_PICO) || defined(ARDUINO_RASPBERRY_PI_PICO_W))
  // uncomment next line for use with GoodDisplay DESPI-PICO or my proto board, or Waveshare RPi boards
  display.epd2.selectSPI(SPIn, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  // uncomment next 2 lines to allow recovery from configuration failures
  pinMode(15, INPUT_PULLUP); // safety pin
  while (!digitalRead(15)) delay(100); // check safety pin for fail recovery
  // recovery can be done also by holding BOOTSEL during power-up.
  // uncomment next line for Waveshare PhotoPainter module
  pinMode(16, OUTPUT); digitalWrite(16, HIGH); // power to the paper
#endif
#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
  hspi.begin(13, 12, 14, 15); // remap hspi for EPD (swap pins)
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#elif (defined(ARDUINO_ARCH_ESP32) && defined(ARDUINO_LOLIN_S2_MINI))
  // SPI.begin(sck, miso, mosi, ss); // preset for remapped pins
  SPI.begin(18, -1, 16, 33); // my LOLIN ESP32 S2 mini connection
#endif
#if defined(ESP32) && defined(ARDUINO_ESP32S3_DEV) && true // CrowPanel
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH); // enable power to the panel
#endif
  //display.init(115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
  display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  //display.init(115200, true, 10, false, SPIn, SPISettings(4000000, MSBFIRST, SPI_MODE0)); // extended init method with SPI channel and/or settings selection
  if (display.pages() > 1)
  {
    delay(100);
    Serial.print("pages = "); Serial.print(display.pages()); Serial.print(" page height = "); Serial.println(display.pageHeight());
    delay(1000);
  }
  //display.clearScreen(); return;
  // first update should be full refresh
  helloWorld();
  delay(1000);
  // partial refresh mode can be used to full screen,
  // effective if display panel hasFastPartialUpdate
  helloFullScreenPartialMode();
  delay(1000);
  //stripeTest(); return; // GDEH029Z13 issue
  helloArduino();
  delay(1000);
  helloEpaper();
  delay(1000);
  //helloValue(123.9, 1);
  //delay(1000);
  showFont("FreeMonoBold9pt7b", &FreeMonoBold9pt7b);
  //showFont("FreeMonoBold12pt7b", &FreeMonoBold12pt7b);
  delay(1000);
  if (display.epd2.WIDTH < 104)
  {
    showFont("glcdfont", 0);
    delay(1000);
  }
  //drawGrid(); return;
  //drawCornerTest(); return;
  display.writeScreenBuffer();
  drawBitmaps();
  //display.powerOff(); return;
  drawGraphics();
  //display.powerOff(); return;
#if !defined(__AVR) // takes too long!
  if ((display.epd2.panel == GxEPD2::ACeP565) || (display.epd2.panel == GxEPD2::GDEY073D46) || (display.epd2.panel == GxEPD2::ACeP730) ||
      (display.epd2.panel == GxEPD2::GDEP0565D90) || (display.epd2.panel == GxEPD2::GDEP073E01))
  {
    //draw7colorlines();
    //delay(2000);
    draw7colors();
    delay(4000);
    display.epd2.drawNativeColors(); delay(2000);
    //return;
  }
#endif
  if (display.epd2.hasPartialUpdate)
  {
    showPartialUpdate();
    delay(1000);
  } // else // on GDEW0154Z04 only full update available, doesn't look nice
  //drawCornerTest();
  //showBox(16, 16, 48, 32, false);
  //showBox(16, 56, 48, 32, true);
  display.powerOff();
  deepSleepTest();
#if defined(ESP32) && defined(_GxBitmaps1304x984_H_)
  drawBitmaps1304x984();
  display.powerOff();
#endif
  Serial.println("setup done");
  display.end();
}

void loop()
{
}

// note for partial update window and setPartialWindow() method:
// partial update window size and position is on byte boundary in physical x direction
// the size is increased in setPartialWindow() if x or w are not multiple of 8 for even rotation, y or h for odd rotation
// see also comment in GxEPD2_BW.h, GxEPD2_3C.h or GxEPD2_GFX.h for method setPartialWindow()

const char HelloWorld[] = "Hello World!";
const char HelloArduino[] = "Hello Arduino!";
const char HelloEpaper[] = "Hello E-Paper!";

void helloWorld()
{
  //Serial.println("helloWorld");
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  if (display.epd2.WIDTH < 104) display.setFont(0);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center bounding box by transposition of origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(HelloWorld);
  }
  while (display.nextPage());
  //Serial.println("helloWorld done");
}

void helloWorldForDummies()
{
  //Serial.println("helloWorld");
  const char text[] = "Hello World!";
  // most e-papers have width < height (portrait) as native orientation, especially the small ones
  // in GxEPD2 rotation 0 is used for native orientation (most TFT libraries use 0 fix for portrait orientation)
  // set rotation to 1 (rotate right 90 degrees) to have enough space on small displays (landscape)
  display.setRotation(1);
  // select a suitable font in Adafruit_GFX
  display.setFont(&FreeMonoBold9pt7b);
  // on e-papers black on white is more pleasant to read
  display.setTextColor(GxEPD_BLACK);
  // Adafruit_GFX has a handy method getTextBounds() to determine the boundary box for a text for the actual font
  int16_t tbx, tby; uint16_t tbw, tbh; // boundary box window
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh); // it works for origin 0, 0, fortunately (negative tby!)
  // center bounding box by transposition of origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  // full window mode is the initial mode, set it anyway
  display.setFullWindow();
  // here we use paged drawing, even if the processor has enough RAM for full buffer
  // so this can be used with any supported processor board.
  // the cost in code overhead and execution time penalty is marginal
  // tell the graphics class to use paged drawing mode
  display.firstPage();
  do
  {
    // this part of code is executed multiple times, as many as needed,
    // in case of full buffer it is executed once
    // IMPORTANT: each iteration needs to draw the same, to avoid strange effects
    // use a copy of values that might change, don't read e.g. from analog or pins in the loop!
    display.fillScreen(GxEPD_WHITE); // set the background to white (fill the buffer with value for white)
    display.setCursor(x, y); // set the postition to start printing text
    display.print(text); // print some text
    // end of part executed multiple times
  }
  // tell the graphics class to transfer the buffer content (page) to the controller buffer
  // the graphics class will command the controller to refresh to the screen when the last page has been transferred
  // returns true if more pages need be drawn and transferred
  // returns false if the last page has been transferred and the screen refreshed for panels without fast partial update
  // returns false for panels with fast partial update when the controller buffer has been written once more, to make the differential buffers equal
  // (for full buffered with fast partial update the (full) buffer is just transferred again, and false returned)
  while (display.nextPage());
  //Serial.println("helloWorld done");
}

void helloFullScreenPartialMode()
{
  //Serial.println("helloFullScreenPartialMode");
  const char fullscreen[] = "full screen update";
  const char fpm[] = "fast partial mode";
  const char spm[] = "slow partial mode";
  const char npm[] = "no partial mode";
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  if (display.epd2.WIDTH < 104) display.setFont(0);
  display.setTextColor(GxEPD_BLACK);
  const char* updatemode;
  if (display.epd2.hasFastPartialUpdate)
  {
    updatemode = fpm;
  }
  else if (display.epd2.hasPartialUpdate)
  {
    updatemode = spm;
  }
  else
  {
    updatemode = npm;
  }
  // do this outside of the loop
  int16_t tbx, tby; uint16_t tbw, tbh;
  // center update text
  display.getTextBounds(fullscreen, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t utx = ((display.width() - tbw) / 2) - tbx;
  uint16_t uty = ((display.height() / 4) - tbh / 2) - tby;
  // center update mode
  display.getTextBounds(updatemode, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t umx = ((display.width() - tbw) / 2) - tbx;
  uint16_t umy = ((display.height() * 3 / 4) - tbh / 2) - tby;
  // center HelloWorld
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t hwx = ((display.width() - tbw) / 2) - tbx;
  uint16_t hwy = ((display.height() - tbh) / 2) - tby;
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(hwx, hwy);
    display.print(HelloWorld);
    display.setCursor(utx, uty);
    display.print(fullscreen);
    display.setCursor(umx, umy);
    display.print(updatemode);
  }
  while (display.nextPage());
  //Serial.println("helloFullScreenPartialMode done");
}

void helloArduino()
{
  //Serial.println("helloArduino");
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  if (display.epd2.WIDTH < 104) display.setFont(0);
  display.setTextColor(display.epd2.hasColor ? GxEPD_RED : GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  // align with centered HelloWorld
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  // height might be different
  display.getTextBounds(HelloArduino, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t y = ((display.height() / 4) - tbh / 2) - tby; // y is base line!
  // make the window big enough to cover (overwrite) descenders of previous text
  uint16_t wh = FreeMonoBold9pt7b.yAdvance;
  uint16_t wy = (display.height() / 4) - wh / 2;
  display.setPartialWindow(0, wy, display.width(), wh);
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    //display.drawRect(x, y - tbh, tbw, tbh, GxEPD_BLACK);
    display.setCursor(x, y);
    display.print(HelloArduino);
  }
  while (display.nextPage());
  delay(1000);
  //Serial.println("helloArduino done");
}

void helloEpaper()
{
  //Serial.println("helloEpaper");
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  if (display.epd2.WIDTH < 104) display.setFont(0);
  display.setTextColor(display.epd2.hasColor ? GxEPD_RED : GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  // align with centered HelloWorld
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  // height might be different
  display.getTextBounds(HelloEpaper, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t y = ((display.height() * 3 / 4) - tbh / 2) - tby; // y is base line!
  // make the window big enough to cover (overwrite) descenders of previous text
  uint16_t wh = FreeMonoBold9pt7b.yAdvance;
  uint16_t wy = (display.height() * 3 / 4) - wh / 2;
  display.setPartialWindow(0, wy, display.width(), wh);
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(HelloEpaper);
  }
  while (display.nextPage());
  //Serial.println("helloEpaper done");
}

// test partial window issue on GDEW0213Z19 and GDEH029Z13
void stripeTest()
{
  helloStripe(104);
  delay(2000);
  helloStripe(96);
}

const char HelloStripe[] = "Hello Stripe!";

void helloStripe(uint16_t pw_xe) // end of partial window in physcal x direction
{
  //Serial.print("HelloStripe("); Serial.print(pw_xe); Serial.println(")");
  display.setRotation(3);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(display.epd2.hasColor ? GxEPD_RED : GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(HelloStripe, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t wh = FreeMonoBold9pt7b.yAdvance;
  uint16_t wy = pw_xe - wh;
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = wy - tby;
  display.setPartialWindow(0, wy, display.width(), wh);
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(HelloStripe);
  }
  while (display.nextPage());
  //Serial.println("HelloStripe done");
}

#if defined(ESP8266) || defined(ESP32)
#include <StreamString.h>
#define PrintString StreamString
#else
class PrintString : public Print, public String
{
  public:
    size_t write(uint8_t data) override
    {
      return concat(char(data));
    };
};
#endif

void helloValue(double v, int digits)
{
  //Serial.println("helloValue");
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(display.epd2.hasColor ? GxEPD_RED : GxEPD_BLACK);
  PrintString valueString;
  valueString.print(v, digits);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(valueString, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() * 3 / 4) - tbh / 2) - tby; // y is base line!
  // show what happens, if we use the bounding box for partial window
  uint16_t wx = (display.width() - tbw) / 2;
  uint16_t wy = ((display.height() * 3 / 4) - tbh / 2);
  display.setPartialWindow(wx, wy, tbw, tbh);
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(valueString);
  }
  while (display.nextPage());
  delay(2000);
  // make the partial window big enough to cover the previous text
  uint16_t ww = tbw; // remember window width
  display.getTextBounds(HelloEpaper, 0, 0, &tbx, &tby, &tbw, &tbh);
  // adjust, because HelloEpaper was aligned, not centered (could calculate this to be precise)
  ww = max(ww, uint16_t(tbw + 12)); // 12 seems ok
  wx = (display.width() - tbw) / 2;
  // make the window big enough to cover (overwrite) descenders of previous text
  uint16_t wh = FreeMonoBold9pt7b.yAdvance;
  wy = (display.height() * 3 / 4) - wh / 2;
  display.setPartialWindow(wx, wy, ww, wh);
  // alternately use the whole width for partial window
  //display.setPartialWindow(0, wy, display.width(), wh);
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(valueString);
  }
  while (display.nextPage());
  //Serial.println("helloValue done");
}

void deepSleepTest()
{
  //Serial.println("deepSleepTest");
  const char hibernating[] = "hibernating ...";
  const char wokeup[] = "woke up";
  const char from[] = "from deep sleep";
  const char again[] = "again";
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  if (display.epd2.WIDTH < 104) display.setFont(0);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  // center text
  display.getTextBounds(hibernating, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(hibernating);
  }
  while (display.nextPage());
  display.hibernate();
  delay(5000);
  display.getTextBounds(wokeup, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t wx = (display.width() - tbw) / 2;
  uint16_t wy = ((display.height() / 3) - tbh / 2) - tby; // y is base line!
  display.getTextBounds(from, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t fx = (display.width() - tbw) / 2;
  uint16_t fy = ((display.height() * 2 / 3) - tbh / 2) - tby; // y is base line!
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(wx, wy);
    display.print(wokeup);
    display.setCursor(fx, fy);
    display.print(from);
  }
  while (display.nextPage());
  delay(5000);
  display.getTextBounds(hibernating, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t hx = (display.width() - tbw) / 2;
  uint16_t hy = ((display.height() / 3) - tbh / 2) - tby; // y is base line!
  display.getTextBounds(again, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t ax = (display.width() - tbw) / 2;
  uint16_t ay = ((display.height() * 2 / 3) - tbh / 2) - tby; // y is base line!
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(hx, hy);
    display.print(hibernating);
    display.setCursor(ax, ay);
    display.print(again);
  }
  while (display.nextPage());
  display.hibernate();
  //Serial.println("deepSleepTest done");
}

void showBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool partial)
{
  //Serial.println("showBox");
  display.setRotation(1);
  if (partial)
  {
    display.setPartialWindow(x, y, w, h);
  }
  else
  {
    display.setFullWindow();
  }
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.fillRect(x, y, w, h, GxEPD_BLACK);
  }
  while (display.nextPage());
  //Serial.println("showBox done");
}

void drawCornerTest()
{
  display.setFullWindow();
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  for (uint16_t r = 0; r <= 4; r++)
  {
    display.setRotation(r);
    display.firstPage();
    do
    {
      display.fillScreen(GxEPD_WHITE);
      display.fillRect(0, 0, 8, 8, GxEPD_BLACK);
      display.fillRect(display.width() - 18, 0, 16, 16, GxEPD_BLACK);
      display.fillRect(display.width() - 25, display.height() - 25, 24, 24, GxEPD_BLACK);
      display.fillRect(0, display.height() - 33, 32, 32, GxEPD_BLACK);
      display.setCursor(display.width() / 2, display.height() / 2);
      display.print(display.getRotation());
    }
    while (display.nextPage());
    delay(2000);
  }
}

void showFont(const char name[], const GFXfont* f)
{
  display.setFullWindow();
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.firstPage();
  do
  {
    drawFont(name, f);
  }
  while (display.nextPage());
}

void drawFont(const char name[], const GFXfont* f)
{
  //display.setRotation(0);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(f);
  display.setCursor(0, 0);
  display.println();
  display.println(name);
  display.println(" !\"#$%&'()*+,-./");
  display.println("0123456789:;<=>?");
  display.println("@ABCDEFGHIJKLMNO");
  display.println("PQRSTUVWXYZ[\\]^_");
  if (display.epd2.hasColor)
  {
    display.setTextColor(GxEPD_RED);
  }
  display.println("`abcdefghijklmno");
  display.println("pqrstuvwxyz{|}~ ");
}

// note for partial update window and setPartialWindow() method:
// partial update window size and position is on byte boundary in physical x direction
// the size is increased in setPartialWindow() if x or w are not multiple of 8 for even rotation, y or h for odd rotation
// see also comment in GxEPD2_BW.h, GxEPD2_3C.h or GxEPD2_GFX.h for method setPartialWindow()
// showPartialUpdate() purposely uses values that are not multiples of 8 to test this

void showPartialUpdate()
{
  // some useful background
  helloWorld();
  // use asymmetric values for test
  uint16_t box_x = 10;
  uint16_t box_y = 15;
  uint16_t box_w = 70;
  uint16_t box_h = 20;
  uint16_t cursor_y = box_y + box_h - 6;
  if (display.epd2.WIDTH < 104) cursor_y = box_y + 6;
  float value = 13.95;
  uint16_t incr = display.epd2.hasFastPartialUpdate ? 1 : 3;
  display.setFont(&FreeMonoBold9pt7b);
  if (display.epd2.WIDTH < 104) display.setFont(0);
  display.setTextColor(GxEPD_BLACK);
  // show where the update box is
  for (uint16_t r = 0; r < 4; r++)
  {
    display.setRotation(r);
    display.setPartialWindow(box_x, box_y, box_w, box_h);
    display.firstPage();
    do
    {
      display.fillRect(box_x, box_y, box_w, box_h, GxEPD_BLACK);
      //display.fillScreen(GxEPD_BLACK);
    }
    while (display.nextPage());
    delay(2000);
    display.firstPage();
    do
    {
      display.fillRect(box_x, box_y, box_w, box_h, GxEPD_WHITE);
    }
    while (display.nextPage());
    delay(1000);
  }
  //return;
  // show updates in the update box
  for (uint16_t r = 0; r < 4; r++)
  {
    display.setRotation(r);
    display.setPartialWindow(box_x, box_y, box_w, box_h);
    for (uint16_t i = 1; i <= 10; i += incr)
    {
      display.firstPage();
      do
      {
        display.fillRect(box_x, box_y, box_w, box_h, GxEPD_WHITE);
        display.setCursor(box_x, cursor_y);
        display.print(value * i, 2);
      }
      while (display.nextPage());
      delay(500);
    }
    delay(1000);
    display.firstPage();
    do
    {
      display.fillRect(box_x, box_y, box_w, box_h, GxEPD_WHITE);
    }
    while (display.nextPage());
    delay(1000);
  }
}

void drawGrid()
{
  uint16_t x, y;
  display.firstPage();
  do
  {
    x = 0;
    do
    {
      display.drawLine(x, 0, x, display.height() - 1, GxEPD_BLACK);
      x += 10;
    }
    while (x < display.width());
    y = 0;
    do
    {
      display.drawLine(0, y, display.width() - 1, y, GxEPD_BLACK);
      y += 10;
    }
    while (y < display.height());
    x = 0;
    do
    {
      display.fillCircle(x, display.height() / 2, 3, GxEPD_BLACK);
      x += 50;
    }
    while (x <= display.width());
    y = 0;
    do
    {
      display.fillCircle(display.width() / 2, y, 3, GxEPD_BLACK);
      y += 50;
    }
    while (y <= display.height());
  }
  while (display.nextPage());
}

void drawBitmaps()
{
  display.setRotation(0);
  display.setFullWindow();
#ifdef _GxBitmaps80x128_H_
  drawBitmaps80x128();
#endif
#ifdef _GxBitmaps152x152_H_
  drawBitmaps152x152();
#endif
#ifdef _GxBitmaps104x212_H_
  drawBitmaps104x212();
#endif
#ifdef _GxBitmaps200x200_H_
  drawBitmaps200x200();
#endif
#ifdef _GxBitmaps128x250_H_
  drawBitmaps128x250();
#endif
#ifdef _GxBitmaps128x296_H_
  drawBitmaps128x296();
#endif
#ifdef _GxBitmaps168x384_H_
  drawBitmaps168x384();
#endif
#ifdef _GxBitmaps152x296_H_
  drawBitmaps152x296();
#endif
#ifdef _GxBitmaps240x320_H_
  drawBitmaps240x320();
#endif
#ifdef _GxBitmaps176x264_H_
  drawBitmaps176x264();
#endif
#ifdef _GxBitmaps240x416_H_
  drawBitmaps240x416();
#endif
#ifdef _GxBitmaps400x300_H_
  drawBitmaps400x300();
#endif
#ifdef _GxBitmaps640x384_H_
  drawBitmaps640x384();
#endif
#ifdef _GxBitmaps648x480_H_
  drawBitmaps648x480();
#endif
#if defined(ESP32) && defined(_GxBitmaps800x276_H_)
  drawBitmaps800x276();
#endif
#ifdef _GxBitmaps800x480_H_
  drawBitmaps800x480();
#endif
#ifdef _WS_Bitmaps800x600_H_
  drawBitmaps800x600();
#endif
#if defined(ESP32) && defined(_GxBitmaps960x640_H_)
  drawBitmaps960x640();
#endif
#if defined(ESP32) && defined(_GxBitmaps960x680_H_)
  drawBitmaps960x680();
#endif
#if defined(ESP32) && defined(_GxBitmaps1304x984_H_)
  drawBitmaps1304x984();
#endif
  // 3-color
#ifdef _GxBitmaps3c104x212_H_
  drawBitmaps3c104x212();
#endif
#ifdef _GxBitmaps3c200x200_H_
  drawBitmaps3c200x200();
#endif
#ifdef _GxBitmaps3c128x250_H_
  drawBitmaps3c128x250();
#endif
#ifdef _GxBitmaps3c128x296_H_
  drawBitmaps3c128x296();
#endif
#ifdef _GxBitmaps3c152x296_H_
  drawBitmaps3c152x296();
#endif
#ifdef _GxBitmaps3c176x264_H_
  drawBitmaps3c176x264();
#endif
#ifdef _GxBitmaps3c400x300_H_
  drawBitmaps3c400x300();
#endif
#ifdef _GxBitmaps3c648x480_H_
  drawBitmaps3c648x480();
#endif
#ifdef _GxBitmaps3c800x480_H_
  drawBitmaps3c800x480();
#endif
#ifdef _GxBitmaps3c880x528_H_
  drawBitmaps3c880x528();
#endif
#if defined(ESP32) && defined(_GxBitmaps3c960x640_H_)
  drawBitmaps3c960x640();
#endif
#if defined(ESP32) && defined(_GxBitmaps3c960x680_H_)
  drawBitmaps3c960x680();
#endif
#if defined(ESP32) && defined(_GxBitmaps1360x480_H_)
  drawBitmaps1360x480();
#endif
  // 3-color
#if defined(ESP32) && defined(_GxBitmaps3c800x276_H_)
  drawBitmaps3c800x276();
#endif
  // 4-color
#if defined(_GxBitmaps4c128x250_H_)
  drawBitmaps4c128x250();
#endif
#if defined(_WS_Bitmaps4c168x168_H_)
  drawBitmaps4c168x168();
#endif
#if defined(_GxBitmaps4c168x384_H_)
  drawBitmaps4c168x384();
#endif
#if defined(_GxBitmaps4c184x360_H_)
  drawBitmaps4c184x360();
#endif
#if defined(_GxBitmaps4c400x300_H_)
  drawBitmaps4c400x300();
#endif
#if defined(_GxBitmaps4c960x640_H_)
  drawBitmaps4c960x640();
#endif
#if defined(ESP32) && defined(_GxBitmaps4c792x272_H_)
  drawBitmaps4c792x272();
#endif
  // 7-color
#if defined(_GxBitmaps7c600x448_H_)
  drawBitmaps7c600x448();
#endif
#if defined(_WS_Bitmaps7c192x143_H_)
  drawBitmaps7c192x143();
#endif
#if defined(_GxBitmaps7c800x480_H_)
  drawBitmaps7c800x480();
#endif
#if defined(_WS_Bitmaps7c300x180_H_)
  drawBitmaps7c300x180();
#endif
#if defined(ESP32) && defined(_GxBitmaps3c1304x984_H_)
  drawBitmaps3c1304x984();
#endif
}

#ifdef _GxBitmaps80x128_H_
void drawBitmaps80x128()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap80x128_1, Bitmap80x128_2, Bitmap80x128_3, Bitmap80x128_4, Bitmap80x128_5
  };
#else
  const unsigned char* bitmaps[] =
  {
    Bitmap80x128_1, Bitmap80x128_2, Bitmap80x128_3, Bitmap80x128_4, Bitmap80x128_5
  };
#endif
  if ((display.epd2.WIDTH == 80) && (display.epd2.HEIGHT == 128))
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], 80, 128, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
    display.firstPage();
    do
    {
      display.fillScreen(GxEPD_WHITE);
      display.drawBitmap(0, 0, WS_Bitmap80x128, 80, 128, GxEPD_BLACK);
    }
    while (display.nextPage());
    delay(2000);
  }
}
#endif

#ifdef _GxBitmaps152x152_H_
void drawBitmaps152x152()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap152x152_1, Bitmap152x152_2, Bitmap152x152_3
  };
#else
  const unsigned char* bitmaps[] =
  {
    Bitmap152x152_1, Bitmap152x152_2, Bitmap152x152_3
  };
#endif
  if ((display.epd2.WIDTH == 152) && (display.epd2.HEIGHT == 152))
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], 152, 152, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps200x200_H_
void drawBitmaps200x200()
{
#if defined(ARDUINO_AVR_PRO)
  const unsigned char* bitmaps[] =
  {
    logo200x200
  };
#elif defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    logo200x200, //first200x200
  };
#elif defined(_BOARD_GENERIC_STM32F103C_H_) || defined(STM32F1xx)
  const unsigned char* bitmaps[] =
  {
    logo200x200, first200x200, second200x200, third200x200, //fourth200x200, fifth200x200, sixth200x200, senventh200x200, eighth200x200
  };
#else
  const unsigned char* bitmaps[] =
  {
    logo200x200, first200x200, second200x200, third200x200, fourth200x200, fifth200x200, sixth200x200, senventh200x200, eighth200x200
    //logo200x200, first200x200, second200x200, fourth200x200, third200x200, fifth200x200, sixth200x200, senventh200x200, eighth200x200 // ED037TC1 test
  };
#endif
  if (display.epd2.hasColor) return; // to avoid many long refreshes
  if ((display.epd2.WIDTH == 200) && (display.epd2.HEIGHT == 200) && !display.epd2.hasColor)
  {
    bool m = display.mirror(true);
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], 200, 200, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
    display.mirror(m);
  }
  //else
  {
    bool mirror_y = (display.epd2.panel != GxEPD2::GDE0213B1);
    display.clearScreen(); // use default for white
    int16_t x = (int16_t(display.epd2.WIDTH) - 200) / 2;
    int16_t y = (int16_t(display.epd2.HEIGHT) - 200) / 2;
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.drawImage(bitmaps[i], x, y, 200, 200, false, mirror_y, true);
      delay(2000);
    }
  }
  bool mirror_y = (display.epd2.panel != GxEPD2::GDE0213B1);
  for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
  {
    int16_t x = -60;
    int16_t y = -60;
    for (uint16_t j = 0; j < 10; j++)
    {
      display.writeScreenBuffer(); // use default for white
      display.writeImage(bitmaps[i], x, y, 200, 200, false, mirror_y, true);
      display.refresh(true);
      if (display.epd2.hasFastPartialUpdate)
      {
        // for differential update: set previous buffer equal to current buffer in controller
        display.epd2.writeScreenBufferAgain(); // use default for white
        display.epd2.writeImageAgain(bitmaps[i], x, y, 200, 200, false, mirror_y, true);
      }
      delay(2000);
      x += display.epd2.WIDTH / 4;
      y += display.epd2.HEIGHT / 4;
      if ((x >= int16_t(display.epd2.WIDTH)) || (y >= int16_t(display.epd2.HEIGHT))) break;
    }
    if (!display.epd2.hasFastPartialUpdate) break; // comment out for full show
    break; // comment out for full show
  }
  display.writeScreenBuffer(); // use default for white
  display.writeImage(bitmaps[0], int16_t(0), 0, 200, 200, false, mirror_y, true);
  display.writeImage(bitmaps[0], int16_t(int16_t(display.epd2.WIDTH) - 200), int16_t(display.epd2.HEIGHT) - 200, 200, 200, false, mirror_y, true);
  display.refresh(true);
  // for differential update: set previous buffer equal to current buffer in controller
  display.epd2.writeScreenBufferAgain(); // use default for white
  display.epd2.writeImageAgain(bitmaps[0], int16_t(0), 0, 200, 200, false, mirror_y, true);
  display.epd2.writeImageAgain(bitmaps[0], int16_t(int16_t(display.epd2.WIDTH) - 200), int16_t(display.epd2.HEIGHT) - 200, 200, 200, false, mirror_y, true);
  delay(2000);
}
#endif

#ifdef _GxBitmaps104x212_H_
void drawBitmaps104x212()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    WS_Bitmap104x212, Bitmap104x212_1, Bitmap104x212_2, Bitmap104x212_3
  };
#else
  const unsigned char* bitmaps[] =
  {
    WS_Bitmap104x212, Bitmap104x212_1, Bitmap104x212_2, Bitmap104x212_3
  };
#endif
  if ((display.epd2.WIDTH == 104) && (display.epd2.HEIGHT == 212) && !display.epd2.hasColor)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, bitmaps[i], 104, 212, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps128x250_H_
void drawBitmaps128x250()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap128x250_1, logo128x250, first128x250, second128x250, third128x250
  };
#else
  const unsigned char* bitmaps[] =
  {
    Bitmap128x250_1, logo128x250, first128x250, second128x250, third128x250
  };
#endif
  if ((display.epd2.WIDTH == 128) && (display.epd2.HEIGHT == 250) && !display.epd2.hasColor)
  {
    bool m = display.mirror(true);
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], 128, 250, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
    display.mirror(m);
  }
}
#endif

#ifdef _GxBitmaps128x296_H_
void drawBitmaps128x296()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap128x296_1, logo128x296, first128x296, second128x296, third128x296
  };
#else
  const unsigned char* bitmaps[] =
  {
    Bitmap128x296_1, logo128x296 //, first128x296, second128x296, third128x296
  };
#endif
  if ((display.epd2.WIDTH == 128) && (display.epd2.HEIGHT == 296) && !display.epd2.hasColor)
  {
    bool m = display.mirror(true);
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], 128, 296, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
    display.mirror(m);
  }
}
#endif

#ifdef _GxBitmaps168x384_H_
void drawBitmaps168x384()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap168x384_1, Bitmap168x384_2, Bitmap168x384_3, Bitmap168x384_4, Bitmap168x384_5
  };
#else
  const unsigned char* bitmaps[] =
  {
    Bitmap168x384_1
  };
#endif
  if ((display.epd2.WIDTH == 168) && (display.epd2.HEIGHT == 384) && !display.epd2.hasColor)
  {
    bool mirrored = display.mirror(true);
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], 168, 384, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
    display.mirror(mirrored);
  }
}
#endif

#ifdef _GxBitmaps152x296_H_
void drawBitmaps152x296()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap152x296_1, Bitmap152x296_2, Bitmap152x296_3
  };
#else
  const unsigned char* bitmaps[] =
  {
    Bitmap152x296_1, Bitmap152x296_2, Bitmap152x296_3
  };
#endif
  if ((display.epd2.WIDTH == 152) && (display.epd2.HEIGHT == 296) && !display.epd2.hasColor)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], 152, 296, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps240x320_H_
void drawBitmaps240x320()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap240x320_1, Bitmap240x320_2, Bitmap240x320_3, Bitmap240x320_4, Bitmap240x320_5
  };
#else
  const unsigned char* bitmaps[] =
  {
    Bitmap240x320_1, Bitmap240x320_2
  };
#endif
  if ((display.epd2.WIDTH == 240) && (display.epd2.HEIGHT == 320) && !display.epd2.hasColor)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], 240, 320, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps176x264_H_
void drawBitmaps176x264()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap176x264_1, Bitmap176x264_2, Bitmap176x264_3, Bitmap176x264_4, Bitmap176x264_5
  };
#else
  const unsigned char* bitmaps[] =
  {
    Bitmap176x264_1, Bitmap176x264_2 //, Bitmap176x264_3, Bitmap176x264_4, Bitmap176x264_5
  };
#endif
  if ((display.epd2.WIDTH == 176) && (display.epd2.HEIGHT == 264) && !display.epd2.hasColor)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], 176, 264, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps240x416_H_
void drawBitmaps240x416()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap240x416_1, Bitmap240x416_2, Bitmap240x416_3
  };
#else
  const unsigned char* bitmaps[] =
  {
    Bitmap240x460_1, Bitmap240x460_2, Bitmap240x460_3
  };
#endif
  if ((display.epd2.WIDTH == 240) && (display.epd2.HEIGHT == 416) && !display.epd2.hasColor)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], 240, 416, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps400x300_H_
void drawBitmaps400x300()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap400x300_1, Bitmap400x300_2
  };
#else
  const unsigned char* bitmaps[] = {}; // not enough code space
#endif
  if ((display.epd2.WIDTH == 400) && (display.epd2.HEIGHT == 300) && !display.epd2.hasColor)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], 400, 300, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps640x384_H_
void drawBitmaps640x384()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap640x384_1, Bitmap640x384_2
  };
#else
  const unsigned char* bitmaps[] = {}; // not enough code space
#endif
  if ((display.epd2.WIDTH == 640) && (display.epd2.HEIGHT == 384))
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], 640, 384, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps648x480_H_
void drawBitmaps648x480()
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap648x480_1, Bitmap648x480_2, Bitmap648x480_3
  };
#else
  const unsigned char* bitmaps[] = {}; // not enough code space
#endif
  if ((display.epd2.WIDTH == 648) && (display.epd2.HEIGHT == 480) && !display.epd2.hasColor)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, bitmaps[i], 648, 480, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#if defined(ESP32) && defined(_GxBitmaps800x276_H_)
void drawBitmaps800x276()
{
  if ((display.epd2.WIDTH == 792) && (display.epd2.HEIGHT == 272) && !display.epd2.hasColor)
  {
    display.drawImage(Bitmap800x276_1, 0, 0, 800, 272, true, false, true); delay(2000);
    display.drawImage(Bitmap800x276_2, 0, 0, 800, 272, true, false, true); delay(2000);
    display.drawImage(Bitmap800x276_3, 0, 0, 800, 272, true, false, true); delay(2000);
    display.clearScreen();
    int16_t wp = display.epd2.WIDTH / 5;
    int16_t hp = display.epd2.HEIGHT / 5;
    for (int16_t i = 0; i < 5; i++)
    {
      for (int16_t j = 0; j < 5; j++)
      {
        display.writeImagePart(Bitmap800x276_1, i * wp, j * hp, 800, 272, i * wp, j * hp, wp, hp, true, false, true);
        display.refresh(true);
        display.epd2.writeImagePartAgain(Bitmap800x276_1, i * wp, j * hp, 800, 272, i * wp, j * hp, wp, hp, true, false, true);
        delay(500);
      }
    }
  }
}
#endif

#ifdef _GxBitmaps800x480_H_
void drawBitmaps800x480()
{
#if defined(ARDUINO_UNOR4_MINIMA) || defined(ARDUINO_UNOR4_WIFI)
  const unsigned char* bitmaps[] =
  {
    Bitmap800x480_3, Bitmap800x480_4
  };
#elif !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap800x480_1, Bitmap800x480_2, Bitmap800x480_3, Bitmap800x480_4
  };
#else
  const unsigned char* bitmaps[] = {}; // not enough code space
#endif
  if ((display.epd2.WIDTH == 800) && (display.epd2.HEIGHT == 480) && !display.epd2.hasColor)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, bitmaps[i], 800, 480, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
    if ((display.epd2.panel == GxEPD2::GDEW075T7) || (display.epd2.panel == GxEPD2::GDEY075T7))
    {
      // avoid ghosting caused by OTP waveform
      display.clearScreen();
      display.refresh(false); // full update
    }
  }
}
#endif

#if defined(ESP32) && defined(_GxBitmaps960x640_H_)
void drawBitmaps960x640()
{
  if ((display.epd2.WIDTH == 960) && (display.epd2.HEIGHT == 640) && !display.epd2.hasColor)
  {
    display.drawImage(Bitmap960x640_1, 0, 0, 960, 640, false, true, true); delay(5000);
    display.drawImage(Bitmap960x640_2, 0, 0, 960, 640, false, true, true); delay(5000);
    display.drawImage(Bitmap960x640_3, 0, 0, 960, 640, false, true, true); delay(5000);
  }
}
#endif

#if defined(ESP32) && defined(_GxBitmaps960x680_H_)
void drawBitmaps960x680()
{
  if ((display.epd2.WIDTH == 960) && (display.epd2.HEIGHT == 680) && !display.epd2.hasColor)
  {
    display.drawImage(Bitmap960x680_1, 0, 0, 960, 680, false, true, true); delay(5000);
    display.drawImage(Bitmap960x680_2, 0, 0, 960, 680, false, true, true); delay(5000);
  }
}
#endif

#ifdef _WS_Bitmaps800x600_H_
void drawBitmaps800x600()
{
#if defined(ESP8266) || defined(ESP32)
  if ((display.epd2.panel == GxEPD2::ED060SCT) || (display.epd2.panel == GxEPD2::ED060KC1) || (display.epd2.panel == GxEPD2::ED078KC2))
  {
    //    Serial.print("sizeof(WS_zoo_800x600) is "); Serial.println(sizeof(WS_zoo_800x600));
    display.drawNative(WS_zoo_800x600, 0, 0, 0, 800, 600, false, false, true);
    delay(2000);
    //    Serial.print("sizeof(WS_pic_1200x825) is "); Serial.println(sizeof(WS_pic_1200x825));
    //    display.drawNative((const uint8_t*)WS_pic_1200x825, 0, 0, 0, 1200, 825, false, false, true);
    //    delay(2000);
    //    Serial.print("sizeof(WS_acaa_1024x731) is "); Serial.println(sizeof(WS_acaa_1024x731));
    //    display.drawNative(WS_acaa_1024x731, 0, 0, 0, 1024, 731, false, false, true);
    //    delay(2000);
  }
#endif
}
#endif

#if defined(ESP32) && defined(_GxBitmaps1304x984_H_)
void drawBitmaps1304x984()
{
  if (display.epd2.panel == GxEPD2::GDEW1248T3)
  {
    display.drawImage(Bitmap1304x984, 0, 0, display.epd2.WIDTH, display.epd2.HEIGHT, false, false, true);
  }
}
#endif

#if defined(ESP32) && defined(_GxBitmaps1360x480_H_)
void drawBitmaps1360x480()
{
  if (display.epd2.panel == GxEPD2::GDEM1085T51)
  {
    display.drawImage(Bitmap1360x480_1, 0, 0, display.epd2.WIDTH, display.epd2.HEIGHT, false, false, true);
    delay(2000);
    display.drawImage(Bitmap1360x480_p1, 0, 0, display.epd2.WIDTH, display.epd2.HEIGHT, false, false, true);
    delay(1000);
    display.drawImage(Bitmap1360x480_p2, 0, 0, display.epd2.WIDTH, display.epd2.HEIGHT, false, false, true);
    delay(1000);
    display.drawImage(Bitmap1360x480_p3, 0, 0, display.epd2.WIDTH, display.epd2.HEIGHT, false, false, true);
    delay(1000);
    display.clearScreen();
    int16_t wp = display.epd2.WIDTH / 5;
    int16_t hp = display.epd2.HEIGHT / 5;
    for (int16_t i = 0; i < 5; i++)
    {
      for (int16_t j = 0; j < 5; j++)
      {
        display.writeImagePart(Bitmap1360x480_1, i * wp, j * hp, 1360, 480, i * wp, j * hp, wp, hp, false, false, true);
        display.refresh(true);
        delay(500);
      }
    }
  }
}
#endif

struct bitmap_pair
{
  const unsigned char* black;
  const unsigned char* red;
};

#ifdef _GxBitmaps3c200x200_H_
void drawBitmaps3c200x200()
{
  bitmap_pair bitmap_pairs[] =
  {
    //{Bitmap3c200x200_black, Bitmap3c200x200_red},
    {WS_Bitmap3c200x200_black, WS_Bitmap3c200x200_red}
  };
  if (display.epd2.panel == GxEPD2::GDEW0154Z04)
  {
    display.firstPage();
    do
    {
      display.fillScreen(GxEPD_WHITE);
      // Bitmap3c200x200_black has 2 bits per pixel
      // taken from Adafruit_GFX.cpp, modified
      int16_t byteWidth = (display.epd2.WIDTH + 7) / 8; // Bitmap scanline pad = whole byte
      uint8_t byte = 0;
      for (int16_t j = 0; j < display.epd2.HEIGHT; j++)
      {
        for (int16_t i = 0; i < display.epd2.WIDTH; i++)
        {
          if (i & 3) byte <<= 2;
          else
          {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
            byte = pgm_read_byte(&Bitmap3c200x200_black[j * byteWidth * 2 + i / 4]);
#else
            byte = Bitmap3c200x200_black[j * byteWidth * 2 + i / 4];
#endif
          }
          if (!(byte & 0x80))
          {
            display.drawPixel(i, j, GxEPD_BLACK);
          }
        }
      }
      display.drawInvertedBitmap(0, 0, Bitmap3c200x200_red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
    }
    while (display.nextPage());
    delay(2000);
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmap_pairs[i].black, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
        display.drawInvertedBitmap(0, 0, bitmap_pairs[i].red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
  if (display.epd2.hasColor)
  {
    display.clearScreen(); // use default for white
    int16_t x = (int16_t(display.epd2.WIDTH) - 200) / 2;
    int16_t y = (int16_t(display.epd2.HEIGHT) - 200) / 2;
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.drawImage(bitmap_pairs[i].black, bitmap_pairs[i].red, x, y, 200, 200, false, false, true);
      delay(2000);
    }
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      int16_t x = -60;
      int16_t y = -60;
      for (uint16_t j = 0; j < 10; j++)
      {
        display.writeScreenBuffer(); // use default for white
        display.writeImage(bitmap_pairs[i].black, bitmap_pairs[i].red, x, y, 200, 200, false, false, true);
        display.refresh();
        delay(1000);
        x += display.epd2.WIDTH / 4;
        y += display.epd2.HEIGHT / 4;
        if ((x >= int16_t(display.epd2.WIDTH)) || (y >= int16_t(display.epd2.HEIGHT))) break;
      }
    }
    display.writeScreenBuffer(); // use default for white
    display.writeImage(bitmap_pairs[0].black, bitmap_pairs[0].red, 0, 0, 200, 200, false, false, true);
    display.writeImage(bitmap_pairs[0].black, bitmap_pairs[0].red, int16_t(display.epd2.WIDTH) - 200, int16_t(display.epd2.HEIGHT) - 200, 200, 200, false, false, true);
    display.refresh();
    delay(2000);
  }
}
#endif

#ifdef _GxBitmaps3c104x212_H_
void drawBitmaps3c104x212()
{
#if !defined(__AVR)
  bitmap_pair bitmap_pairs[] =
  {
    {Bitmap3c104x212_1_black, Bitmap3c104x212_1_red},
    {Bitmap3c104x212_2_black, Bitmap3c104x212_2_red},
    {WS_Bitmap3c104x212_black, WS_Bitmap3c104x212_red}
  };
#else
  bitmap_pair bitmap_pairs[] =
  {
    {Bitmap3c104x212_1_black, Bitmap3c104x212_1_red},
    //{Bitmap3c104x212_2_black, Bitmap3c104x212_2_red},
    {WS_Bitmap3c104x212_black, WS_Bitmap3c104x212_red}
  };
#endif
  if (display.epd2.panel == GxEPD2::GDEW0213Z16)
  {
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmap_pairs[i].black, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
        if (bitmap_pairs[i].red == WS_Bitmap3c104x212_red)
        {
          display.drawInvertedBitmap(0, 0, bitmap_pairs[i].red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
        }
        else display.drawBitmap(0, 0, bitmap_pairs[i].red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps3c128x250_H_
void drawBitmaps3c128x250()
{
  if ((display.epd2.WIDTH == 128) && (display.epd2.HEIGHT == 250) && display.epd2.hasColor)
  {
    bool mirrored = display.mirror(true);
    display.firstPage();
    do
    {
      display.fillScreen(GxEPD_WHITE);
      display.drawInvertedBitmap(0, 0, Bitmap3c128x250_1_black, 128, 250, GxEPD_BLACK);
      display.drawInvertedBitmap(0, 0, Bitmap3c128x250_1_red, 128, 250, GxEPD_RED);
    }
    while (display.nextPage());
    delay(2000);
#if !defined(__AVR)
    display.firstPage();
    do
    {
      display.fillScreen(GxEPD_WHITE);
      display.drawInvertedBitmap(0, 0, Bitmap3c128x250_2_black, 128, 250, GxEPD_BLACK);
      display.drawBitmap(0, 0, Bitmap3c128x250_2_red, 128, 250, GxEPD_RED);
    }
    while (display.nextPage());
    delay(2000);
#endif
    display.mirror(mirrored);
  }
}
#endif

#ifdef _GxBitmaps3c128x296_H_
void drawBitmaps3c128x296()
{
#if !defined(__AVR)
  bitmap_pair bitmap_pairs[] =
  {
    {Bitmap3c128x296_1_black, Bitmap3c128x296_1_red},
    {Bitmap3c128x296_2_black, Bitmap3c128x296_2_red},
    {WS_Bitmap3c128x296_black, WS_Bitmap3c128x296_red}
  };
#else
  bitmap_pair bitmap_pairs[] =
  {
    //{Bitmap3c128x296_1_black, Bitmap3c128x296_1_red},
    //{Bitmap3c128x296_2_black, Bitmap3c128x296_2_red},
    {WS_Bitmap3c128x296_black, WS_Bitmap3c128x296_red}
  };
#endif
  if ((display.epd2.WIDTH == 128) && (display.epd2.HEIGHT == 296) && display.epd2.hasColor)
  {
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmap_pairs[i].black, 128, 296, GxEPD_BLACK);
        if (bitmap_pairs[i].red == WS_Bitmap3c128x296_red)
        {
          display.drawInvertedBitmap(0, 0, bitmap_pairs[i].red, 128, 296, GxEPD_RED);
        }
        else display.drawBitmap(0, 0, bitmap_pairs[i].red, 128, 296, GxEPD_RED);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps3c152x296_H_
void drawBitmaps3c152x296()
{
  bitmap_pair bitmap_pairs[] =
  {
    {Bitmap3c152x296_black, Bitmap3c152x296_red}
  };
  if (display.epd2.panel == GxEPD2::GDEY0266Z90)
  {
    bool mirrored = display.mirror(true);
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, bitmap_pairs[i].black, 152, 296, GxEPD_BLACK);
        display.drawInvertedBitmap(0, 0, bitmap_pairs[i].red, 152, 296, GxEPD_RED);
      }
      while (display.nextPage());
      delay(2000);
    }
    display.mirror(mirrored);
  }
}
#endif

#ifdef _GxBitmaps3c176x264_H_
void drawBitmaps3c176x264()
{
  bitmap_pair bitmap_pairs[] =
  {
    {Bitmap3c176x264_black, Bitmap3c176x264_red}
  };
  if (display.epd2.panel == GxEPD2::GDEW027C44)
  {
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, bitmap_pairs[i].black, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
        display.drawBitmap(0, 0, bitmap_pairs[i].red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps3c400x300_H_
void drawBitmaps3c400x300()
{
#if !defined(__AVR)
  bitmap_pair bitmap_pairs[] =
  {
    {Bitmap3c400x300_1_black, Bitmap3c400x300_1_red},
    {Bitmap3c400x300_2_black, Bitmap3c400x300_2_red},
    {WS_Bitmap3c400x300_black, WS_Bitmap3c400x300_red},
    {Bitmap3c400x300_3_black, Bitmap3c400x300_3_red},
    {Bitmap3c400x300_4_black, Bitmap3c400x300_4_red},
  };
#else
  bitmap_pair bitmap_pairs[] = {}; // not enough code space
#endif
  if ((display.epd2.WIDTH == 400) && (display.epd2.HEIGHT == 300) && display.epd2.hasColor)
  {
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmap_pairs[i].black, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
        display.drawInvertedBitmap(0, 0, bitmap_pairs[i].red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps3c648x480_H_
void drawBitmaps3c648x480()
{
#if !defined(__AVR)
  bitmap_pair bitmap_pairs[] =
  {
    {Bitmap3c648x480_black, Bitmap3c648x480_red},
    {Bitmap3c648x480_black_2, Bitmap3c648x480_red_2}
  };
#else
  bitmap_pair bitmap_pairs[] = {}; // not enough code space
#endif
  if ((display.epd2.panel == GxEPD2::GDEW0583Z83) || (display.epd2.panel == GxEPD2::GDEQ0583Z31))
  {
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, bitmap_pairs[i].black, 648, 480, GxEPD_BLACK);
        display.drawBitmap(0, 0, bitmap_pairs[i].red, 648, 480, GxEPD_RED);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#if defined(ESP32) && defined(_GxBitmaps3c800x276_H_)
void drawBitmaps3c800x276()
{
  if ((display.epd2.WIDTH == 792) && (display.epd2.HEIGHT == 272) && display.epd2.hasColor)
  {
    display.drawImage(Bitmap3c800x276_black_1, Bitmap3c800x276_red_1, 0, 0, 800, 272, true, false, true); delay(2000);
    display.drawImage(Bitmap3c800x276_black_2, Bitmap3c800x276_red_2, 0, 0, 800, 272, true, false, true); delay(2000);
    int16_t wp = display.epd2.WIDTH / 5;
    int16_t hp = display.epd2.HEIGHT / 5;
    int16_t n = 0;
    for (int16_t k = 0; k < 3; k++)
    {
      display.writeScreenBuffer();
      for (int16_t i = 0; i < 5; i++)
      {
        for (int16_t j = 0; j < 5; j++)
        {
          if ((n++ % 2) || (k == 2))
          {
            display.writeImagePart(Bitmap3c800x276_black_1, Bitmap3c800x276_red_1, i * wp, j * hp, 800, 272, i * wp, j * hp, wp, hp, true, false, true);
          }
        }
      }
      display.refresh(false);
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps3c800x480_H_
void drawBitmaps3c800x480()
{
#if !defined(__AVR)
  bitmap_pair bitmap_pairs[] =
  {
    {Bitmap3c800x480_1_black, Bitmap3c800x480_1_red}
  };
#else
  bitmap_pair bitmap_pairs[] = {}; // not enough code space
#endif
  if (display.epd2.panel == GxEPD2::GDEW075Z08)
  {
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawBitmap(0, 0, bitmap_pairs[i].black, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
        display.drawBitmap(0, 0, bitmap_pairs[i].red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps3c880x528_H_
void drawBitmaps3c880x528()
{
#if !defined(__AVR)
  bitmap_pair bitmap_pairs[] =
  {
    {Bitmap3c880x528_black, Bitmap3c880x528_red}
  };
#else
  bitmap_pair bitmap_pairs[] = {}; // not enough code space
#endif
  if (display.epd2.panel == GxEPD2::GDEH075Z90)
  {
    bool m = display.mirror(true);
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmap_pairs[i].black, 880, 528, GxEPD_BLACK);
        display.drawInvertedBitmap(0, 0, bitmap_pairs[i].red, 880, 528, GxEPD_RED);
      }
      while (display.nextPage());
      delay(2000);
    }
    display.mirror(m);
  }
}
#endif

#if defined(ESP32) && defined(_GxBitmaps3c960x640_H_)
void drawBitmaps3c960x640()
{
  if ((display.epd2.WIDTH == 960) && (display.epd2.HEIGHT == 640) && display.epd2.hasColor)
  {
    display.drawImage(Bitmap3c960x640_black, Bitmap3c960x640_red, 0, 0, 960, 640, false, false, true);
    delay(2000);
  }
}
#endif

#if defined(ESP32) && defined(_GxBitmaps3c960x680_H_)
void drawBitmaps3c960x680()
{
  if ((display.epd2.WIDTH == 960) && (display.epd2.HEIGHT == 680) && display.epd2.hasColor)
  {
    display.drawImage(Bitmap3c960x680_black, Bitmap3c960x680_red, 0, 0, 960, 680, false, true, true);
  }
  delay(2000);
}
#endif

#if defined(ESP32) && defined(_GxBitmaps3c1304x984_H_)
void drawBitmaps3c1304x984()
{
  if (display.epd2.panel == GxEPD2::GDEY1248Z51)
  {
    //display.drawImage(Bitmap3c1304x984_black, Bitmap3c1304x984_red, 0, 0, 1304, 984, false, false, true);
    display.writeImage(0, Bitmap3c1304x984_red, 0, 0, 1304, 984, true, false, true); // red bitmap is inverted
    display.drawImage(Bitmap3c1304x984_black, 0, 0, 0, 1304, 984, true, false, true); // black bitmap is normal
  }
}
#endif

#if defined(_GxBitmaps4c128x250_H_)
void drawBitmaps4c128x250()
{
  if (display.epd2.panel == GxEPD2::GDEY0213F51)
  {
    display.drawNative(Bitmap4c128x250, 0, 0, 0, 128, 250, false, false, true);
    delay(5000);
  }
}
#endif

#if defined(_WS_Bitmaps4c168x168_H_)
void drawBitmaps4c168x168()
{
  if ((display.epd2.panel == GxEPD2::Waveshare437inch4color) || (display.epd2.panel == GxEPD2::Waveshare3inch4color) ||
      (display.epd2.panel == GxEPD2::GDEY0579F51) || (display.epd2.panel == GxEPD2::GDEY116F51))
  {
    display.drawNative(WS_Bitmap4c168x168, 0, (display.epd2.WIDTH - 168) / 2, (display.epd2.HEIGHT - 168) / 2, 168, 168, false, false, true);
    delay(5000);
  }
}
#endif

#if defined(_WS_Bitmaps4c168x400_H_)
void drawBitmaps4c168x400()
{
  if (display.epd2.panel == GxEPD2::Waveshare3inch4color)
  {
    display.drawNative(WS_Bitmap4c168x400, 0, (display.epd2.WIDTH - 168) / 2, (display.epd2.HEIGHT - 400) / 2, 168, 400, false, false, true);
    delay(5000);
  }
}
#endif

#if defined(_GxBitmaps4c168x384_H_)
void drawBitmaps4c168x384()
{
  if (display.epd2.panel == GxEPD2::GDEY029F51H)
  {
    display.drawNative(Bitmap4c168x384, 0, (display.epd2.WIDTH - 168) / 2, (display.epd2.HEIGHT - 384) / 2, 168, 384, false, false, true);
    delay(5000);
  }
}
#endif

#if defined(_GxBitmaps4c184x360_H_)
void drawBitmaps4c184x360()
{
  if (display.epd2.panel == GxEPD2::GDEY0266F51H)
  {
    display.drawNative(Bitmap4c184x360, 0, (display.epd2.WIDTH - 184) / 2, (display.epd2.HEIGHT - 360) / 2, 184, 360, false, false, true);
    delay(5000);
  }
}
#endif

#if defined(_GxBitmaps4c400x300_H_)
void drawBitmaps4c400x300()
{
  if (display.epd2.panel == GxEPD2::GDEY0420F51)
  {
    display.drawNative(Bitmap4c400x300, 0, (display.epd2.WIDTH - 400) / 2, (display.epd2.HEIGHT - 300) / 2, 400, 300, false, false, true);
    delay(5000);
  }
}
#endif

#if defined(_GxBitmaps4c960x640_H_)
void drawBitmaps4c960x640()
{
  if (display.epd2.panel == GxEPD2::GDEY116F51)
  {
    display.drawNative(Bitmap4c960x640, 0, (display.epd2.WIDTH - 960) / 2, (display.epd2.HEIGHT - 640) / 2, 960, 640, true, false, true);
    delay(5000);
  }
}
#endif

#if defined(ESP32) && defined(_GxBitmaps4c792x272_H_)
void drawBitmaps4c792x272()
{
  if (display.epd2.panel == GxEPD2::GDEY0579F51)
  {
    display.drawNative(Bitmap4c792x272, 0, (display.epd2.WIDTH - 792) / 2, (display.epd2.HEIGHT - 272) / 2, 792, 272, true, false, true);
    delay(5000);
#if defined(GxEPD2_DRIVER_CLASS) && defined(IS_GxEPD2_DRIVER_4C)
#if IS_GxEPD2_DRIVER_4C(GxEPD2_DRIVER_CLASS)
    int16_t wp = display.epd2.WIDTH / 5;
    int16_t hp = display.epd2.HEIGHT / 5;
    int16_t n = 0;
    for (int16_t k = 0; k < 3; k++)
    {
      display.writeScreenBuffer();
      for (int16_t i = 0; i < 5; i++)
      {
        for (int16_t j = 0; j < 5; j++)
        {
          if ((n++ % 2) || (k == 2))
          {
            display.epd2.writeNativePart(Bitmap4c792x272, 0, i * wp, j * hp, 792, 272, i * wp, j * hp, wp, hp, true, false, true);
          }
        }
      }
      display.refresh(false);
      delay(2000);
    }
#endif
#endif
  }
}
#endif

#if defined(_GxBitmaps7c600x448_H_)
void drawBitmaps7c600x448()
{
  if (display.epd2.panel == GxEPD2::GDEP0565D90)
  {
    display.epd2.drawDemoBitmap(Bitmap7c600x448, 0, 0, 0, 600, 448, 1, false, true);
    delay(5000);
  }
}
#endif

#if defined(_WS_Bitmaps7c192x143_H_)
void drawBitmaps7c192x143()
{
  if (display.epd2.panel == GxEPD2::ACeP565)
  {
    display.drawNative(WS_Bitmap7c192x143, 0, (display.epd2.WIDTH - 192) / 2, (display.epd2.HEIGHT - 143) / 2, 192, 143, false, false, true);
    delay(5000);
  }
}
#endif

#if defined(_GxBitmaps7c800x480_H_)
void drawBitmaps7c800x480()
{
  if ((display.epd2.panel == GxEPD2::GDEY073D46) || (display.epd2.panel == GxEPD2::ACeP730) || (display.epd2.panel == GxEPD2::GDEP0565D90) || (display.epd2.panel == GxEPD2::GDEP073E01))
  {
    display.epd2.drawDemoBitmap(Bitmap7c800x480, 0, 0, 0, 800, 480, 0, false, true); // special format
    delay(5000);
    display.epd2.drawDemoBitmap(Bitmap7c800x480_2, 0, 0, 0, 800, 480, 1, false, true); // special format mode 1
    delay(5000);
  }
}
#endif

#if defined(_WS_Bitmaps7c300x180_H_)
void drawBitmaps7c300x180()
{
  if ((display.epd2.panel == GxEPD2::GDEY073D46) || (display.epd2.panel == GxEPD2::ACeP730) || (display.epd2.panel == GxEPD2::GDEP0565D90))
  {
    display.drawNative(WS_Bitmap7c300x180, 0, (display.epd2.WIDTH - 300) / 2, (display.epd2.HEIGHT - 180) / 2, 300, 180, false, false, true);
    delay(5000);
  }
}
#endif

void draw7colors()
{
  display.setRotation(0);
  uint16_t h = display.height() / 7;
  display.firstPage();
  do
  {
    display.fillRect(0, 0, display.width(), h, GxEPD_BLACK);
    display.fillRect(0, h, display.width(), h, GxEPD_WHITE);
    display.fillRect(0, 2 * h, display.width(), h, GxEPD_GREEN);
    display.fillRect(0, 3 * h, display.width(), h, GxEPD_BLUE);
    display.fillRect(0, 4 * h, display.width(), h, GxEPD_RED);
    display.fillRect(0, 5 * h, display.width(), h, GxEPD_YELLOW);
    display.fillRect(0, 6 * h, display.width(), h, GxEPD_ORANGE);
  }
  while (display.nextPage());
}

void draw7colorlines()
{
  display.setRotation(0);
  uint16_t h = 2;
  display.firstPage();
  do
  {
    uint16_t y = 0;
    do
    {
      display.fillRect(0, y, display.width(), h, GxEPD_BLACK); y += h;
      display.fillRect(0, y, display.width(), h, GxEPD_WHITE); y += h;
      display.fillRect(0, y, display.width(), h, GxEPD_GREEN); y += h;
      display.fillRect(0, y, display.width(), h, GxEPD_WHITE); y += h;
      display.fillRect(0, y, display.width(), h, GxEPD_BLUE); y += h;
      display.fillRect(0, y, display.width(), h, GxEPD_WHITE); y += h;
      display.fillRect(0, y, display.width(), h, GxEPD_RED); y += h;
      display.fillRect(0, y, display.width(), h, GxEPD_WHITE); y += h;
      display.fillRect(0, y, display.width(), h, GxEPD_YELLOW); y += h;
      display.fillRect(0, y, display.width(), h, GxEPD_WHITE); y += h;
      display.fillRect(0, y, display.width(), h, GxEPD_ORANGE); y += h;
      display.fillRect(0, y, display.width(), h, GxEPD_WHITE); y += h;
    }
    while ((y + 12 * h) < uint16_t(display.height()));
    //display.drawPixel(0, y, GxEPD_BLACK); display.drawPixel(10, y, GxEPD_GREEN);
    //display.drawPixel(20, y, GxEPD_BLUE); display.drawPixel(30, y, GxEPD_RED);
    //display.drawPixel(40, y, GxEPD_YELLOW); display.drawPixel(50, y, GxEPD_ORANGE);
    display.fillRect(0, y, 2, 2, GxEPD_BLACK); display.fillRect(10, y, 2, 2, GxEPD_GREEN);
    display.fillRect(20, y, 2, 2, GxEPD_BLUE); display.fillRect(30, y, 2, 2, GxEPD_RED);
    display.fillRect(40, y, 2, 2, GxEPD_YELLOW); display.fillRect(50, y, 2, 2, GxEPD_ORANGE);
  }
  while (display.nextPage());
}

void drawGraphics()
{
  display.setRotation(0);
  display.firstPage();
  do
  {
    display.drawRect(display.width() / 8, display.height() / 8, display.width() * 3 / 4, display.height() * 3 / 4, GxEPD_BLACK);
    display.drawLine(display.width() / 8, display.height() / 8, display.width() * 7 / 8, display.height() * 7 / 8, GxEPD_BLACK);
    display.drawLine(display.width() / 8, display.height() * 7 / 8, display.width() * 7 / 8, display.height() / 8, GxEPD_BLACK);
    display.drawCircle(display.width() / 2, display.height() / 2, display.height() / 4, GxEPD_BLACK);
    display.drawPixel(display.width() / 4, display.height() / 2 , GxEPD_BLACK);
    display.drawPixel(display.width() * 3 / 4, display.height() / 2 , GxEPD_BLACK);
  }
  while (display.nextPage());
  delay(1000);
}
