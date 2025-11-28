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
//
// GxEPD2_RotaryUsingBusyCallback.ino: an example showing use of busyCallback to handle time-critical services
// GxEPD2 uses busy wait on the BUSY line from EPDs by design, 
// to handle all combinations of paged display and differential refresh in a user friendly way.
// busyCallback can be used to overcome this restriction caused by busy wait.
// note: this feature is only available if the BUSY line is used, not if the default delay is used instead.

// Supporting Arduino Forum Topics (closed, read only):
// Good Display ePaper for Arduino: https://forum.arduino.cc/t/good-display-epaper-for-arduino/419657
// Waveshare e-paper displays with SPI: https://forum.arduino.cc/t/waveshare-e-paper-displays-with-spi/467865
//
// Add new topics in https://forum.arduino.cc/c/using-arduino/displays/23 for new questions and issues

// see GxEPD2_wiring_examples.h for wiring suggestions and examples

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

// select the display class and display driver class in the following file (new style):
#include "GxEPD2_display_selection_new_style.h"

// include the BasicEncoder adapted for GxEPD2_RotaryUsingBusyCallback.ino example use, without interrupts on any processor
#include "BasicEncoder.h"

// define the pins to be used by the BasicEncoder
const int8_t pinA = 5;  // D1 on my Wemos D1 mini
const int8_t pinB = 16; // D0 on my Wemos D1 mini
//const int8_t pinA = 2; // on UNO
//const int8_t pinB = 3; // on UNO

BasicEncoder encoder(pinA, pinB);

// busyCallback function called during waiting for BUSY to end, to service encoder
void busyCallback(const void* p)
{
  encoder.service();
}

// for handling alternative SPI pins (ESP32, RP2040) see example GxEPD2_Example.ino

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");
  delay(100);
  display.init(); // disable diagnostics to avoid delay and catch most rotary pulses
  //display.init(115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
  //display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  display.epd2.setBusyCallback(busyCallback); // register callback to be called during BUSY active time
  // first update should be full refresh
  helloWorld();
  delay(1000);
}

void loop()
{
  encoder.service();
  int encoder_change = encoder.get_change();
  if (encoder_change)
  {
    showEncoderValue(encoder.get_count());
  }
}

const char HelloWorld[] = "Hello World!";

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

const char EncoderValue[] = "Encoder = ";

#if (defined(ESP8266) || defined(ESP32)) && false
#include <StreamString.h>
#define PrintString StreamString
#else
class PrintString : public Print, public String
{
  public:
  PrintString(const char s[]) : String(s){};
    size_t write(uint8_t data) override
    {
      return concat(char(data));
    };
};
#endif

void showEncoderValue(int value)
{
  //Serial.println("showEncoderValue");
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(display.epd2.hasColor ? GxEPD_RED : GxEPD_BLACK);
  PrintString valueString(EncoderValue);
  valueString.print(value);
  static uint16_t ww = 0;
  static uint16_t wh = 0;
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(valueString, 0, 0, &tbx, &tby, &tbw, &tbh);
  if (tbw > ww) ww = tbw;
  if (tbh > wh) wh = tbh;
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() * 3 / 4) - tbh / 2) - tby; // y is base line!
  uint16_t wx = (display.width() - ww) / 2;
  uint16_t wy = ((display.height() * 3 / 4) - wh / 2);
  display.setPartialWindow(wx, wy, ww, wh);
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(valueString);
  }
  while (display.nextPage());
  //Serial.println("showEncoderValue done");
}
