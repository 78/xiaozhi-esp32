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

// experimental example GxEPD2x_MixedTest.ino: proof of concept for differential refresh on capable 3-color panels

// see also GxEPD2x_FastBlackWhiteOnColor.ino: differential refresh on capable 3-color panels

// see GxEPD2_wiring_examples.h for wiring suggestions and examples
// if you use a different wiring, you need to adapt the constructor parameters!

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
//#include <GFX.h>

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// e.g. for Wemos D1 mini:
#if defined (ESP8266)
GxEPD2_3C<GxEPD2_213_Z19c, GxEPD2_213_Z19c::HEIGHT> display(GxEPD2_213_Z19c(/*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4)); // GDEW0213Z19
//GxEPD2_3C<GxEPD2_290_Z13c, GxEPD2_290_Z13c::HEIGHT> display(GxEPD2_290_Z13c(/*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4)); // GDEH029Z13
#endif

#if defined(ESP32)
//GxEPD2_3C<GxEPD2_213_Z19c, GxEPD2_213_Z19c::HEIGHT> display(GxEPD2_213_Z19c(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEW0213Z19
//GxEPD2_3C<GxEPD2_290_Z13c, GxEPD2_290_Z13c::HEIGHT> display(GxEPD2_290_Z13c(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEH029Z13
#endif

const uint16_t window_x = 8;
const uint16_t window_y = 10;
const uint16_t window_width = 80;
const uint16_t window_height = 24;

// we need a canvas with correct explicit assignment operator
class MyCanvas1 : public GFXcanvas1
{
  private:
    uint32_t bytes;
  public:
    MyCanvas1(uint16_t w, uint16_t h) : GFXcanvas1(w, h), bytes(((w + 7) / 8) * h) {};
    MyCanvas1& operator=(const MyCanvas1& from)
    {
      if (bytes == from.bytes)
      {
        memcpy(getBuffer(), from.getBuffer(), bytes);
      } // else would need realloc, but buffer is private
      return *this;
    };
};

//GFXcanvas1 current_bw_window(window_width, window_height);
//GFXcanvas1 previous_bw_window(window_width, window_height);
MyCanvas1 current_bw_window(window_width, window_height);
MyCanvas1 previous_bw_window(window_width, window_height);

void setup()
{
  Serial.begin(115200);
  Serial.println();
  display.init(115200);
  helloWorld();
  if (current_bw_window.getBuffer() && previous_bw_window.getBuffer())
  {
    current_bw_window.fillScreen(GxEPD_WHITE);
    previous_bw_window.fillScreen(GxEPD_WHITE);
    // only needed if window is not yet white
    //display.drawImage(current_bw_window.getBuffer(), previous_bw_window.getBuffer(), window_x, window_y, window_width, window_height);
    for (uint16_t i = 0; i < 100; i++)
    {
      showValue(i);
    }
  }
  else
  {
    Serial.println("GxEPD2x_MixedTest: canvas buffer allocation failed!");
    Serial.print("current_bw_window  0x"); Serial.println(uint32_t(current_bw_window.getBuffer()), HEX);
    Serial.print("previous_bw_window 0x"); Serial.println(uint32_t(previous_bw_window.getBuffer()), HEX);
  }
  display.hibernate();
}

void showValue(uint16_t value)
{
  current_bw_window.setFont(&FreeMonoBold9pt7b);
  current_bw_window.setTextColor(GxEPD_BLACK);
  current_bw_window.fillScreen(GxEPD_WHITE);
  current_bw_window.setCursor(20, FreeMonoBold9pt7b.yAdvance);
  current_bw_window.print(value);
  display.writeImage(previous_bw_window.getBuffer(), current_bw_window.getBuffer(), window_x, window_y, window_width, window_height);
  display.epd2.refresh_bw(window_x, window_y, window_width, window_height);
  // this needs a canvas with correct explicit assignment operator
  previous_bw_window = current_bw_window;
  //Serial.print("current_bw_window  0x"); Serial.println(uint32_t(current_bw_window.getBuffer()), HEX);
  //Serial.print("previous_bw_window 0x"); Serial.println(uint32_t(previous_bw_window.getBuffer()), HEX);
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
