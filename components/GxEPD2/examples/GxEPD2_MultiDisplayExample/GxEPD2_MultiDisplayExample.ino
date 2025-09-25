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

// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter
// enable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 1 

// uncomment next line to use class GFX of library GFX_Root instead of Adafruit_GFX
//#include <GFX.h>
// Note: if you use this with ENABLE_GxEPD2_GFX 1:
//       uncomment it in GxEPD2_GFX.h too, or add #include <GFX.h> before any #include <GxEPD2_GFX.h>
// !!!!  ============================================================================================ !!!!

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// create display class instances for each display, each instance with different CS line, each instance with RST disabled,
// (or use separate RST pins for each display),
// each instance with different BUSY line, or BUSY lines or-ed to one pin
// disable reset line to disable cross resets by multiple instances

#if defined (ESP8266)
#define RST_PIN 0 // D3(0)
#define CS_1 SS // CS = D8(15)
#define CS_2 16 // CS = D0(16)
#define CS_3  5 // CS = D1(5)
// BUSY lines can be or-ed with diodes and pulldown register for displays with BUSY active HIGH
#define BUSY_H_x 4 // BUSY = D2(4)
// BUSY lines can be or-ed with diodes and pullup resistor for displays with BUSY active LOW
//#define BUSY_L_x 4 // BUSY = D2(4)
// select one and adapt to your mapping, can use full buffer size (full HEIGHT)
//GxEPD2_BW<GxEPD2_154, GxEPD2_154::HEIGHT> display1(GxEPD2_154(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEP015OC1 no longer available
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display1(GxEPD2_154_D67(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEH0154D67
//GxEPD2_BW<GxEPD2_154_T8, GxEPD2_154_T8::HEIGHT> display(GxEPD2_154_T8(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW0154T8 152x152
//GxEPD2_BW<GxEPD2_154_M09, GxEPD2_154_M09::HEIGHT> display(GxEPD2_154_M09(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW0154M09 200x200
//GxEPD2_BW<GxEPD2_154_M10, GxEPD2_154_M10::HEIGHT> display(GxEPD2_154_M10(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW0154M10 152x152
GxEPD2_BW<GxEPD2_213, GxEPD2_213::HEIGHT> display2(GxEPD2_213(/*CS=*/ CS_2, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x));
//GxEPD2_BW<GxEPD2_213_B72, GxEPD2_213_B72::HEIGHT> display(GxEPD2_213_B72(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEH0213B72
//GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEH0213B73
//GxEPD2_BW<GxEPD2_213_flex, GxEPD2_213_flex::HEIGHT> display(GxEPD2_213_flex(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW0213I5F
//GxEPD2_BW<GxEPD2_213_M21, GxEPD2_213_M21::HEIGHT> display(GxEPD2_213_M21(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW0213M21
//GxEPD2_BW<GxEPD2_213_T5D, GxEPD2_213_T5D::HEIGHT> display(GxEPD2_213_T5D(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW0213T5D
GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> display3(GxEPD2_290(/*CS=*/ CS_3, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x));
//GxEPD2_BW<GxEPD2_290_T5, GxEPD2_290_T5::HEIGHT> display(GxEPD2_290_T5(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW029T5
//GxEPD2_BW<GxEPD2_290_T5D, GxEPD2_290_T5D::HEIGHT> display(GxEPD2_290_T5D(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW029T5D
//GxEPD2_BW<GxEPD2_290_M06, GxEPD2_290_M06::HEIGHT> display(GxEPD2_290_M06(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW029M06
//GxEPD2_BW<GxEPD2_260, GxEPD2_260::HEIGHT> display(GxEPD2_260(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW<GxEPD2_270, GxEPD2_270::HEIGHT> display(GxEPD2_270(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW<GxEPD2_371, GxEPD2_371::HEIGHT> display(GxEPD2_371(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display(GxEPD2_420(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW<GxEPD2_420_M01, GxEPD2_420_M01::HEIGHT> display(GxEPD2_420_M01(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEW042M01
// can use only half buffer size
//GxEPD2_BW < GxEPD2_583, GxEPD2_583::HEIGHT / 2 > display(GxEPD2_583(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW < GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT / 2 > display(GxEPD2_583_T8(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW < GxEPD2_750, GxEPD2_750::HEIGHT / 2 > display(GxEPD2_750(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW < GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT / 2 > display(GxEPD2_750_T7(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEW075T7 800x480
// 3-color e-papers
//GxEPD2_3C<GxEPD2_154c, GxEPD2_154c::HEIGHT> display(GxEPD2_154c(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEW0154Z04 no longer available
//GxEPD2_3C<GxEPD2_154_Z90c, GxEPD2_154_Z90c::HEIGHT> display(GxEPD2_154_Z90c(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEH0154Z90
//GxEPD2_3C<GxEPD2_213c, GxEPD2_213c::HEIGHT> display(GxEPD2_213c(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_3C<GxEPD2_213_Z19c, GxEPD2_213_Z19c::HEIGHT> display(GxEPD2_213_Z19c(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEW0213Z19
//GxEPD2_3C<GxEPD2_290c, GxEPD2_290c::HEIGHT> display(GxEPD2_290c(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_3C<GxEPD2_290_Z13c, GxEPD2_290_Z13c::HEIGHT> display(GxEPD2_290_Z13c(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEH029Z13
//GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEM029C90
//GxEPD2_3C<GxEPD2_270c, GxEPD2_270c::HEIGHT> display(GxEPD2_270c(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
// can use only half buffer size
//GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT / 2> display(GxEPD2_420c(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
// can use only quarter buffer size
//GxEPD2_3C<GxEPD2_583c, GxEPD2_583c::HEIGHT / 4> display(GxEPD2_583c(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT / 4> display3(GxEPD2_750c(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_3C < GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 4 > display(GxEPD2_750c_Z08(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEW075Z08 800x480
#endif

#if defined(ESP32)
#define RST_PIN 16
#define CS_1 SS // CS = 5(SS)
#define CS_2  0 // CS = 0
#define CS_3  2 // CS = 2
#define CS_4 13 // CS = 13
// BUSY lines can be or-ed with diodes and pulldown register for displays with BUSY active HIGH
#define BUSY_H_x 4 // BUSY = 4
// BUSY lines can be or-ed with diodes and pullup resistor for displays with BUSY active LOW
#define BUSY_L_x 15 // BUSY = 15
// select one and adapt to your mapping, can use full buffer size (full HEIGHT)
//GxEPD2_BW<GxEPD2_154, GxEPD2_154::HEIGHT> display1(GxEPD2_154(/*CS=*/ CS_1, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEP015OC1 no longer available
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display1(GxEPD2_154_D67(/*CS=*/ CS_1, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEH0154D67
//GxEPD2_BW<GxEPD2_154_T8, GxEPD2_154_T8::HEIGHT> display(GxEPD2_154_T8(/*CS=*/ CS_1, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW0154T8 152x152
//GxEPD2_BW<GxEPD2_154_M09, GxEPD2_154_M09::HEIGHT> display(GxEPD2_154_M09(/*CS=*/ CS_1, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW0154M09 200x200
//GxEPD2_BW<GxEPD2_154_M10, GxEPD2_154_M10::HEIGHT> display(GxEPD2_154_M10(/*CS=*/ CS_1, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW0154M10 152x152
GxEPD2_BW<GxEPD2_213, GxEPD2_213::HEIGHT> display2(GxEPD2_213(/*CS=*/ CS_2, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x));
//GxEPD2_BW<GxEPD2_213_B72, GxEPD2_213_B72::HEIGHT> display(GxEPD2_213_B72(/*CS=*/ CS_2, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEH0213B72
//GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=*/ CS_2, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEH0213B73
//GxEPD2_BW<GxEPD2_213_flex, GxEPD2_213_flex::HEIGHT> display(GxEPD2_213_flex(/*CS=*/ CS_2, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW0213I5F
//GxEPD2_BW<GxEPD2_213_M21, GxEPD2_213_M21::HEIGHT> display(GxEPD2_213_M21(/*CS=*/ CS_2, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW0213M21
//GxEPD2_BW<GxEPD2_213_T5D, GxEPD2_213_T5D::HEIGHT> display(GxEPD2_213_T5D(//*CS=*/ CS_2, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW0213T5D
GxEPD2_BW<GxEPD2_290, GxEPD2_290::HEIGHT> display3(GxEPD2_290(/*CS=*/ CS_3, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x));
//GxEPD2_BW<GxEPD2_290_T5, GxEPD2_290_T5::HEIGHT> display(GxEPD2_290_T5(/*CS=*/ CS_1, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW029T5
//GxEPD2_BW<GxEPD2_290_T5D, GxEPD2_290_T5D::HEIGHT> display(GxEPD2_290_T5D(/*CS=*/ CS_2, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW029T5D
//GxEPD2_BW<GxEPD2_290_M06, GxEPD2_290_M06::HEIGHT> display(GxEPD2_290_M06(/*CS=*/ CS_2, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_H_x)); // GDEW029M06
//GxEPD2_BW<GxEPD2_260, GxEPD2_260::HEIGHT> display(GxEPD2_260(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW<GxEPD2_270, GxEPD2_270::HEIGHT> display(GxEPD2_270(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW<GxEPD2_371, GxEPD2_371::HEIGHT> display(GxEPD2_371(/*CS=*/ CS_x, /*DC=D3*/ 0, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW<GxEPD2_420, GxEPD2_420::HEIGHT> display4(GxEPD2_420(/*CS=*/ CS_4, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW<GxEPD2_420_M01, GxEPD2_420_M01::HEIGHT> display(GxEPD2_420_M01(/*CS=*/ CS_4, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEW042M01
//GxEPD2_BW<GxEPD2_583, GxEPD2_583::HEIGHT> display(GxEPD2_583(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT> display(GxEPD2_583_T8(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));
//GxEPD2_BW<GxEPD2_750, GxEPD2_750::HEIGHT> display(GxEPD2_750(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEW075T7 800x480
// 3-color e-papers
//GxEPD2_3C<GxEPD2_154c, GxEPD2_154c::HEIGHT> display(GxEPD2_154c(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEW0154Z04 no longer available
//GxEPD2_3C<GxEPD2_154_Z90c, GxEPD2_154_Z90c::HEIGHT> display(GxEPD2_154_Z90c(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEH0154Z90
//GxEPD2_3C<GxEPD2_213c, GxEPD2_213c::HEIGHT> display(GxEPD2_213c(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_3C<GxEPD2_213_Z19c, GxEPD2_213_Z19c::HEIGHT> display(GxEPD2_213_Z19c(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEW0213Z19
//GxEPD2_3C<GxEPD2_290c, GxEPD2_290c::HEIGHT> display(GxEPD2_290c(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_3C<GxEPD2_290_Z13c, GxEPD2_290_Z13c::HEIGHT> display(GxEPD2_290_Z13c(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEH029Z13
//GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEM029C90
//GxEPD2_3C<GxEPD2_270c, GxEPD2_270c::HEIGHT> display(GxEPD2_270c(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT> display(GxEPD2_420c(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
GxEPD2_3C<GxEPD2_750c, GxEPD2_750c::HEIGHT> display4(GxEPD2_750c(/*CS=*/ CS_4, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x));
//GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT> display(GxEPD2_750c_Z08(/*CS=*/ CS_x, /*DC=*/ 17, /*RST=*/ -1, /*BUSY=*/ BUSY_L_x)); // GDEW075Z08 800x480
#endif

// comment out unused bitmaps to reduce code space used
#include "bitmaps/Bitmaps200x200.h" // 1.54" b/w
#include "bitmaps/Bitmaps128x250.h" // 2.13" b/w
#include "bitmaps/Bitmaps128x296.h" // 2.9"  b/w
#include "bitmaps/Bitmaps176x264.h" // 2.7"  b/w
#include "bitmaps/Bitmaps400x300.h" // 4.2"  b/w
#include "bitmaps/Bitmaps640x384.h" // 7.5"  b/w
// 3-color
#include "bitmaps/Bitmaps3c200x200.h" // 1.54" b/w/r
#include "bitmaps/Bitmaps3c104x212.h" // 2.13" b/w/r
#include "bitmaps/Bitmaps3c128x296.h" // 2.9"  b/w/r
#include "bitmaps/Bitmaps3c176x264.h" // 2.7"  b/w/r
#include "bitmaps/Bitmaps3c400x300.h" // 4.2"  b/w/r

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");

  // one common reset for all displays
  digitalWrite(RST_PIN, HIGH);
  pinMode(RST_PIN, OUTPUT);
  delay(20);
  digitalWrite(RST_PIN, LOW);
  delay(20);
  digitalWrite(RST_PIN, HIGH);
  delay(200);

  display1.init(115200); // enable diagnostic output on Serial
  display2.init(115200); // enable diagnostic output on Serial
  display3.init(115200); // enable diagnostic output on Serial
#if defined(ESP32)
  display4.init(115200); // enable diagnostic output on Serial
#endif
  // first update should be full refresh
  helloWorld(display1);
  helloWorld(display2);
  helloWorld(display3);
#if defined(ESP32)
  helloWorld(display4);
#endif
  delay(1000);
  // partial refresh mode can be used to full screen,
  // effective if display panel hasFastPartialUpdate
  helloFullScreenPartialMode(display1);
  helloFullScreenPartialMode(display2);
  helloFullScreenPartialMode(display3);
#if defined(ESP32)
  helloFullScreenPartialMode(display4);
#endif
  delay(1000);
  helloArduino(display1);
  helloArduino(display2);
  helloArduino(display3);
#if defined(ESP32)
  helloArduino(display4);
#endif
  delay(1000);
  helloEpaper(display1);
  helloEpaper(display2);
  helloEpaper(display3);
#if defined(ESP32)
  helloEpaper(display4);
#endif
  delay(1000);
  showFont(display1, "FreeMonoBold9pt7b", &FreeMonoBold9pt7b);
  showFont(display2, "FreeMonoBold9pt7b", &FreeMonoBold9pt7b);
  showFont(display3, "FreeMonoBold9pt7b", &FreeMonoBold9pt7b);
#if defined(ESP32)
  showFont(display4, "FreeMonoBold9pt7b", &FreeMonoBold9pt7b);
#endif
  delay(1000);
  drawBitmaps(display1);
  drawBitmaps(display2);
  drawBitmaps(display3);
#if defined(ESP32)
  drawBitmaps(display4);
#endif
//  if (display1.epd2.hasPartialUpdate)
//  {
//    showPartialUpdate(display1);
//    delay(1000);
//  } // else // on GDEW0154Z04 only full update available, doesn't look nice
  //drawCornerTest(display1);
  //showBox(display1, 16, 16, 48, 32, false);
  //showBox(display1, 16, 56, 48, 32, true);
  display1.powerOff();
  display2.powerOff();
  display3.powerOff();
  Serial.println("setup done");
}

void loop()
{
}


void helloWorld(GxEPD2_GFX& display)
{
  //Serial.println("helloWorld");
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  uint16_t x = (display.width() - 160) / 2;
  uint16_t y = display.height() / 2;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.println("Hello World!");
  }
  while (display.nextPage());
  //Serial.println("helloWorld done");
}


void helloFullScreenPartialMode(GxEPD2_GFX& display)
{
  //Serial.println("helloFullScreenPartialMode");
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  display.firstPage();
  do
  {
    uint16_t x = (display.width() - 160) / 2;
    uint16_t y = display.height() / 2;
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.println("Hello World!");
    y = display.height() / 4;
    display.setCursor(x, y);
    display.println("full screen");
    y = display.height() * 3 / 4;
    if (display.width() <= 200) x = 0;
    display.setCursor(x, y);
    if (display.epd2.hasFastPartialUpdate)
    {
      display.println("fast partial mode");
    }
    else if (display.epd2.hasPartialUpdate)
    {
      display.println("slow partial mode");
    }
    else
    {
      display.println("no partial mode");
    }
  }
  while (display.nextPage());
  //Serial.println("helloFullScreenPartialMode done");
}


void helloArduino(GxEPD2_GFX& display)
{
  //Serial.println("helloArduino");
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(display.epd2.hasColor ? GxEPD_RED : GxEPD_BLACK);
  uint16_t x = (display.width() - 160) / 2;
  uint16_t y = display.height() / 4;
  display.setPartialWindow(0, y - 14, display.width(), 20);
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.println("Hello Arduino!");
  }
  while (display.nextPage());
  delay(1000);
  //Serial.println("helloArduino done");
}


void helloEpaper(GxEPD2_GFX& display)
{
  //Serial.println("helloEpaper");
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(display.epd2.hasColor ? GxEPD_RED : GxEPD_BLACK);
  uint16_t x = (display.width() - 160) / 2;
  uint16_t y = display.height() * 3 / 4;
  display.setPartialWindow(0, y - 14, display.width(), 20);
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.println("Hello E-Paper!");
  }
  while (display.nextPage());
  //Serial.println("helloEpaper done");
}


void showBox(GxEPD2_GFX& display, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool partial)
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


void drawCornerTest(GxEPD2_GFX& display)
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


void showFont(GxEPD2_GFX& display, const char name[], const GFXfont* f)
{
  display.setFullWindow();
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.firstPage();
  do
  {
    drawFont(display, name, f);
  }
  while (display.nextPage());
}


//void drawFont(GxEPD2_GFX& display, const char name[], const GFXfont* f);


void drawFont(GxEPD2_GFX& display, const char name[], const GFXfont* f)
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


void showPartialUpdate(GxEPD2_GFX& display)
{
  // some useful background
  helloWorld(display);
  // use asymmetric values for test
  uint16_t box_x = 10;
  uint16_t box_y = 15;
  uint16_t box_w = 70;
  uint16_t box_h = 20;
  uint16_t cursor_y = box_y + box_h - 6;
  float value = 13.95;
  uint16_t incr = display.epd2.hasFastPartialUpdate ? 1 : 3;
  display.setFont(&FreeMonoBold9pt7b);
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


#ifdef _GxBitmaps200x200_H_

void drawBitmaps200x200(GxEPD2_GFX& display);

void drawBitmaps200x200(GxEPD2_GFX& display)
{
#if defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    logo200x200, first200x200 //, second200x200, third200x200, fourth200x200, fifth200x200, sixth200x200, senventh200x200, eighth200x200
  };
#elif defined(_BOARD_GENERIC_STM32F103C_H_)
  const unsigned char* bitmaps[] =
  {
    logo200x200, first200x200, second200x200, third200x200, fourth200x200, fifth200x200 //, sixth200x200, senventh200x200, eighth200x200
  };
#else
  const unsigned char* bitmaps[] =
  {
    logo200x200, first200x200, second200x200, third200x200, fourth200x200, fifth200x200, sixth200x200, senventh200x200, eighth200x200
  };
#endif
  if ((display.epd2.panel == GxEPD2::GDEP015OC1) || (display.epd2.panel == GxEPD2::GDEH0154D67))
  {
    bool m = display.mirror(true);
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
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
      x += 40;
      y += 40;
      if ((x >= int16_t(display.epd2.WIDTH)) || (y >= int16_t(display.epd2.HEIGHT))) break;
    }
    if (!display.epd2.hasFastPartialUpdate) break; // comment out for full show
    break; // comment out for full show
  }
  display.writeScreenBuffer(); // use default for white
  display.writeImage(bitmaps[0], int16_t(0), 0, 200, 200, false, mirror_y, true);
  display.writeImage(bitmaps[0], int16_t(int16_t(display.epd2.WIDTH) - 200), int16_t(display.epd2.HEIGHT) - 200, 200, 200, false, mirror_y, true);
  display.refresh(true);
  delay(2000);
}
#endif

#ifdef _GxBitmaps128x250_H_

void drawBitmaps128x250(GxEPD2_GFX& display);

void drawBitmaps128x250(GxEPD2_GFX& display)
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
  if (display.epd2.panel == GxEPD2::GDE0213B1)
  {
    bool m = display.mirror(true);
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
    display.mirror(m);
  }
}
#endif

#ifdef _GxBitmaps128x296_H_

void drawBitmaps128x296(GxEPD2_GFX& display);

void drawBitmaps128x296(GxEPD2_GFX& display)
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
  if (display.epd2.panel == GxEPD2::GDEH029A1)
  {
    bool m = display.mirror(true);
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
    display.mirror(m);
  }
}
#endif

#ifdef _GxBitmaps176x264_H_

void drawBitmaps176x264(GxEPD2_GFX& display);

void drawBitmaps176x264(GxEPD2_GFX& display)
{
  const unsigned char* bitmaps[] =
  {
    Bitmap176x264_1, Bitmap176x264_2
  };
  if (display.epd2.panel == GxEPD2::GDEW027W3)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps400x300_H_

void drawBitmaps400x300(GxEPD2_GFX& display);

void drawBitmaps400x300(GxEPD2_GFX& display)
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap400x300_1, Bitmap400x300_2
  };
#else
  const unsigned char* bitmaps[] = {}; // not enough code space
#endif
  if (display.epd2.panel == GxEPD2::GDEW042T2)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps640x384_H_

void drawBitmaps640x384(GxEPD2_GFX& display);

void drawBitmaps640x384(GxEPD2_GFX& display)
{
#if !defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    Bitmap640x384_1, Bitmap640x384_2
  };
#else
  const unsigned char* bitmaps[] = {}; // not enough code space
#endif
  if (display.epd2.panel == GxEPD2::GDEW075T8)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmaps[i], display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
      }
      while (display.nextPage());
      delay(2000);
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

void drawBitmaps3c200x200(GxEPD2_GFX& display);

void drawBitmaps3c200x200(GxEPD2_GFX& display)
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
        delay(2000);
        x += 40;
        y += 40;
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

void drawBitmaps3c104x212(GxEPD2_GFX& display);

void drawBitmaps3c104x212(GxEPD2_GFX& display)
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

#ifdef _GxBitmaps3c128x296_H_

void drawBitmaps3c128x296(GxEPD2_GFX& display);

void drawBitmaps3c128x296(GxEPD2_GFX& display)
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
  if (display.epd2.panel == GxEPD2::GDEW029Z10)
  {
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.firstPage();
      do
      {
        display.fillScreen(GxEPD_WHITE);
        display.drawInvertedBitmap(0, 0, bitmap_pairs[i].black, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
        if (bitmap_pairs[i].red == WS_Bitmap3c128x296_red)
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

#ifdef _GxBitmaps3c176x264_H_

void drawBitmaps3c176x264(GxEPD2_GFX& display);

void drawBitmaps3c176x264(GxEPD2_GFX& display)
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

void drawBitmaps3c400x300(GxEPD2_GFX& display);

void drawBitmaps3c400x300(GxEPD2_GFX& display)
{
#if !defined(__AVR)
  bitmap_pair bitmap_pairs[] =
  {
    {Bitmap3c400x300_1_black, Bitmap3c400x300_1_red},
    {Bitmap3c400x300_2_black, Bitmap3c400x300_2_red},
    {WS_Bitmap3c400x300_black, WS_Bitmap3c400x300_red}
  };
#else
  bitmap_pair bitmap_pairs[] = {}; // not enough code space
#endif
  if (display.epd2.panel == GxEPD2::GDEW042Z15)
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


void drawBitmaps(GxEPD2_GFX& display)
{
  display.setFullWindow();
  display.setRotation(0);
#ifdef _GxBitmaps128x250_H_
  drawBitmaps128x250(display);
#endif
#ifdef _GxBitmaps128x296_H_
  drawBitmaps128x296(display);
#endif
#ifdef _GxBitmaps176x264_H_
  drawBitmaps176x264(display);
#endif
#ifdef _GxBitmaps400x300_H_
  drawBitmaps400x300(display);
#endif
#ifdef _GxBitmaps640x384_H_
  drawBitmaps640x384(display);
#endif
  // 3-color
#ifdef _GxBitmaps3c104x212_H_
  drawBitmaps3c104x212(display);
#endif
#ifdef _GxBitmaps3c128x296_H_
  drawBitmaps3c128x296(display);
#endif
#ifdef _GxBitmaps3c176x264_H_
  drawBitmaps3c176x264(display);
#endif
#ifdef _GxBitmaps3c400x300_H_
  drawBitmaps3c400x300(display);
#endif
  // show these after the specific bitmaps
#ifdef _GxBitmaps200x200_H_
  drawBitmaps200x200(display);
#endif
  // 3-color
#ifdef _GxBitmaps3c200x200_H_
  drawBitmaps3c200x200(display);
#endif
}
