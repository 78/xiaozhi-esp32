// GxEPD2_WiFi_Example : Display Library example for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
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

#include "GxEPD2_WiFi_DisplaySelection.h"

#if defined (ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <WiFiClient.h>
#include <WiFiClientSecure.h>

const char* ssid     = "........";
const char* password = "........";
const int httpPort  = 80;
const int httpsPort = 443;

// note: the certificates have been moved to a separate header file, as R"CERT( destroys IDE Auto Format capability

#include "GxEPD2_github_raw_certs.h"

const char* certificate_rawcontent = github_io_pem;  // ok, should work until Fri, 14 Mar 2025 23:59:59 GMT

const char* host_rawcontent   = "raw.githubusercontent.com";
const char* path_rawcontent   = "/ZinggJM/GxEPD2/master/extras/bitmaps/";
const char* path_workcontent   = "/ZinggJM/GxEPD2/work_in_progress/extras/bitmaps/";
const char* path_prenticedavid   = "/prenticedavid/MCUFRIEND_kbv/master/extras/bitmaps/";
const char* path_waveshare_c  = "/waveshare/e-Paper/master/RaspberryPi_JetsonNano/c/pic/";
const char* path_waveshare_py = "/waveshare/e-Paper/master/RaspberryPi_JetsonNano/python/pic/";
const char* fp_rawcontent     = "97:D8:C5:70:0F:12:24:6C:88:BC:FA:06:7E:8C:A7:4D:A8:62:67:28"; // SHA-1 as of 12.04.2024

// note that BMP bitmaps are drawn at physical position in physical orientation of the screen
void showBitmapFrom_HTTP(const char* host, const char* path, const char* filename, int16_t x, int16_t y, bool with_color = true);
void showBitmapFrom_HTTPS(const char* host, const char* path, const char* filename, const char* fingerprint, int16_t x, int16_t y, bool with_color = true,
                          const char* certificate = certificate_rawcontent);

// draws BMP bitmap according to set orientation
void showBitmapFrom_HTTP_Buffered(const char* host, const char* path, const char* filename, int16_t x, int16_t y, bool with_color = true);
void showBitmapFrom_HTTPS_Buffered(const char* host, const char* path, const char* filename, const char* fingerprint, int16_t x, int16_t y, bool with_color = true,
                                   const char* certificate = certificate_rawcontent);

void showNative4cFrom_HTTP(const char* host, const char* path, const char* filename, int16_t x, int16_t y, bool with_color = true);
void showNative4cFrom_HTTPS(const char* host, const char* path, const char* filename, const char* fingerprint, int16_t x, int16_t y, bool with_color = true,
                            const char* certificate = certificate_rawcontent);

void showNative7cFrom_HTTP(const char* host, const char* path, const char* filename, int16_t x, int16_t y, bool with_color = true);
void showNative7cFrom_HTTPS(const char* host, const char* path, const char* filename, const char* fingerprint, int16_t x, int16_t y, bool with_color = true,
                            const char* certificate = certificate_rawcontent);

#if defined(ESP32)
// uncomment next line to use HSPI for EPD (and VSPI for SD), e.g. with Waveshare ESP32 Driver Board
//#define USE_HSPI_FOR_EPD
#endif

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
// SPIClassRP2040(spi_inst_t *spi, pin_size_t rx, pin_size_t cs, pin_size_t sck, pin_size_t tx);
// uncomment next line for use with Waveshare Pico-ePaper-2.9.
SPIClassRP2040 SPIn(spi1, 12, 13, 10, 11); // need be valid pins for same SPI channel, else fails blinking 4 long 4 short
#endif

#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
SPIClass hspi(HSPI);
#endif

void setup()
{
#if defined(ESP32) && defined(USE_HSPI_FOR_EPD)
  hspi.begin(13, 12, 14, 15); // remap hspi for EPD (swap pins)
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#endif
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
  delay(2000); // USB serial available
  display.epd2.selectSPI(SPIn, SPISettings(4000000, MSBFIRST, SPI_MODE0));
#endif

  Serial.begin(115200);
  Serial.println();
  Serial.println("GxEPD2_WiFi_Example");

  //display.init(115200); // default 10ms reset pulse, e.g. for bare panels with DESPI-C02
  display.init(115200, true, 2, false); // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse

  if (display.pages() > 1)
  {
    delay(100);
    Serial.println();
    Serial.print("pages = "); Serial.print(display.pages()); Serial.print(" page height = "); Serial.println(display.pageHeight());
    delay(1000);
  }

  Serial.println();
  WiFi.mode(WIFI_STA); // switch off AP
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  int ConnectTimeout = 60; // 30 seconds
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    Serial.print(WiFi.status());
    if (--ConnectTimeout <= 0)
    {
      Serial.println();
      Serial.println("WiFi connect timeout");
      return;
    }
  }
  Serial.println();
  Serial.println("WiFi connected");

  // Print the IP address
  Serial.println(WiFi.localIP());

  setClock();

  if ((display.epd2.panel == GxEPD2::GDEW0154Z04) || (display.epd2.panel == GxEPD2::ACeP565) || (display.epd2.panel == GxEPD2::GDEY073D46) || (display.epd2.panel == GxEPD2::GDEP073E01) || false)
  {
    //drawBitmapsBuffered_200x200();
    //drawBitmapsBuffered_other();
    //drawBitmapsBuffered_7C();
    drawBitmaps_7C();
    //drawBitmaps_7C_mixed();
  }
  else
  {
    drawBitmaps_200x200();
    drawBitmaps_other();
  }

  //drawBitmaps_test();
  //drawBitmapsBuffered_test();

  Serial.println("GxEPD2_WiFi_Example done");
}

void loop(void)
{
}

void drawBitmaps_200x200()
{
  int16_t x = (display.width() - 200) / 2;
  int16_t y = (display.height() - 200) / 2;
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "logo200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "first200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "second200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "third200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "fourth200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "fifth200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "sixth200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "seventh200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "eighth200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
}

void drawBitmaps_other()
{
  int16_t w2 = display.width() / 2;
  int16_t h2 = display.height() / 2;
  //showBitmapFrom_HTTP("www.packescape.com", "/img/assets/", "IniciMenusTV2.bmp", w2 - 200, h2 - 150, false);
  //delay(2000);
  showBitmapFrom_HTTP("www.squix.org", "/blog/wunderground/", "chanceflurries.bmp", w2 - 50, h2 - 50, false);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_prenticedavid, "betty_1.bmp", fp_rawcontent, w2 - 100, h2 - 160);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_prenticedavid, "betty_4.bmp", fp_rawcontent, w2 - 102, h2 - 126);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_prenticedavid, "marilyn_240x240x8.bmp", fp_rawcontent, w2 - 120, h2 - 120);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_prenticedavid, "miniwoof.bmp", fp_rawcontent, w2 - 60, h2 - 80);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_prenticedavid, "test.bmp", fp_rawcontent, w2 - 120, h2 - 160);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_prenticedavid, "tiger.bmp", fp_rawcontent, w2 - 160, h2 - 120);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_prenticedavid, "tiger_178x160x4.bmp", fp_rawcontent, w2 - 89, h2 - 80);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_prenticedavid, "tiger_240x317x4.bmp", fp_rawcontent, w2 - 120, h2 - 160);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_prenticedavid, "tiger_320x200x24.bmp", fp_rawcontent, w2 - 160, h2 - 100);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_prenticedavid, "tiger16T.bmp", fp_rawcontent, w2 - 160, h2 - 100);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_prenticedavid, "woof.bmp", fp_rawcontent, w2 - 100, h2 - 100);
  delay(2000);
}

void drawBitmaps_test()
{
  int16_t w2 = display.width() / 2;
  int16_t h2 = display.height() / 2;
  //showBitmapFrom_HTTPS(host_rawcontent, path_workcontent, "z0gs/screenshot.bmp", fp_rawcontent, 0, 0); delay(2000); return;
  //showNative4cFrom_HTTPS(host_rawcontent, path_workcontent, "z0gs/screenshot.bmp", fp_rawcontent, 0, 0); delay(2000); return;
  showBitmapFrom_HTTPS(host_rawcontent, path_prenticedavid, "betty_4.bmp", fp_rawcontent, w2 - 102, h2 - 126);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "output5.bmp", fp_rawcontent, 0, 0);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "output6.bmp", fp_rawcontent, 0, 0);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "tractor_1.bmp", fp_rawcontent, 0, 0);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "tractor_4.bmp", fp_rawcontent, 0, 0);
  delay(2000);
  //showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "tractor_8.bmp", fp_rawcontent, 0, 0); // format 1: BI_RLE8 is not supported
  //delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "tractor_11.bmp", fp_rawcontent, 0, 0);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "tractor_44.bmp", fp_rawcontent, 0, 0);
  delay(2000);
  showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "tractor_88.bmp", fp_rawcontent, 0, 0);
  delay(2000);
  //showBitmapFrom_HTTPS(host_rawcontent, path_rawcontent, "liberato640x384.bmp", fp_rawcontent, 0, 0);
  //delay(2000);
}

void drawBitmapsBuffered_200x200()
{
  int16_t x = (display.width() - 200) / 2;
  int16_t y = (display.height() - 200) / 2;
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "logo200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "first200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "second200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "third200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "fourth200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "fifth200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "sixth200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "seventh200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "eighth200x200.bmp", fp_rawcontent, x, y);
  delay(2000);
}

void drawBitmapsBuffered_other()
{
  int16_t w2 = display.width() / 2;
  int16_t h2 = display.height() / 2;
  showBitmapFrom_HTTP_Buffered("www.squix.org", "/blog/wunderground/", "chanceflurries.bmp", w2 - 50, h2 - 50, false);
  delay(2000);
  //showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_prenticedavid, "betty_1.bmp", fp_rawcontent, w2 - 100, h2 - 160);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_prenticedavid, "betty_4.bmp", fp_rawcontent, w2 - 102, h2 - 126);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_prenticedavid, "marilyn_240x240x8.bmp", fp_rawcontent, w2 - 120, h2 - 120);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_prenticedavid, "miniwoof.bmp", fp_rawcontent, w2 - 60, h2 - 80);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_prenticedavid, "test.bmp", fp_rawcontent, w2 - 120, h2 - 160);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_prenticedavid, "tiger.bmp", fp_rawcontent, w2 - 160, h2 - 120);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_prenticedavid, "tiger_178x160x4.bmp", fp_rawcontent, w2 - 89, h2 - 80);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_prenticedavid, "tiger_240x317x4.bmp", fp_rawcontent, w2 - 120, h2 - 160);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_prenticedavid, "tiger_320x200x24.bmp", fp_rawcontent, w2 - 160, h2 - 100);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_prenticedavid, "tiger16T.bmp", fp_rawcontent, w2 - 160, h2 - 100);
  delay(2000);
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_prenticedavid, "woof.bmp", fp_rawcontent, w2 - 100, h2 - 100);
  delay(2000);
}

void drawBitmapsBuffered_7C()
{
  if ((display.epd2.panel == GxEPD2::ACeP565) || (display.epd2.panel == GxEPD2::GDEY073D46) || (display.epd2.panel == GxEPD2::GDEP073E01))
  {
    int16_t w2 = display.width() / 2;
    int16_t h2 = display.height() / 2;
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_waveshare_c, "5in65f.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_waveshare_c, "5in65f2.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_waveshare_c, "5in65f3.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_waveshare_c, "5in65f4.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_waveshare_py, "5in65f4.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_waveshare_py, "N-Color1.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);

    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "displayed_bmp_small_but_padded.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "displayed_bmp_large.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
  }
}

void drawBitmaps_7C()
{
  if ((display.epd2.panel == GxEPD2::ACeP565) || (display.epd2.panel == GxEPD2::GDEY073D46) || (display.epd2.panel == GxEPD2::GDEP073E01))
  {
    int16_t w2 = display.epd2.WIDTH / 2;
    int16_t h2 = display.epd2.HEIGHT / 2;
    showNative7cFrom_HTTPS(host_rawcontent, path_waveshare_c, "5in65f.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_waveshare_c, "5in65f2.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_waveshare_c, "5in65f3.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_waveshare_c, "5in65f4.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_waveshare_py, "5in65f4.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_waveshare_py, "N-Color1.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);

    showNative7cFrom_HTTPS(host_rawcontent, path_rawcontent, "displayed_bmp_small_but_padded.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_rawcontent, "displayed_bmp_large.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
  }
}

void drawBitmaps_7C_mixed()
{
  if ((display.epd2.panel == GxEPD2::ACeP565) || (display.epd2.panel == GxEPD2::GDEY073D46) || (display.epd2.panel == GxEPD2::GDEP073E01))
  {
    int16_t w2 = display.epd2.WIDTH / 2;
    int16_t h2 = display.epd2.HEIGHT / 2;
    display.setRotation(2);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_waveshare_c, "5in65f.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_waveshare_c, "5in65f.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_waveshare_c, "5in65f2.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_waveshare_c, "5in65f2.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_waveshare_c, "5in65f3.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_waveshare_c, "5in65f3.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_waveshare_c, "5in65f4.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_waveshare_c, "5in65f4.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_waveshare_py, "5in65f4.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_waveshare_py, "5in65f4.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_waveshare_py, "N-Color1.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_waveshare_py, "N-Color1.bmp", fp_rawcontent, w2 - 300, h2 - 224);
    delay(2000);

    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "displayed_bmp_small_but_padded.bmp", fp_rawcontent, w2 - 400, h2 - 240);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_rawcontent, "displayed_bmp_small_but_padded.bmp", fp_rawcontent, w2 - 400, h2 - 240);
    delay(2000);
    showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_rawcontent, "displayed_bmp_large.bmp", fp_rawcontent, w2 - 400, h2 - 240);
    delay(2000);
    showNative7cFrom_HTTPS(host_rawcontent, path_rawcontent, "displayed_bmp_large.bmp", fp_rawcontent, w2 - 400, h2 - 240);
    delay(2000);
  }
}

void drawBitmapsBuffered_test()
{
  int16_t w2 = display.width() / 2;
  int16_t h2 = display.height() / 2;
  showBitmapFrom_HTTPS_Buffered(host_rawcontent, path_prenticedavid, "betty_4.bmp", fp_rawcontent, w2 - 102, h2 - 126);
  delay(2000);
}

static const uint16_t input_buffer_pixels = 800; // may affect performance
//static const uint16_t input_buffer_pixels = 960; // may affect performance

static const uint16_t max_row_width = 1872; // for up to 7.8" display 1872x1404
static const uint16_t max_palette_pixels = 256; // for depth <= 8

uint8_t input_buffer[3 * input_buffer_pixels]; // up to depth 24
uint8_t output_row_mono_buffer[max_row_width / 8]; // buffer for at least one row of b/w bits
uint8_t output_row_color_buffer[max_row_width / 8]; // buffer for at least one row of color bits
uint8_t mono_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 b/w
uint8_t color_palette_buffer[max_palette_pixels / 8]; // palette buffer for depth <= 8 c/w
uint16_t rgb_palette_buffer[max_palette_pixels]; // palette buffer for depth <= 8 for buffered graphics, needed for 7-color display

void showBitmapFrom_HTTP(const char* host, const char* path, const char* filename, int16_t x, int16_t y, bool with_color)
{
  WiFiClient client;
  bool connection_ok = false;
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  uint32_t startTime = millis();
  if ((x >= display.epd2.WIDTH) || (y >= display.epd2.HEIGHT)) return;
  Serial.println(); Serial.print("downloading file \""); Serial.print(filename);  Serial.println("\"");
  Serial.print("connecting to "); Serial.println(host);
  if (!client.connect(host, httpPort))
  {
    Serial.println("connection failed");
    return;
  }
  Serial.print("requesting URL: ");
  Serial.println(String("http://") + host + path + filename);
  client.print(String("GET ") + path + filename + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: GxEPD2_WiFi_Example\r\n" +
               "Connection: close\r\n\r\n");
  Serial.println("request sent");
  while (client.connected())
  {
    String line = client.readStringUntil('\n');
    if (!connection_ok)
    {
      connection_ok = line.startsWith("HTTP/1.1 200 OK");
      if (connection_ok) Serial.println(line);
    }
    if (!connection_ok) Serial.println(line);
    if (line == "\r")
    {
      Serial.println("headers received");
      break;
    }
  }
  if (!connection_ok) return;
  // Parse BMP header
  if (read16(client) == 0x4D42) // BMP signature
  {
    uint32_t fileSize = read32(client);
    uint32_t creatorBytes = read32(client); (void)creatorBytes; //unused
    uint32_t imageOffset = read32(client); // Start of image data
    uint32_t headerSize = read32(client);
    uint32_t width  = read32(client);
    int32_t height = (int32_t) read32(client);
    uint16_t planes = read16(client);
    uint16_t depth = read16(client); // bits per pixel
    uint32_t format = read32(client);
    uint32_t bytes_read = 7 * 4 + 3 * 2; // read so far
    if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
    {
      Serial.print("File size: "); Serial.println(fileSize);
      Serial.print("Image Offset: "); Serial.println(imageOffset);
      Serial.print("Header size: "); Serial.println(headerSize);
      Serial.print("Bit Depth: "); Serial.println(depth);
      Serial.print("Image size: ");
      Serial.print(width);
      Serial.print('x');
      Serial.println(abs(height));
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
          bytes_read += skip(client, imageOffset - (4 << depth) - bytes_read); // 54 for regular, diff for colorsimportant
          for (uint16_t pn = 0; pn < (1 << depth); pn++)
          {
            blue  = client.read();
            green = client.read();
            red   = client.read();
            client.read();
            bytes_read += 4;
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
        bytes_read += skip(client, rowPosition - bytes_read);
        for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
        {
          if (!connection_ok || !(client.connected() || client.available())) break;
          delay(1); // yield() to avoid WDT
          uint32_t in_remain = rowSize;
          uint32_t in_idx = 0;
          uint32_t in_bytes = 0;
          uint8_t in_byte = 0; // for depth <= 8
          uint8_t in_bits = 0; // for depth <= 8
          uint8_t out_byte = 0xFF; // white (for w%8!=0 border)
          uint8_t out_color_byte = 0xFF; // white (for w%8!=0 border)
          uint32_t out_idx = 0;
          for (uint16_t col = 0; col < w; col++) // for each pixel
          {
            yield();
            if (!connection_ok || !(client.connected() || client.available())) break;
            // Time to read more pixel data?
            if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
            {
              uint32_t get = in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain;
              uint32_t got = read8n(client, input_buffer, get);
              while ((got < get) && connection_ok)
              {
                //Serial.print("got "); Serial.print(got); Serial.print(" < "); Serial.print(get); Serial.print(" @ "); Serial.println(bytes_read);
                uint32_t gotmore = read8n(client, input_buffer + got, get - got);
                got += gotmore;
                connection_ok = gotmore > 0;
              }
              in_bytes = got;
              in_remain -= got;
              bytes_read += got;
              in_idx = 0;
            }
            if (!connection_ok)
            {
              Serial.print("Error: got no more after "); Serial.print(bytes_read); Serial.println(" bytes read!");
              break;
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
          int16_t yrow = y + (flip ? h - row - 1 : row);
          display.writeImage(output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);
        } // end line
        Serial.print("downloaded in ");
        Serial.print(millis() - startTime);
        Serial.println(" ms");
        display.refresh();
      }
      Serial.print("bytes read "); Serial.println(bytes_read);
    }
  }
  client.stop();
  if (!valid)
  {
    Serial.println("bitmap format not handled.");
  }
}

void showBitmapFrom_HTTPS(const char* host, const char* path, const char* filename, const char* fingerprint, int16_t x, int16_t y, bool with_color, const char* certificate)
{
  // Use WiFiClientSecure class to create TLS connection
#if defined (ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
  BearSSL::WiFiClientSecure client;
  BearSSL::X509List cert(certificate ? certificate : certificate_rawcontent);
#else
  WiFiClientSecure client;
#endif
  bool connection_ok = false;
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  uint32_t startTime = millis();
  if ((x >= display.epd2.WIDTH) || (y >= display.epd2.HEIGHT)) return;
  Serial.println(); Serial.print("downloading file \""); Serial.print(filename);  Serial.println("\"");
  Serial.print("connecting to "); Serial.println(host);
#if defined (ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
  if (certificate) client.setTrustAnchors(&cert);
  else if (fingerprint) client.setFingerprint(fingerprint);
  else client.setInsecure();
#elif defined (ESP32)
  if (certificate) client.setCACert(certificate);
#endif
  if (!client.connect(host, httpsPort))
  {
    Serial.println("connection failed");
    return;
  }
  Serial.print("requesting URL: ");
  Serial.println(String("https://") + host + path + filename);
  client.print(String("GET ") + path + filename + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: GxEPD2_WiFi_Example\r\n" +
               "Connection: close\r\n\r\n");
  Serial.println("request sent");
  while (client.connected())
  {
    String line = client.readStringUntil('\n');
    if (!connection_ok)
    {
      connection_ok = line.startsWith("HTTP/1.1 200 OK");
      if (connection_ok) Serial.println(line);
    }
    if (!connection_ok) Serial.println(line);
    if (line == "\r")
    {
      Serial.println("headers received");
      break;
    }
  }
  if (!connection_ok) return;
  // Parse BMP header
  uint16_t signature = 0;
  for (int16_t i = 0; i < 50; i++)
  {
    if (!client.available()) delay(100);
    else signature = read16(client);
    if (signature == 0x4D42) break;
  }
  if (signature == 0x4D42) // BMP signature
  {
    uint32_t fileSize = read32(client);
    uint32_t creatorBytes = read32(client); (void)creatorBytes; //unused
    uint32_t imageOffset = read32(client); // Start of image data
    uint32_t headerSize = read32(client);
    uint32_t width  = read32(client);
    int32_t height = (int32_t) read32(client);
    uint16_t planes = read16(client);
    uint16_t depth = read16(client); // bits per pixel
    uint32_t format = read32(client);
    uint32_t bytes_read = 7 * 4 + 3 * 2; // read so far
    if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
    {
      Serial.print("File size: "); Serial.println(fileSize);
      Serial.print("Image Offset: "); Serial.println(imageOffset);
      Serial.print("Header size: "); Serial.println(headerSize);
      Serial.print("Bit Depth: "); Serial.println(depth);
      Serial.print("Image size: ");
      Serial.print(width);
      Serial.print('x');
      Serial.println(abs(height));
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
          //bytes_read += skip(client, 54 - bytes_read); //palette is always @ 54
          bytes_read += skip(client, imageOffset - (4 << depth) - bytes_read); // 54 for regular, diff for colorsimportant
          for (uint16_t pn = 0; pn < (1 << depth); pn++)
          {
            blue  = client.read();
            green = client.read();
            red   = client.read();
            client.read();
            bytes_read += 4;
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
        //Serial.print("skip "); Serial.println(rowPosition - bytes_read);
        bytes_read += skip(client, rowPosition - bytes_read);
        for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
        {
          if (!connection_ok || !(client.connected() || client.available())) break;
          delay(1); // yield() to avoid WDT
          uint32_t in_remain = rowSize;
          uint32_t in_idx = 0;
          uint32_t in_bytes = 0;
          uint8_t in_byte = 0; // for depth <= 8
          uint8_t in_bits = 0; // for depth <= 8
          uint8_t out_byte = 0xFF; // white (for w%8!=0 border)
          uint8_t out_color_byte = 0xFF; // white (for w%8!=0 border)
          uint32_t out_idx = 0;
          for (uint16_t col = 0; col < w; col++) // for each pixel
          {
            yield();
            if (!connection_ok || !(client.connected() || client.available())) break;
            // Time to read more pixel data?
            if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
            {
              uint32_t get = in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain;
              uint32_t got = read8n(client, input_buffer, get);
              while ((got < get) && connection_ok)
              {
                //Serial.print("got "); Serial.print(got); Serial.print(" < "); Serial.print(get); Serial.print(" @ "); Serial.println(bytes_read);
                //if ((get - got) > client.available()) delay(200); // does improve? yes, if >= 200
                uint32_t gotmore = read8n(client, input_buffer + got, get - got);
                got += gotmore;
                connection_ok = gotmore > 0;
              }
              in_bytes = got;
              in_remain -= got;
              bytes_read += got;
              in_idx = 0;
            }
            if (!connection_ok)
            {
              Serial.print("Error: got no more after "); Serial.print(bytes_read); Serial.println(" bytes read!");
              break;
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
          int16_t yrow = y + (flip ? h - row - 1 : row);
          display.writeImage(output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);
        } // end line
        Serial.print("downloaded in "); Serial.print(millis() - startTime); Serial.println(" ms");
        Serial.print("bytes read "); Serial.println(bytes_read);
        display.refresh();
      }
    }
  }
  client.stop();
  if (!valid)
  {
    Serial.println("bitmap format not handled.");
  }
}

#include "GxEPD2_WiFi_Buffered.h"

#include "GxEPD2_WiFi_Native4c.h"
#include "GxEPD2_WiFi_Native7c.h"

uint16_t read16(WiFiClient& client)
{
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read(); // MSB
  return result;
}

uint32_t read32(WiFiClient& client)
{
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = client.read(); // LSB
  ((uint8_t *)&result)[1] = client.read();
  ((uint8_t *)&result)[2] = client.read();
  ((uint8_t *)&result)[3] = client.read(); // MSB
  return result;
}

#if USE_BearSSL

uint32_t skip(BearSSL::WiFiClientSecure& client, int32_t bytes)
{
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0))
  {
    if (client.available())
    {
      client.read();
      remain--;
    }
    else delay(10);
    if (millis() - start > 2000) break; // don't hang forever
  }
  return bytes - remain;
}

uint32_t read8n(BearSSL::WiFiClientSecure& client, uint8_t* buffer, int32_t bytes)
{
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0))
  {
    if (client.available())
    {
      int16_t v = client.read();
      *buffer++ = uint8_t(v);
      remain--;
    }
    else delay(10);
    if (millis() - start > 2000) break; // don't hang forever
  }
  return bytes - remain;
}

#endif

uint32_t skip(WiFiClient& client, int32_t bytes)
{
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0))
  {
    if (client.available())
    {
      client.read();
      remain--;
    }
    else delay(1);
    if (millis() - start > 2000) break; // don't hang forever
  }
  return bytes - remain;
}

uint32_t read8n(WiFiClient& client, uint8_t* buffer, int32_t bytes)
{
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0))
  {
    if (client.available())
    {
      int16_t v = client.read();
      *buffer++ = uint8_t(v);
      remain--;
    }
    else delay(1);
    if (millis() - start > 2000) break; // don't hang forever
  }
  return bytes - remain;
}

// Set time via NTP, as required for x.509 validation
void setClock()
{
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}
