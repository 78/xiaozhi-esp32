# GxEPD2
## Arduino Display Library for SPI E-Paper Displays

- With full Graphics and Text support using Adafruit_GFX

- For SPI e-paper displays from Dalian Good Display 
- and SPI e-paper boards from Waveshare

### important note:
- the display panels are for 3.3V supply and 3.3V data lines
- never connect data lines directly to 5V Arduino data pins, use e.g. 4k7/10k resistor divider
- series resistor only is not enough for reliable operation (back-feed effect through protection diodes)
- 4k7/10k resistor divider may not work with flat cable extensions or Waveshare 4.2 board, use level converter then
- do not forget to connect GND
- the actual Waveshare display boards now have level converters and series regulator, safe for 5V
- use 3k3 pull-down on SS for ESP8266 for boards with level converters
- note that 7.5" e-paper displays don't work reliable if fed from 3.3V Arduino pin
- note that Waveshare boards with "clever" reset circuit may need shortened reset pulse
- use `init(115200, true, 2, false)` for Waveshare boards with "clever" reset circuit
- note that Waveshare boards with "clever" reset circuit need 1k pull-up on RST on ESP8266, or different pin
- note that the new Waveshare Universal e-Paper Raw Panel Driver HAT Rev 2.3 needs PWR connected to VCC or driven HIGH
- see https://www.waveshare.com/wiki/E-Paper_Driver_HAT

### Paged Drawing, Picture Loop
 - This library uses paged drawing to limit RAM use and cope with missing single pixel update support
 - buffer size can be selected in the application by template parameter page_height, see GxEPD2_Example
 - Paged drawing is implemented as picture loop, like in U8G2 (Oliver Kraus)
 - see https://github.com/olikraus/u8glib/wiki/tpictureloop
 - Paged drawing is also available using drawPaged() and drawCallback(), like in GxEPD
- ` // GxEPD style paged drawing; drawCallback() is called as many times as needed `
- ` void drawPaged(void (*drawCallback)(const void*), const void* pv) `
- paged drawing is done using Adafruit_GFX methods inside picture loop or drawCallback

### Full Screen Buffer Support
 - full screen buffer is selected by setting template parameter page_height to display height
 - drawing to full screen buffer is done using Adafruit_GFX methods without picture loop or drawCallback
 - and then calling method display()

### Low Level Bitmap Drawing Support
 - bitmap drawing support to the controller memory and screen is available:
 - either through the template class instance methods that forward calls to the base display class
 - or directly using an instance of a base display class and calling its methods directly

### Supporting Arduino Forum Topics:

- Waveshare e-paper displays with SPI: http://forum.arduino.cc/index.php?topic=487007.0
- Good Display ePaper for Arduino : https://forum.arduino.cc/index.php?topic=436411.0
- Note that these topics are closed. Use only for reference.
- The Arduino Forum is no longer watched and answered by the Author.
- New questions or issues should be posted on https://github.com/ZinggJM/GxEPD2/discussions

### Note on documentation
- GxEPD2 uses Adafruit_GFX for Graphics and Text support, which is well documented there
- GxEPD2 uses meaningful method names, and has some comments in the header files
- consult the header files GxEPD2_BW.h, GxEPD2_3C.h and GxEPD2_GFX.h
- for the concept of paged drawing and picture loop see: 
- https://github.com/olikraus/u8glib/wiki/tpictureloop

### Note on issues and pull requests
- issues should be reported in https://github.com/ZinggJM/GxEPD2/discussions.
- issues on GitHub are disabled; there were too many false issues.
- pull requests are not welcome, will not be merged.
- pull requests can't be disabled, but will be closed.
- please place information about interesting fork additions in https://github.com/ZinggJM/GxEPD2/discussions.

### Supported SPI e-paper panels from Good Display:
- GDEW0102T4     1.02" b/w 80x128, UC8175
- DEPG0150BN     1.50" b/w 200x200, SSD1681, e.g. LILYGO® TTGO T5 V2.4.1 1.50 inch
- GDEP015OC1     1.54" b/w 200x200, IL3829, no longer available
- GDEH0154D67    1.54" b/w 200x200, SSD1681, replacement for GDEP015OC1
- GDEW0154T8     1.54" b/w 152x152, UC8151 (IL0373)
- GDEW0154M09    1.54" b/w 200x200, JD79653A
- GDEW0154M10    1.54" b/w 152x152, UC8151D, DES
- GDEY0154D67    1.54" b/w 200x200, SSD1681
- GDEW0154Z04    1.54" b/w/r 200x200, IL0376F, no longer available
- GDEH0154Z90    1.54" b/w/r 200x200, SSD1681, replacement for GDEW0154Z04
- GDE0213B1      2.13" b/w 122x250, IL3895, phased out
- GDEH0213B72    2.13" b/w 122x250, SSD1675A (IL3897), replacement for GDE0213B1
- GDEH0213B73    2.13" b/w 122x250, SSD1675B, new replacement for GDE0213B1, GDEH0213B72
- GDEM0213B74    2.13" b/w 122x250, SSD1680
- GDEW0213I5F    2.13" b/w 104x212, UC8151 (IL0373), flexible
- GDEW0213T5D    2.13" b/w 104x212, UC8151 (IL0373)
- GDEW0213M21    2.13" b/w 104x212, UC8151 (IL0373), DES
- GDEW0213Z16    2.13" b/w/r 104x212, UC8151 (IL0373)
- GDEW0213Z19    2.13" b/w/r 104x212, UC8151D
- GDEY0213Z98    2.13" b/w/r 122x250, SSD1680
- GDEY0213F51    2.13" 4-color 122x250, JD79661
- DEPG0213BN     2.13" b/w 122x250, SSD1680, e.g. LILYGO® TTGO T5 V2.3.1 2.13 inch
- GDEY0213B74    2.13" b/w 122x250, SSD1680
- GDEW026T0      2.6" b/w 152x296, UC8151 (IL0373)
- GDEW026M01     2.6" b/w 152x296, UC8151 (IL0373), DES
- DEPG0266BN     2.66" b/w 152x296, SSD1680, e.g. LILYGO® TTGO T5 2.66 inch
- GDEY0266Z90    2.66" b/w/r 152x296, SSD1680
- GDEY0266F51H   2.66" 4-color 184x460, JD79667
- GDEW027C44     2.7" b/w/r 176x264, IL91874
- GDEW027W3      2.7" b/w 176x264, EK79652 (IL91874)
- GDEY027T91     2.7" b/w 176x264, SSD1680
- GDEH029A1      2.9" b/w 128x296, SSD1608 (IL3820)
- GDEW029T5      2.9" b/w 128x296, UC8151 (IL0373)
- GDEW029T5D     2.9" b/w 128x296, UC8151D
- GDEW029I6FD    2.9" b/w 128x296, UC8151D, flexible
- GDEM029T94     2.9" b/w 128x296, SSD1680
- GDEW029M06     2.9" b/w 128x296, UC8151D, DES
- GDEW029Z10     2.9" b/w/r 128x296, UC8151 (IL0373)
- GDEH029Z13     2.9" b/w/r 128x296, UC8151D
- GDEM029C90     2.9" b/w/y 128x296, SSD1680
- DEPG0290BS     2.9" b/w 128x296, SSD1680, e.g. LILYGO® TTGO T5 V2.4.1 2.9"
- GDEY029T94     2.9" b/w 128x296, SSD1680
- GDEY029T71H    2.9" b/w 168x384, SSD1685
- GDEY029F51H    2.9" 4-color 168x384, JD79667
- Waveshare3inch4color 3.0" 4-color 168x400
- GDEQ031T10     3.1" b/w 240x320, UC8253
- GDEY037T03     3.7" b/w 240x416, UC8253
- ED037TC1       3.7" b/w 280x480, SSD1677, Waveshare 3.7"
- GDEW0371W7     3.7" b/w 240x416, UC8171 (IL0324)
- GDEW042T2      4.2" b/w 400x300, UC8176 (IL0398)
- GDEW042M01     4.2" b/w 400x300, UC8176 (IL0398), DES
- GDEW042Z15     4.2" b/w/r 400x300, UC8176 (IL0398)
- GDEQ042Z21     4.2" b/w/r 400x300, UC8276, (Waveshare V2)
- GDEY042Z98     4.2" b/w/r 400x300, SSD1683
- GDEY042T81     4.2" b/w 400x300, SSD1683
- GDEY0420F51    4.2" 4-color 400x300, HX8717
- GDEQ0426T82    4.26" b/w 800x480, SSD1677
- Waveshare437inch4color Waveshare 4.37" 4-color e-paper display 512x368 
- ACeP565        5.65" Waveshare 5.65" 7-color e-paper display 600x448
- GDEP0565D90    5.65" 7-color 600x448
- GDEY0579T93    5.79" b/w 792x272, SSD1683
- GDEY0579Z93    5.79" b/w/r 792x272, SSD1683
- GDEY0579F51    5.79" 4-color 792x272, HX8717
- GDEW0583T7     5.83" b/w 600x448, UC8159c (IL0371)
- GDEW0583T8     5.83" b/w 648x480, EK79655 (GD7965)
- GDEW0583Z83    5.83" b/w/r 648x480, EK79655 (GD7965)
- GDEQ0583T31    5.83" b/w 648x480, UC8179
- GDEQ0583Z31    5.83" b/w/r 648x480, UC8179C
- GDEY073D46     7.3" 800x480 7-color
- GDEP073E01     7.3" 800x480 7-color
- ACeP730        7.3" Waveshare 7-color e-paper display 800x480, PhotoPainter
- GDEW075T8      7.5" b/w 640x384, UC8159c (IL0371)
- GDEW075T7      7.5" b/w 800x480, EK79655 (GD7965)
- GDEY075T7      7.5" b/w 800x480, UC8179 (GD7965)
- GDEW075Z09     7.5" b/w/r 640x384, UC8159c (IL0371)
- GDEW075Z08     7.5" b/w/r 800x480, EK79655 (GD7965)
- GDEH075Z90     7.5" b/w/r 880x528, SSD1677
- GDEM102T91    10.2" b/w 960x640, SSD1677
- GDEM1085T51   10.85 b/w 1360x480, JD79686AB
- GDEH116T91    11.6" b/w 960x640, SSD1677
- GDEY116Z91    11.6" b/w/r 960x640, SSD1677
- GDEY116F51    11.6" 4-color 960x640, SSD2677
- GDEW1248T3    12.48" b/w 1304x984, UC8179
- GDEY1248Z51   12.48" b/w/r 1304x984, UC8179
- GDEM133T91    13.3" b/w 960x680, SSD1677
- GDEM133Z91    13.3" b/w/r 960x680, SSD1677
#### Supported SPI e-paper panels & boards from Waveshare: compare with Good Display, same panel
#### other supported panels
- GYE042A87     4.2" b/w 400x300, SSD1683 (HINK-E042-A07-FPC-A1)
- SE0420NQ04    4.2" b/w 400x300, UC8276C (OPM042A2_V1.0)
- ED060SCT        6" grey levels, on Waveshare e-Paper IT8951 Driver HAT
- ED060KC1        6" grey levels, 1448x1072, on Waveshare e-Paper IT8951 Driver HAT
- ED078KC2      7.8" grey levels, 1872x1404, on Waveshare e-Paper IT8951 Driver HAT
- ES103TC1     10.3" grey levels, 1872x1404, on Waveshare e-Paper IT8951 Driver HAT

### I can and will only support e-paper panels I have!
- I accept panel donations to add support to GxEPD2 only on exception.
- I support only panels from known sources that are well documented.
- Let me know about interesting panels that fall into this category.
- I occasionally buy new panels, but adding support will take as much time as needed.

### Version 1.6.4
- added support for GDEY037T03 3.7" b/w 240x416, UC8253
#### Version 1.6.3
- updated GxEPD2_4C.h with WIDTH_VISIBLE (for GDEY0213F51)
- updated GxEPD2_154_Z90c.cpp powerOff() handling
- updated powerOff() handling for other 3-color displays
#### Version 1.6.2
- fixed support for GDEY029T71H 2.9" b/w 168x384, SSD1685
- GxEPD2_WiFi_Example: added direct (non-buffered) BMP download to 7-color displays
- some minor fixes
#### Version 1.6.1
- fixed and updated GxEPD2_WiFi_Example
#### Version 1.6.0
- updated support for GDEY029T94: fixed a partial refresh issue
- updated support for GDEY042T81: fixed fast full refresh for 2024 panel version
- updated support for GDEP073E01: fixed color mapping for its native color values
- updated GxEPD2_display_selection_new_style.h: added Arduino Nano Every, Arduino Nano ESP32, Arduino Nano IoT.
#### Version 1.5.9
- added Power Save command (PWS) to GDEY075T7 init (issue with dithered bitmaps)
- added support for GDEY0579T93 5.79" b/w 792x272, SSD1683
- added support for GDEY0579Z93 5.79" b/w/r 792x272, SSD1683
- added support for GDEY0579F51 5.79" 4-color 792x272, HX8717
- the GDEY0579F51 has partial window addressing capability, 
- but of little use, because the controller buffer is modified during refresh
- partial window refresh can't be used on GDEY0579F51, refresh is full screen
- added support for GDEY042Z98 4.2" b/w/r 400x300, SSD1683
- GDEY042Z98 has "fast" full refresh and supports fast b/w partial refresh
- added support for GDEP073E01 7.3" 800x480 7-color
#### Version 1.5.8
- added support for GDEP0565D90 5.65" 7-color 600x448
- added support for GDEY116F51 11.6"  4-color 960x640, SSD2677
- added support for GDEY029T71H 2.9" b/w 168x384, SSD1685
#### Version 1.5.7
- added support for GDEY0213F51 2.13" 4-color 122x250, JD79661
- added support for GDEM1085T51 10.85 b/w 1360x480, JD79686AB
#### Version 1.5.6
- version for ongoing fixes and additions
- added support for GYE042A87  4.2" b/w 400x300
- added support for SE0420NQ04 4.2" b/w 400x300
- added support for GDEQ0583Z31 5.83" b/w/r 648x480
- added support for GDEM102T91 10.2" b/w 960x640, SSD1677
- added support for GDEY116Z91 11.6" b/w/r 960x640, SSD1677
- added support for GDEM133T91 13.3" b/w 960x680, SSD1677
- added support for GDEM133Z91 13.3" b/w/r 960x680, SSD1677
- improved differential refresh for SE0420NQ04
- improved full refresh for SE0420NQ04
- fixed driver class GxEPD2_213.cpp
#### Version 1.5.5
- updated 7-color driver classes (design cleanup fixed)
- note: Waveshare PhotoPainter doesn't work with MBED Pico package
- note: use package https://github.com/earlephilhower/arduino-pico for PhotoPainter
#### Version 1.5.4
- added support for GDEQ031T10 3.1" b/w 240x320
- updated support for GDEY075T7 7.5" b/w 800x480
- renamed driver class GxEPD2_750_YT7 to GxEPD2_750_GDEY075T7
- the actual GDEY075T7 panels have fast full refresh capability
- added support for Waveshare 7.3" 7-color, driver class GxEPD2_730c_ACeP_730
- added support for Waveshare 7.3" 7-color PhotoPainter with RPi Pico RP2040
- updated b/w driver classes for SSD controllers
- updated 7-color driver classes (design cleanup)
#### Version 1.5.3
- added support for GDEQ0426T82 4.26" b/w 800x480
- added support for GDEY0266F51H 2.66" 4-color 184x460
- added support for GDEY029F51H 2.9" 4-color 168x384
- added support for GDEY0420F51 4.2" 4-color 400x300
- added support for Waveshare 3.00" 4-color e-paper display 168x400
- updated support for Waveshare 4.37" 4-color e-paper display 512x368
- the 4-color displays have partial window addressing and refresh
- renamed GDEY042T91 to its new official name GDEY042T81
#### Version 1.5.2
- added support for Waveshare 4.37" 4-color e-paper display 512x368
- fixed __has_include test (for packages that don't support __has_include)
- added WIDTH_VISIBLE for correct graphics width for 2.13" b/w 122x250
- added end() method, to release SPI and reset all pins to INPUT
#### Version 1.5.1
- added support for GDEY0154D67 1.54" b/w 200x200
- added support for GDEY0213B74 2.13" b/w 128x250
- updated support for GDEY027T91 2.7" b/w 176x264
- renamed driver class for GDEY027T91 to GxEPD2_270_GDEY027T91
- added support for GDEY029T94 2.9" b/w 128x296
- added support for GDEY042T91 4.2" b/w 400x300
- added support for GDEQ0583T31 5.83" b/w 648x480
- these panels have fast full refresh capability
- fast full refresh can be disabled in the class header for low temperature use
- added support for GDEY073D46 7.3" 800x480 7-color
- all these new panels have been donated by Good Display
- added __has_include test to GxEPD2_BW.h, GxEPD2_3C.h, GxEPD2_7C
- this allows to remove or put aside unused driver classes for faster compile
- several fixes and improvements
#### Version 1.5.0
- added support for GDEY075T7 7.5" b/w 800x480
- updated GxEPD2_Example, added HSPI use for Waveshare ESP32 Driver Board
- updated GxEPD2_SD_AVR_Example, fixes for BMP handling
- updated GxEPD2_SD_Example, fixes for BMP handling, added HSPI use for Waveshare ESP32 Driver Board
- updated GxEPD2_SerialFlash_Example, fixes for BMP handling
- updated GxEPD2_SerialFlash_Loader, updated https certificates and use
- updated GxEPD2_Spiffs_Example, fixes for BMP handling
- updated GxEPD2_Spiffs_Loader, updated https certificates and use
- added GxEPD2_WiFi_CertStore_Example, use of a BearSSL CertStore on ESP8266, like a browser
- added GxEPD2_WiFi_CertStore_Loader, download root certificates (from Mozilla) to ESP8266 flash
- updated GxEPD2_WiFi_Example, fixes for BMP handling, updated https certificates and use
- updated GxEPD2_WS_ESP32_Driver, added HSPI use for Waveshare ESP32 Driver Board
#### Version 1.4.9
- added missing constructor lines to GxEPD2_display_selection.h
- added panel details to comments in GxEPD2_display_selection.h
- added panel details to Supported SPI e-paper panels in README.md
#### Version 1.4.8
- added support for GDEY027T91 2.7" b/w 176x264
- added support for GDEY1248Z51 12.48" b/w/r 1304x984
- fixed name of panel DEPG0150BN
- updated certificate and fingerprint for GitHub download in GxEPD2_WiFi_Example
#### Version 1.4.7
- added support for DEPG0150BN 1.50" b/w 200x200, e.g. LILYGO® TTGO T5 V2.4.1 1.50 inch
- added support for DEPG0213BN 2.13" b/w 128x250, e.g. LILYGO® TTGO T5 V2.3.1 2.13 inch
- added support for DEPG0266BN 2.66" b/w 152x296, e.g. LILYGO® TTGO T5 2.66 inch
- added support for DEPG0290BS 2.9"  b/w 128x296, e.g. LILYGO® TTGO T5 V2.4.1 2.9"
- added support for ES103TC1 10.3" grey levels, 1872x1404, on Waveshare e-Paper IT8951 Driver HAT
#### Version 1.4.6
- added support for GDEY0266Z90 2.66" b/w/r 152x296
- added support for GDEW0583Z83 5.83" b/w/r 648x460
#### Version 1.4.5
- added support for GDEY0213Z98 2.13" b/w/r 122x250
- tested with LOLIN 2.13" Tri-Color eInk / ePaper 250x122 Display Shield
- https://www.aliexpress.com/item/1005003020667903.html
#### Version 1.4.4
- interim update to solve compiler warnings (ALL for GCC for AVR)
#### Version 1.4.3
- added option to select an alternate HW SPI channel and/or SPI settings
- by method selectSPI(SPIClass& spi, SPISettings spi_settings) of driver base class GxEPD2_EPD
- by calling selectSPI before calling init() of display class
- or by calling extended init() method that has these parameters added
- tested with RPi Pico RP2040 using Good Display DESPI-PICO connection shield
- updated GxEPD2_Example to show use with DESPI-PICO
- DESPI-PICO: see https://www.good-display.com/product/393.html
#### Version 1.4.2
- added support for Waveshare 3.7" b/w board and panel ED037TC1
- waveform tables for ED037TC1 are taken unmodified from Waveshare demo code
- refresh behavior with ED037TC1 with GxEPD2 is not perfect, could not be resolved
- ISSUE RESOLVED: RESE needs to be 3 ohms on DESPI-C02 for this panel, Waveshare schematics is wrong
- added differential refresh for GDEH116T91, waveform table taken from ED037TC1
- differential refresh for GDEH116T91 looks ok; can be disabled with hasFastPartialUpdate = false
- increased sustain phase for differential refresh on GDEW042T2
#### Version 1.4.1
- fix for Waveshare "clever" reset circuit: power controller before reset pulse
#### Version 1.4.0
- changed the default reset duration to 10ms instead of 20ms
- changed the delay after reset to 10ms or reset duration, whichever is higher, instead of 200ms
- added a busyCallback feature, to allow to service periodic actions during wait for BUSY termination
- ` // register a callback function to be called during _waitWhileBusy continuously. `
- ` void setBusyCallback(void (*busyCallback)(const void*), const void* busy_callback_parameter = 0); `
- added example GxEPD2_RotaryUsingBusyCallback.ino
#### Version 1.3.9
- fix for STM32 official package pin number range (int16_t)
- fix for refresh(int16_t x, int16_t y, int16_t w, int16_t h) methods screen intersection
#### Version 1.3.8
- added support for GDEQ042Z21 4.2" b/w/r 400x300 panel or Waveshare 4.2" V2 b/w/r board
- the Waveshare 4.2" V2 b/w/r board requires the shortened reset pulse
- added support for GDEW029I6FD 2.9" b/w 128x296 flexible board
- some fixes and improvement for the new style panel selection
#### Version 1.3.7
- added support for GDEW0102T4 1.02" b/w 80x128 panel or Waveshare 1.02" b/w board
- the GDEW0102T4 panel has a different connector than the other SPI panels
- the GDEW0102T4 panel can be used with the DESPI-C102 connection module
- https://buy-lcd.com/products/connection-board-demo-kit-for-102-inch-e-ink-display-despi-c102
- my test have been done with the Waveshare 1.02" b/w board
#### Version 1.3.6
- added support for Waveshare 2.9" b/w V2, driver class GxEPD2_290_T94_V2
- Waveshare 2.9" b/w V2 uses a GDEM029T94 variant without partial update wft in OTP
- driver class GxEPD2_290_T94_V2 uses partial update wft written to registers
- added NOTE for RST pull-up on ESP8266 with "clever" reset circuit, or alternate pin
#### Version 1.3.5
- added support for GDEM0213B74 122x250 b/w e-paper panel
- added support for ED078KC2, 7.8" grey levels, 1872x1404, on Waveshare e-Paper IT8951 Driver HAT
#### Version 1.3.4
- added support for GDEH116T91 960x640 b/w e-paper panel
- GDEH116T91 has only full screen refresh, no wavetable for differential refresh yet
- added support for processor Arduino Nano RP2040 Connect to the examples
- added general fast b/w refresh for capable 3-color displays GDEW0213Z19, GDEW029Z13
- added example GxEPD2x_FastBlackWhiteOnColor.ino for GDEW0213Z19, GDEW029Z13
- evaluation of other fast b/w capable 3-color panels may follow
#### Version 1.3.3
- added b/w differential refresh method to GDEW0213Z19
- added b/w differential refresh method to GDEW029Z13
- up to 100 b/w fast refreshes have been possible (resulting in slightly reddish background)
- added experimental example GxEPD2x_MixedTest for proof of concept
- GxEPD2x_MixedTest needs enough RAM for 2 GFXCanvas1 objects
- general 3-color mixed refresh in GxEPD2 is in evaluation, for capable panels
#### Version 1.3.2
- added support for GDEW0213T5D 104x212 b/w e-paper panel
- added support for GDEW029T5D  128x296 b/w e-paper panel
- added support for GDEW0213Z19 104x212 b/w/r e-paper panel
- added support for GDEW029Z13  128x296 b/w/r e-paper panel
- both GDEW0213Z19 and GDEW029Z13 support only full screen refresh (controller issue)
#### Version 1.3.1
- added support for GDEM029C90 128x296 b/w/y e-paper panel
- GDEM029C90 has a very long refresh time of about 27 seconds
- GDEM029C90 has partial update (window addressing) of controller memory
- GDEM029C90 supports only full refresh of screen
#### Version 1.3.0
- added support for GDEM029T94 128x296 b/w e-paper panel
- added support for GDEW026M01 152x296 b/w new DES e-paper panel
- the new GDEW026M01 DES b/w e-paper panel has higher contrast and wide temperature range
- differential refresh (fast partial update) is supported for normal temperature range
- disable differential refresh for extended temperatures by setting hasFastPartialUpdate = false;
- the differential waveform table for GDEW026M01 is experimental and may need improvement
- major update of display selection in the examples to make additions easier:
- old style selection is now in separate include files GxEPD2_display_selection.h 
- and GxEPD2_display_selection_added.h
- added new style display selection in include file GxEPD2_display_selection_new_style.h
- either selection style can be used. old style is intended for easy copying of constructor lines.
#### Version 1.2.16
- fix for GDEH0154D67, to work independent of OTP version (ping-pong issue)
#### Version 1.2.15
- added support for GDEW0213M21 2.13" 104x212 b/w new DES e-paper panel
- added support for GDEW029M06 2.9" 128x296 b/w new DES e-paper panel
- added support for GDEW042M01 4.2" 400x300 b/w new DES e-paper panel
- these new DES b/w e-paper panels have higher contrast and wide temperature range
- differential refresh (fast partial update) is supported for normal temperature range
- disable differential refresh for extended temperatures by setting hasFastPartialUpdate = false;
- the GDEW042M01 I have is a preliminary version, differential waveform may need update
#### Version 1.2.14
- added support for 6" 1448x1072 ED060KC1 on Waveshare e-Paper IT8951 Driver HAT
- fixed GxEPD2_WiFi_Example.ino: new GitHub fingerprint for ESP8266, certificate for ESP32
- fixed color discriminator for 7-color display in GxEPD2_7C.h
- added multicolor support for 7-color display in GxEPD2_WiFi_Example, GxEPD2_Spiffs_Example,
- and GxEPD2_SD_Example, GxEPD2_SerialFlash_Example
- fixed paged write to 7-color display controller (transaction and CS handling issue)
- some more fixes
#### Version 1.2.13
- added support for GDEH0154Z90 1.54" 200x200 b/w/r, replacement for GDEW0154Z04
- GDEH0154Z90 has partial window addressing, but no partial window refresh
- added some missing constructor examples, e.g. for IT8951 Driver HAT on Due
- cleaned-up wiring information in the examples
#### Version 1.2.12
- fixed GxEPD2_MinimumExample.ino to draw black on white (white on white was invisible)
- added example GxEPD2_HelloWorld.ino
- updated wiring info and constructor parameters for the actual Waveshare e-Paper ESP8266 Driver Board
- added some missing explicit initializers (nobody noticed as compilers usually initialize simple members)
#### Version 1.2.11
- added support for Waveshare 5.65" 7-color e-paper display 600x448
- the unknown controller of this display doesn't support partial update, as far as known
- 7-color graphics support is available with class GxEPD2_7C
- 7-color graphics support requires paging even on ESP32
- 7-color graphics support causes heavy paging on Arduino UNO, gets very slow
- refresh is quite fast for a color e-paper display: 12 seconds
#### Version 1.2.10
- added support for GDEH075Z90 7.5" b/w/r 880x528
- the controller of GDEH075Z90 supports partial update, but refesh is full screen
- the controller of GDEH075Z90 doesn't support differential update (not possible on 3-color anyway)
- note: the connector of  the GDEH075Z90 is mirrored; connects downward on DESPI-C02
- added optional init parameter "reset_duration" in ms, same default 20ms as before
- reset_duration = 2 may help with "clever" reset circuit of newer boards from Waveshare
#### Version 1.2.9
- added support for GDEW0154T8 1.54" b/w 152x152
- added support for GDEW0154M09 1.54" b/w 200x200
- added support for GDEW0154M10 1.54" b/w 152x152 DES
- added support for GDEW0583T8 5.83" b/w 648x460
- added option to use class GFX of library GFX_Root instead of Adafruit_GFX
- differential update on GDEW0154M10 is experimental, there is no partial update demo yet
#### Version 1.2.8
- improved transfer speed for the bigger displays (3.71" and up)
#### Version 1.2.7
- fix paged drawing of initial screen on GDEH029A1
- sometimes true is false, if applied the wrong way round
#### Version 1.2.6
- slightly improved differential refresh for GDEW1248T3
- minor fixes
#### Version 1.2.5
- added support for GDEW1248T3 12.84" b/w panel on Waveshare 12.48 e-paper display frame
- tested with Waveshare Epaper ESP32 Driver Board mounted on connection board
- tested with Arduino UNO mounted on connection board, is extremely slow
- doesn't work with Arduino MEGA on connection board with switch set to ICSP and pins bent out of way, reason unknown
- not tested with other processors
- caution: wiring connector is on 5V side of level converters!
#### Version 1.2.4
- added support for GDEH0154D67 1.54" b/w, replacement for GDEP015OC1
- added GxEPD2_SerialFlash_Loader, WiFi bitmap downloader for SPI-flash
- added GxEPD2_SerialFlash_Example, SPI-flash example, e.g. for Winbond 25Q16BVSIG
- minor fixes and comment cleanups
- fix refresh(false) in upper layer: add powerOff() after full refresh
#### Version 1.2.3
- fixed partial update for 2.13" 3-color and 2.9" 3-color e-paper
- partial update can be disabled with attribute usePartialUpdateWindow = false
- added GxEPD2_GFX_Example to show uses of GxEPD2_GFX base class
- replaced GxEPD2_MultiDisplayExample code, same code as GxEPD2_GFX_MultiDisplayExample
- added extras/examples/GxEPD2_T_MultiDisplayExample, alternate example using template functions
- major and minor fixes, such as typos that survived too long
#### Version 1.2.2
- fixed BMP handling, e.g. for BMPs created by ImageMagick
- see also Arduino Forum Topic https://forum.arduino.cc/index.php?topic=642343.0
#### Version 1.2.1
- added support for GDEW075T7 7.5" b/w 800x480
- GDEW075T7 has differential update (1.6s) using a charge balancing waveform
- added optional SW SPI support, see /extras/sw_spi/README
- added /extras/tests/GxEPD2_RefreshTests/GxEPD2_RefreshTests.ino, for waveform tuning
- minor fixes 
- note that 7.5" e-paper displays don't work reliable if fed from 3.3V Arduino pin
#### Version 1.2.0
- added "fast partial update" (differential update) for GDEW0371W7 3.7" b/w 240x416
- improved differential update waveform for GDEW026T0 2.6" b/w 152x256
- fixed init code & improved differential update for GDEW042T2 4.2" b/w 300x400
- note that all differential refresh waveforms are a compromise (ghosting, big font use)
- parameters for differential waveform for these display can easily be changed for experimenting
- GDEW042T2 would have greyed background without sustain phase
- GDEW042T2 needs multiple full refreshes after extended use of partial updates
#### Version 1.1.10
- added support for GDEH0213B73 2.13" b/w, replacement for GDE0213B1, GDEH0213B72
- added support for GDEW026T0 2.6" b/w 152x256
- added support for GDEW0371W7 3.7" b/w 240x416
- added support for GDEW075Z08 7.5" b/w/r 800x480
- GDEW075Z08 does allow (slow) partial update, set usePartialUpdate = false to disable for better image
- changed 4.2" b/w waveform table, for better result with actual panels
#### Version 1.1.9
- note for ESP8266 when using SS for CS: (wiring suggestion) 
- connect 4.7k pull-down from GPIO15 to GND if your board or shield has level converters
- fixes for large displays (use uint16_t for buffer index)
#### Version 1.1.8
- fix for incomplete download in GxEPD2_WiFi_Example
- added missing method displayWindow() to GxEPD2_GFX base class
- fix and clean up of initial refresh for panels with differential update
- initial refresh needs to be full update, not "fast partial update", for these panels,
- as the screen content may differ from the "previous buffer" content.
- add clean of controller buffer(s) on initial write to controller, for partial update.
#### Version 1.1.7
- enhanced support for full buffered, non-paged use, for processors with enough RAM
- use void display(bool partial_update_mode = false); corresponds to update() in  GxEPD
- use added void displayWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
- use added writeImagePart(...), drawImagePart(...), used by displayWindow()
- added example GxEPD2_NotPagedExample.ino
- several fixes, e.g. parameter range check in setPartialWindow()
#### Version 1.1.6
- added support for GDEH0213B72 2.13" b/w, replacement for GDE0213B1
- changed SPI speed for IT8951 Driver HAT to 24MHz for write, 10MHz for read
- minor fixes, e.g. centering of text bounding box in GxEPD2_Example
#### Version 1.1.5
- added support for 6" ED060SCT on Waveshare e-Paper IT8951 Driver HAT
- uses 3.3V data lines, but 5V supply (~80mA active, ~20mA stand-by, sleep doesn't work)
- note: 5V supply needs to be exact and strong; 5V over diode from USB (e.g. Wemos D1 mini) doesn't work!
- note that the IT8951 Driver HAT is panel specific, with flash parameterized by supplier
- this is an initial version
#### Version 1.1.4+
- added GxEPD2_WS_ESP32_Driver example for Waveshare ESP32 Driver Board
#### Version 1.1.4
- eliminated double refresh for "fast partial update"
- moved wavetables to the driver classes
- added one explicit drawImage(...) and writeImage(...) method (for removed default paramter values for 1.1.3)
- added init method with added parameter  initial, for re-init after processor deep sleep wakeup
- added init parameter pulldown_rst_mode, for special RST handling (not needed for waveshare shield)
#### Version 1.1.3
- fixed wavetables for GDEW029T5 and GDEW0213I5F
- fixed drawImage(...) overloaded methods signature matching ambiguity
#### Version 1.1.2
- added support for GDEW029T5
- fixed (added) clipping for partial window
- fixed (added) powerOff() after full update (partial update keeps power on)
- added hibernate() for minimum power use by displays that support it
#### Version 1.1.1
- 2.7" b/w GDEW027W3 with fast partial update support, based on new demo code wavetable
- mapping suggestion added for Arduino MEGA
- NOTE: use voltage divider resistors for 5V Arduinos, series resistor is not reliable enough
- ConnectingHardware.md updated
#### Version 1.1.0
- added  support for GDEW0213I5F for 2.13" b/w 104x212 flexible display
- updated GxEPD2_WiFi_Example and GxEPD2_Spiffs_Loader to use BearSSL on ESP8266, for large bitmap downloads
#### Version 1.0.9
- add GxEPD2_U8G2_Fonts_Example, e.g. for use of Umlauts ÄÖÜäéöü
- NOTE: you need to SAVE the modified example to a saveable location for UTF-8 characters to work
#### Version 1.0.8
- add GxEPD2_SD_AVR_boards_added.h to GxEPD2_SD_AVR_Example
- with example definitions for non-AVR boards (low level display class use example)
#### Version 1.0.7
- add GxEPD2_boards_added.h to GxEPD2_Example 
- with example definitions for Arduino DUE and MKR1000
#### Version 1.0.6
- add buffered drawing option to GxEPD2_WiFi_Example
- allows use with 1.54" 3-color 200x200 display, may also be useful for small bitmaps
#### Version 1.0.5
- add buffered drawing option to GxEPD2_SD_Example and GxEPD2_Spiffs_Example
- allows use with 1.54" 3-color 200x200 display, may also be useful for small bitmaps
#### Version 1.0.4
- add GxEPD2_GFX base class support (optional, selectable, uses slightly more code)
- base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter
- add GxEPD2_GFX_MultiDisplayExample, uses  GxEPD2_GFX reference parameter
- modify GxEPD2_MultiDisplayExample for ESP8266 (template issue: class expected instead of typename)
#### Version 1.0.3
- fix GxEPD2_SD_Example & GxEPD2_SD_AVR_Example
- add GxEPD2_MultiDisplayExample (preliminary version)
#### Version 1.0.2
- initial release version 1.0.2
- tested with ESP8266, ESP32, STM32F103C8T6, AVR Arduino (Pro Mini 3.3V)
- 1.54" 3-color GxEPD2_154c can be used with paging for AVR
- wave tables in program space,  4.2" b/w can be used with GxEPD2_SD_AVR_Example
- issues enabled and welcome, please use Forum Topic for enhancement suggestions
#### Version 1.0.1
- pre-release test version
- GxEPD2_SD_AVR_Example added, has no graphics buffer to reduce RAM usage (base display class use)
- issues disabled so far, use Arduino Forum Topics instead
- in this version 1.54" 3-color GxEPD2_154c can only be used with full size buffer (or with no buffer base display class)
#### Version 1.0.0
- preliminary version, under construction
