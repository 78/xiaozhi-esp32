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

#if !defined(__AVR) && !defined(_BOARD_GENERIC_STM32F103C_H_)

// note 16.11.2019: the compiler may exclude code based on constant if statements (display.epd2.panel == constant),
//                  therefore bitmaps may get optimized out by the linker

// comment out unused bitmaps to reduce code space used
#include "bitmaps/Bitmaps152x152.h" // 1.54" b/w
#include "bitmaps/Bitmaps200x200.h" // 1.54" b/w
#include "bitmaps/Bitmaps104x212.h" // 2.13" b/w flexible GDEW0213I5F
#include "bitmaps/Bitmaps128x250.h" // 2.13" b/w
#include "bitmaps/Bitmaps128x296.h" // 2.9"  b/w
#include "bitmaps/Bitmaps152x296.h" // 2.6"  b/w
#include "bitmaps/Bitmaps176x264.h" // 2.7"  b/w
#include "bitmaps/Bitmaps240x416.h" // 3.71"  b/w
#include "bitmaps/Bitmaps400x300.h" // 4.2"  b/w
#include "bitmaps/Bitmaps648x480.h" // 5.38"  b/w
#include "bitmaps/Bitmaps640x384.h" // 7.5"  b/w
#include "bitmaps/Bitmaps800x480.h" // 7.5"  b/w
// 3-color
#include "bitmaps/Bitmaps3c200x200.h" // 1.54" b/w/r
#include "bitmaps/Bitmaps3c104x212.h" // 2.13" b/w/r
#include "bitmaps/Bitmaps3c128x296.h" // 2.9"  b/w/r
#include "bitmaps/Bitmaps3c176x264.h" // 2.7"  b/w/r
#include "bitmaps/Bitmaps3c400x300.h" // 4.2"  b/w/r
#if defined(ESP8266) || defined(ESP32)
#include "bitmaps/Bitmaps3c800x480.h" // 7.5"  b/w/r
#include "bitmaps/Bitmaps3c880x528.h" // 7.5"  b/w/r
#include "bitmaps/WS_Bitmaps800x600.h" // 6.0"  grey
#include "bitmaps/WS_Bitmaps7c192x143.h" // 5.65" 7-color
#endif
#if defined(ESP32)
#include "bitmaps/Bitmaps1304x984.h" // 12.48" b/w
#endif

#else

// select only one to fit in code space
//#include "bitmaps/Bitmaps200x200.h" // 1.54" b/w
//#include "bitmaps/Bitmaps128x250.h" // 2.13" b/w
//#include "bitmaps/Bitmaps128x296.h" // 2.9"  b/w
//#include "bitmaps/Bitmaps176x264.h" // 2.7"  b/w
////#include "bitmaps/Bitmaps400x300.h" // 4.2"  b/w // not enough code space
////#include "bitmaps/Bitmaps640x384.h" // 7.5"  b/w // not enough code space
// 3-color
//#include "bitmaps/Bitmaps3c200x200.h" // 1.54" b/w/r
//#include "bitmaps/Bitmaps3c104x212.h" // 2.13" b/w/r
//#include "bitmaps/Bitmaps3c128x296.h" // 2.9"  b/w/r
//#include "bitmaps/Bitmaps3c176x264.h" // 2.7"  b/w/r
////#include "bitmaps/Bitmaps3c400x300.h" // 4.2"  b/w/r // not enough code space

#endif

// for handling alternative SPI pins (ESP32, RP2040) see example GxEPD2_Example.ino

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("setup");
  //display.init(115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
  display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  // first update should be full refresh
  helloWorld();
  delay(1000);
  // partial refresh mode can be used to full screen,
  // effective if display panel hasFastPartialUpdate
  helloFullScreenPartialMode();
  delay(1000);
  helloArduino();
  delay(1000);
  helloEpaper();
  delay(1000);
  showFont("FreeMonoBold9pt7b", &FreeMonoBold9pt7b);
  delay(1000);
  drawBitmaps();
  if (display.epd2.hasPartialUpdate)
  {
    showPartialUpdate();
    delay(1000);
  } // else // on GDEW0154Z04 only full update available, doesn't look nice
  //drawCornerTest();
  //showBox(16, 16, 48, 32, false);
  //showBox(16, 56, 48, 32, true);
  display.powerOff();
  Serial.println("setup done");
}

void loop()
{
}

const char HelloWorld[] = "Hello World!";
const char HelloArduino[] = "Hello Arduino!";
const char HelloEpaper[] = "Hello E-Paper!";

struct coordinates
{
  uint16_t x;
  uint16_t y;
};

void helloWorldCallback(const void* p)
{
  const coordinates& where(*reinterpret_cast<const coordinates*>(p));
  display.setCursor(where.x, where.y);
  display.print(HelloWorld);
}

void helloWorld()
{
  //Serial.println("helloWorld");
  coordinates cursor;
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  if (display.epd2.WIDTH < 104) display.setFont(0);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center bounding box by transposition of origin:
  cursor.x = ((display.width() - tbw) / 2) - tbx;
  cursor.y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.drawPaged(helloWorldCallback, &cursor);
  //Serial.println("helloWorld done");
}

struct text_at
{
  const char* text;
  coordinates where;
};

void helloFullScreenPartialModeCallback(const void* p)
{
  const text_at* what(reinterpret_cast<const text_at*>(p));
  display.fillScreen(GxEPD_WHITE);
  display.setCursor(what[0].where.x, what[0].where.y);
  display.print(what[0].text);
  display.setCursor(what[1].where.x, what[1].where.y);
  display.print(what[1].text);
  display.setCursor(what[2].where.x, what[2].where.y);
  display.print(what[2].text);
}

void helloFullScreenPartialMode()
{
  //Serial.println("helloFullScreenPartialMode");
  const char fullscreen[] = "full screen update";
  const char fpm[] = "fast partial mode";
  const char spm[] = "slow partial mode";
  const char npm[] = "no partial mode";
  text_at what[3];
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
  int16_t tbx, tby; uint16_t tbw, tbh;
  // center update text
  display.getTextBounds(fullscreen, 0, 0, &tbx, &tby, &tbw, &tbh);
  what[0].text = fullscreen;
  what[0].where.x = ((display.width() - tbw) / 2) - tbx;
  what[0].where.y = ((display.height() / 4) - tbh / 2) - tby;
  // center update mode
  display.getTextBounds(updatemode, 0, 0, &tbx, &tby, &tbw, &tbh);
  what[1].text = updatemode;
  what[1].where.x = ((display.width() - tbw) / 2) - tbx;
  what[1].where.y = ((display.height() * 3 / 4) - tbh / 2) - tby;
  // center HelloWorld
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  what[2].text = HelloWorld;
  what[2].where.x = ((display.width() - tbw) / 2) - tbx;
  what[2].where.y = ((display.height() - tbh) / 2) - tby;
  display.drawPaged(helloFullScreenPartialModeCallback, &what);
  //Serial.println("helloFullScreenPartialMode done");
}

void helloArduinoCallback(const void* p)
{
  const coordinates& where(*reinterpret_cast<const coordinates*>(p));
  display.setCursor(where.x, where.y);
  display.print(HelloArduino);
}

void helloArduino()
{
  //Serial.println("helloArduino");
  coordinates cursor;
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  if (display.epd2.WIDTH < 104) display.setFont(0);
  display.setTextColor(display.epd2.hasColor ? GxEPD_RED : GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  // align with centered HelloWorld
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  cursor.x = ((display.width() - tbw) / 2) - tbx;
  // height might be different
  display.getTextBounds(HelloArduino, 0, 0, &tbx, &tby, &tbw, &tbh);
  cursor.y = ((display.height() / 4) - tbh / 2) - tby; // y is base line!
  // make the window big enough to cover (overwrite) descenders of previous text
  uint16_t wh = FreeMonoBold9pt7b.yAdvance;
  uint16_t wy = (display.height() / 4) - wh / 2;
  display.setPartialWindow(0, wy, display.width(), wh);
  display.drawPaged(helloArduinoCallback, &cursor);
  delay(1000);
  //Serial.println("helloArduino done");
}

void helloEpaperCallback(const void* p)
{
  const coordinates& where(*reinterpret_cast<const coordinates*>(p));
  display.setCursor(where.x, where.y);
  display.print(HelloEpaper);
}

void helloEpaper()
{
  //Serial.println("helloEpaper");
  coordinates cursor;
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  if (display.epd2.WIDTH < 104) display.setFont(0);
  display.setTextColor(display.epd2.hasColor ? GxEPD_RED : GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  // align with centered HelloWorld
  display.getTextBounds(HelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  cursor.x = ((display.width() - tbw) / 2) - tbx;
  // height might be different
  display.getTextBounds(HelloEpaper, 0, 0, &tbx, &tby, &tbw, &tbh);
  cursor.y = ((display.height() * 3 / 4) - tbh / 2) - tby; // y is base line!
  // make the window big enough to cover (overwrite) descenders of previous text
  uint16_t wh = FreeMonoBold9pt7b.yAdvance;
  uint16_t wy = (display.height() * 3 / 4) - wh / 2;
  display.setPartialWindow(0, wy, display.width(), wh);
  display.drawPaged(helloEpaperCallback, &cursor);
  //Serial.println("helloEpaper done");
}

struct DrawFontParameters
{
  const char* name;
  const GFXfont* f;
};

void showFont(const char name[], const GFXfont* f)
{
  DrawFontParameters fontParameters{name, f};
  display.setFullWindow();
  display.setRotation(0);
  display.setTextColor(GxEPD_BLACK);
  display.drawPaged(drawFontCallback, &fontParameters);
}

void drawFontCallback(const void* parameters)
{
  const DrawFontParameters* p = reinterpret_cast<const DrawFontParameters*>(parameters);
  //display.setRotation(0);
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(p->f);
  display.setCursor(0, 0);
  display.println();
  display.println(p->name);
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

struct ShowBoxParameters
{
  uint16_t x, y, w, h;
  uint16_t color;
};

struct ShowValueParameters
{
  uint16_t x, y, w, h, cursor_y;
  float value;
};

void showBoxCallback(const void* parameters)
{
  const ShowBoxParameters* p = reinterpret_cast<const ShowBoxParameters*>(parameters);
  display.fillRect(p->x, p->y, p->w, p->h, p->color);
}

void showValueBoxCallback(const void* parameters)
{
  const ShowValueParameters* p = reinterpret_cast<const ShowValueParameters*>(parameters);
  display.fillRect(p->x, p->y, p->w, p->h, GxEPD_WHITE);
  display.setCursor(p->x, p->cursor_y);
  display.print(p->value);
}

void showPartialUpdate()
{
  // some useful background
  helloWorld();
  // use asymmetric values for test
  ShowBoxParameters boxParameters{10, 15, 70, 20, GxEPD_WHITE};
  ShowValueParameters valueParameters{10, 15, 70, 20, 0, 13.95};
  valueParameters.cursor_y = valueParameters.y + valueParameters.h - 6;
  float value = 13.95;
  uint16_t incr = display.epd2.hasFastPartialUpdate ? 1 : 3;
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  // show where the update box is
  for (uint16_t r = 0; r < 4; r++)
  {
    display.setRotation(r);
    display.setPartialWindow(boxParameters.x, boxParameters.y, boxParameters.w, boxParameters.h);
    boxParameters.color = GxEPD_BLACK;
    display.drawPaged(showBoxCallback, &boxParameters);
    delay(2000);
    boxParameters.color = GxEPD_WHITE;
    display.drawPaged(showBoxCallback, &boxParameters);
    while (display.nextPage());
    delay(1000);
  }
  // show updates in the update box
  for (uint16_t r = 0; r < 4; r++)
  {
    display.setRotation(r);
    display.setPartialWindow(boxParameters.x, boxParameters.y, boxParameters.w, boxParameters.h);
    for (uint16_t i = 1; i <= 10; i += incr)
    {
      valueParameters.value = value * i;
      display.drawPaged(showValueBoxCallback, &valueParameters);
      delay(500);
    }
    delay(1000);
    boxParameters.color = GxEPD_WHITE;
    display.drawPaged(showBoxCallback, &boxParameters);
    delay(1000);
  }
}

void showBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool partial)
{
  //Serial.println("showBox");
  ShowBoxParameters boxParameters{x, y, w, h, GxEPD_BLACK};
  display.setRotation(1);
  if (partial)
  {
    display.setPartialWindow(x, y, w, h);
  }
  else
  {
    display.setFullWindow();
  }
  display.drawPaged(showBoxCallback, &boxParameters);
  //Serial.println("showBox done");
}

void drawCornerTestCallback(const void*)
{
  display.fillScreen(GxEPD_WHITE);
  display.fillRect(0, 0, 8, 8, GxEPD_BLACK);
  display.fillRect(display.width() - 18, 0, 16, 16, GxEPD_BLACK);
  display.fillRect(display.width() - 25, display.height() - 25, 24, 24, GxEPD_BLACK);
  display.fillRect(0, display.height() - 33, 32, 32, GxEPD_BLACK);
  display.setCursor(display.width() / 2, display.height() / 2);
  display.print(display.getRotation());
}

void drawCornerTest()
{
  display.setFullWindow();
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  for (uint16_t r = 0; r <= 4; r++)
  {
    display.setRotation(r);
    display.drawPaged(drawCornerTestCallback, 0);
    delay(2000);
  }
}

void drawBitmaps()
{
  display.setFullWindow();
#ifdef _GxBitmaps128x250_H_
  drawBitmaps128x250();
#endif
#ifdef _GxBitmaps128x296_H_
  drawBitmaps128x296();
#endif
#ifdef _GxBitmaps176x264_H_
  drawBitmaps176x264();
#endif
#ifdef _GxBitmaps400x300_H_
  drawBitmaps400x300();
#endif
#ifdef _GxBitmaps640x384_H_
  drawBitmaps640x384();
#endif
  // 3-color
#ifdef _GxBitmaps3c104x212_H_
  drawBitmaps3c104x212();
#endif
#ifdef _GxBitmaps3c128x296_H_
  drawBitmaps3c128x296();
#endif
#ifdef _GxBitmaps3c176x264_H_
  drawBitmaps3c176x264();
#endif
#ifdef _GxBitmaps3c400x300_H_
  drawBitmaps3c400x300();
#endif
  // show these after the specific bitmaps
#ifdef _GxBitmaps200x200_H_
  drawBitmaps200x200();
#endif
  // 3-color
#ifdef _GxBitmaps3c200x200_H_
  drawBitmaps3c200x200();
#endif
}

void drawInvertedBitmapCallback(const void* parameter)
{
  const uint8_t* bitmap = reinterpret_cast<const uint8_t*>(parameter);
  display.fillScreen(GxEPD_WHITE);
  display.drawInvertedBitmap(0, 0, bitmap, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
}

#ifdef _GxBitmaps200x200_H_
void drawBitmaps200x200()
{
#if defined(__AVR)
  const unsigned char* bitmaps[] =
  {
    logo200x200, first200x200 //, second200x200, third200x200, fourth200x200, fifth200x200, sixth200x200, senventh200x200, eighth200x200
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
      display.drawPaged(drawInvertedBitmapCallback, bitmaps[i]);
      delay(2000);
    }
    display.mirror(m);
  }
  //else
  {
    bool mirror_y = (display.epd2.panel != GxEPD2::GDE0213B1);
    display.clearScreen(); // use default for white
    uint16_t x = (display.epd2.WIDTH - 200) / 2;
    uint16_t y = (display.epd2.HEIGHT - 200) / 2;
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
      if ((x >= display.epd2.WIDTH) || (y >= display.epd2.HEIGHT)) break;
    }
    if (!display.epd2.hasFastPartialUpdate) break; // comment out for full show
    break; // comment out for full show
  }
  display.writeScreenBuffer(); // use default for white
  display.writeImage(bitmaps[0], 0, 0, 200, 200, false, mirror_y, true);
  display.writeImage(bitmaps[0], display.epd2.WIDTH - 200, display.epd2.HEIGHT - 200, 200, 200, false, mirror_y, true);
  display.refresh(true);
  delay(2000);
}
#endif

#ifdef _GxBitmaps128x250_H_
void drawBitmaps128x250()
{
  const unsigned char* bitmaps[] =
  {
    Bitmap128x250_1, logo128x250, first128x250, second128x250, third128x250
  };
  if (display.epd2.panel == GxEPD2::GDE0213B1)
  {
    bool m = display.mirror(true);
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.drawPaged(drawInvertedBitmapCallback, bitmaps[i]);
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
  if (display.epd2.panel == GxEPD2::GDEH029A1)
  {
    bool m = display.mirror(true);
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.drawPaged(drawInvertedBitmapCallback, bitmaps[i]);
      delay(2000);
    }
    display.mirror(m);
  }
}
#endif

#ifdef _GxBitmaps176x264_H_
void drawBitmaps176x264()
{
  const unsigned char* bitmaps[] =
  {
    Bitmap176x264_1, Bitmap176x264_2
  };
  if (display.epd2.panel == GxEPD2::GDEW027W3)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.drawPaged(drawInvertedBitmapCallback, bitmaps[i]);
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps400x300_H_
void drawBitmaps400x300()
{
  const unsigned char* bitmaps[] =
  {
    Bitmap400x300_1, Bitmap400x300_2
  };
  if (display.epd2.panel == GxEPD2::GDEW042T2)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.drawPaged(drawInvertedBitmapCallback, bitmaps[i]);
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps640x384_H_
void drawBitmaps640x384()
{
  const unsigned char* bitmaps[] =
  {
    Bitmap640x384_1, Bitmap640x384_2
  };
  if (display.epd2.panel == GxEPD2::GDEW075T8)
  {
    for (uint16_t i = 0; i < sizeof(bitmaps) / sizeof(char*); i++)
    {
      display.drawPaged(drawInvertedBitmapCallback, bitmaps[i]);
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

void drawBitmapPairCallback(const void* parameters)
{
  const bitmap_pair* bitmaps = reinterpret_cast<const bitmap_pair*>(parameters);
  display.fillScreen(GxEPD_WHITE);
  display.drawBitmap(0, 0, bitmaps->black, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
  display.drawBitmap(0, 0, bitmaps->red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
}

void drawInvertedBitmapPairCallback(const void* parameters)
{
  const bitmap_pair* bitmaps = reinterpret_cast<const bitmap_pair*>(parameters);
  display.fillScreen(GxEPD_WHITE);
  display.drawInvertedBitmap(0, 0, bitmaps->black, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
  display.drawInvertedBitmap(0, 0, bitmaps->red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
}

void drawSpecialBitmapPairCallback(const void* parameters)
{
  const bitmap_pair* bitmaps = reinterpret_cast<const bitmap_pair*>(parameters);
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
        byte = pgm_read_byte(&bitmaps->black[j * byteWidth * 2 + i / 4]);
#else
        byte = bitmaps->black[j * byteWidth * 2 + i / 4];
#endif
      }
      if (!(byte & 0x80))
      {
        display.drawPixel(i, j, GxEPD_BLACK);
      }
    }
  }
  display.drawInvertedBitmap(0, 0, bitmaps->red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
}

#ifdef _GxBitmaps3c200x200_H_
void drawBitmaps3c200x200()
{
  bitmap_pair bitmap_pairs[] =
  {
    //{Bitmap3c200x200_black, Bitmap3c200x200_red}, // special
    {WS_Bitmap3c200x200_black, WS_Bitmap3c200x200_red}
  };
  bitmap_pair special_bitmap{Bitmap3c200x200_black, Bitmap3c200x200_red};
  if (display.epd2.panel == GxEPD2::GDEW0154Z04)
  {
    display.drawPaged(drawSpecialBitmapPairCallback, &special_bitmap);
    delay(2000);
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.drawPaged(drawInvertedBitmapPairCallback, &bitmap_pairs[i]);
      delay(2000);
    }
  }
  if (display.epd2.hasColor)
  {
    display.clearScreen(); // use default for white
    uint16_t x = (display.epd2.WIDTH - 200) / 2;
    uint16_t y = (display.epd2.HEIGHT - 200) / 2;
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
        if ((x >= display.epd2.WIDTH) || (y >= display.epd2.HEIGHT)) break;
      }
    }
    display.writeScreenBuffer(); // use default for white
    display.writeImage(bitmap_pairs[0].black, bitmap_pairs[0].red, 0, 0, 200, 200, false, false, true);
    display.writeImage(bitmap_pairs[0].black, bitmap_pairs[0].red, display.epd2.WIDTH - 200, display.epd2.HEIGHT - 200, 200, 200, false, false, true);
    display.refresh();
    delay(2000);
  }
}
#endif

#ifdef _GxBitmaps3c104x212_H_
void drawBitmapPairCallback3c104x212(const void* parameters)
{
  const bitmap_pair* bitmaps = reinterpret_cast<const bitmap_pair*>(parameters);
  display.fillScreen(GxEPD_WHITE);
  display.drawInvertedBitmap(0, 0, bitmaps->black, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
  if (bitmaps->red == WS_Bitmap3c104x212_red)
  {
    display.drawInvertedBitmap(0, 0, bitmaps->red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
  }
  else display.drawBitmap(0, 0, bitmaps->red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
}
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
      display.drawPaged(drawBitmapPairCallback3c104x212, &bitmap_pairs[i]);
      delay(2000);
    }
  }
}
#endif

#ifdef _GxBitmaps3c128x296_H_
void drawBitmapPairCallback3c128x296(const void* parameters)
{
  const bitmap_pair* bitmaps = reinterpret_cast<const bitmap_pair*>(parameters);
  display.fillScreen(GxEPD_WHITE);
  display.drawInvertedBitmap(0, 0, bitmaps->black, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
  if (bitmaps->red == WS_Bitmap3c128x296_red)
  {
    display.drawInvertedBitmap(0, 0, bitmaps->red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
  }
  else display.drawBitmap(0, 0, bitmaps->red, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_RED);
}
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
  if (display.epd2.panel == GxEPD2::GDEW029Z10)
  {
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.drawPaged(drawBitmapPairCallback3c128x296, &bitmap_pairs[i]);
      delay(2000);
    }
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
      display.drawPaged(drawBitmapPairCallback, &bitmap_pairs[i]);
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
    {WS_Bitmap3c400x300_black, WS_Bitmap3c400x300_red}
  };
#else
  bitmap_pair bitmap_pairs[] = {}; // not enough code space
#endif
  if (display.epd2.panel == GxEPD2::GDEW042Z15)
  {
    for (uint16_t i = 0; i < sizeof(bitmap_pairs) / sizeof(bitmap_pair); i++)
    {
      display.drawPaged(drawInvertedBitmapPairCallback, &bitmap_pairs[i]);
      delay(2000);
    }
  }
}
#endif
