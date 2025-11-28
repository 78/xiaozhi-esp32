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

// experimental example GxEPD2x_FastBlackWhiteOnColor.ino: differential refresh on capable 3-color panels

// see GxEPD2_wiring_examples.h for wiring suggestions and examples
// if you use a different wiring, you need to adapt the constructor parameters!

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
//#include <GFX.h>

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// e.g. for Wemos D1 mini:
#if defined (ESP8266)
//GxEPD2_3C<GxEPD2_213_Z19c, GxEPD2_213_Z19c::HEIGHT> display(GxEPD2_213_Z19c(/*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4)); // GDEW0213Z19
//GxEPD2_3C<GxEPD2_290_Z13c, GxEPD2_290_Z13c::HEIGHT> display(GxEPD2_290_Z13c(/*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4)); // GDEH029Z13
//GxEPD2_3C<GxEPD2_420c_GDEY042Z98, GxEPD2_420c_GDEY042Z98::HEIGHT> display(GxEPD2_420c_GDEY042Z98(/*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4)); // GDEY042Z98
#endif

#if defined(ESP32)
//GxEPD2_3C<GxEPD2_213_Z19c, GxEPD2_213_Z19c::HEIGHT> display(GxEPD2_213_Z19c(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEW0213Z19
//GxEPD2_3C<GxEPD2_290_Z13c, GxEPD2_290_Z13c::HEIGHT> display(GxEPD2_290_Z13c(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEH029Z13
GxEPD2_3C<GxEPD2_420c_GDEY042Z98, GxEPD2_420c_GDEY042Z98::HEIGHT> display(GxEPD2_420c_GDEY042Z98(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEY042Z98
#endif

const uint16_t window_x = 8;
const uint16_t window_y = 10;
const uint16_t window_width = 80;
const uint16_t window_height = 24;

void setup()
{
  Serial.begin(115200);
  Serial.println();
  display.init(115200);
  helloWorld();
  if (display.epd2.panel == GxEPD2::GDEY042Z98)
  {
    // the controller SSD1683 of this panel requires old and new data to be equal outside of the partial window
    // else it would refresh also outside of the partial window
    display.epd2.writeScreenBuffer(0xFF, 0xFF); // both controller buffers set to white
    // note that the buffer content is lost by this
  }
  // make sure window is only b/w, before using fast b/w refreshes
  //clearWindow(); // commented out, partial window was cleared by helloWorld()
  for (uint16_t i = 0; i < 100; i++)
  {
    showValue(i);
  }
  display.hibernate();
}

// make sure window is only b/w, before using fast b/w refreshes
void clearWindow()
{
  display.setPartialWindow(window_x, window_y, window_width, window_height);
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
  }
  while (display.nextPage());
}

void showValue(uint16_t value)
{
  display.setRotation(0);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  if ((1 == display.pages()) && true) // test non-paged
  {
    display.setFullWindow();
    display.fillRect(window_x, window_y, window_width, window_height, GxEPD_WHITE);
    display.setCursor(window_x + 20, window_y + FreeMonoBold9pt7b.yAdvance);
    display.print(value);
    // non-paged fast bw partial update
    display.displayWindowBW(window_x, window_y, window_width, window_height);
  }
  else // test paged
  {
    display.setPartialWindow(window_x, window_y, window_width, window_height);
    display.firstPage();
    do
    {
      display.fillScreen(GxEPD_WHITE);
      display.setCursor(window_x + 20, window_y + FreeMonoBold9pt7b.yAdvance);
      display.print(value);
    }
    while (display.nextPageBW()); //paged fast bw partial update
  }
  delay(1000);
}

const char HelloWorld[] = "Hello World!";

void helloWorld()
{
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_RED);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
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
}

void loop() {};
