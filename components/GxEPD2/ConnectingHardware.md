# GxEPD2
## Arduino Display Library for SPI E-Paper Displays

- With full Graphics and Text support using Adafruit_GFX

- For SPI e-paper displays from Dalian Good Display 
- and SPI e-paper boards from Waveshare

### important note :
- the display panels are for 3.3V supply and 3.3V data lines
- never connect data lines directly to 5V Arduino data pins, use e.g. 4k7/10k resistor divider
- series resistor only is not enough for reliable operation (back-feed effect through protection diodes)
- 4k7/10k resistor divider may not work with flat cable extensions or Waveshare 4.2 board, use level converter then
- do not forget to connect GND
- the actual Waveshare display boards now have level converters and series regulator, safe for 5V
- use 3k3 pull-down on SS for ESP8266 for boards with level converters

## mapping suggestions

#### mapping suggestion from Waveshare SPI e-Paper to Wemos D1 mini
- BUSY -> D2, RST -> D4, DC -> D3, CS -> D8, CLK -> D5, DIN -> D7, GND -> GND, 3.3V -> 3.3V
- NOTE: connect 3.3k pull-down from D8 to GND if your board or shield has level converters
- NOTE for ESP8266: using SS (GPIO15) for CS may cause boot mode problems, use different pin in case, or 3.3k pull-down
- NOTE: connect 1k pull-up from D4 (RST) to 3.3V if your board or shield has the "clever" reset circuit, or use a different pin

#### mapping suggestion from Waveshare SPI e-Paper to generic ESP8266
- BUSY -> GPIO4, RST -> GPIO2, DC -> GPIO0, CS -> GPIO15, CLK -> GPIO14, DIN -> GPIO13, GND -> GND, 3.3V -> 3.3V
- NOTE: connect 3.3k pull-down from GPIO15 to GND if your board or shield has level converters
- NOTE for ESP8266: using SS (GPIO15) for CS may cause boot mode problems, use different pin in case, or 3.3k pull-down
- NOTE: connect 1k pull-up from GPIO2 (RST) to 3.3V if your board or shield has the "clever" reset circuit, or use a different pin

#### mapping of Waveshare e-Paper ESP8266 Driver Board, new version
- BUSY -> GPIO5, RST -> GPIO2, DC -> GPIO4, CS -> GPIO15, CLK -> GPIO14, DIN -> GPIO13, GND -> GND, 3.3V -> 3.3V
- NOTE for ESP8266: using SS (GPIO15) for CS may cause boot mode problems, add a 3.3k pull-down in case
-      the e-Paper ESP8266 Driver Board should have no boot mode issue, as it doesn't use level converters

#### mapping of Waveshare e-Paper ESP8266 Driver Board, old version
- BUSY -> GPIO16, RST -> GPIO5, DC -> GPIO4, CS -> GPIO15, CLK -> GPIO14, DIN -> GPIO13, GND -> GND, 3.3V -> 3.3V
- NOTE for ESP8266: using SS (GPIO15) for CS may cause boot mode problems, add a 3.3k pull-down in case
-      the e-Paper ESP8266 Driver Board should have no boot mode issue, as it doesn't use level converters

#### mapping suggestion for ESP32, e.g. LOLIN32, see .../variants/.../pins_arduino.h for your board
- NOTE: there are variants with different pins for SPI ! CHECK SPI PINS OF YOUR BOARD
- BUSY -> 4, RST -> 16, DC -> 17, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V

#### mapping of Waveshare ESP32 Driver Board
- BUSY -> 25, RST -> 26, DC -> 27, CS-> 15, CLK -> 13, DIN -> 14
- NOTE: this board uses "unusual" SPI pins and requires re-mapping of HW SPI to these pins in SPIClass
-       see example GxEPD2_WS_ESP32_Driver.ino, it shows how this can be done easily

#### mapping suggestion for ESP32, e.g. LOLIN32 D32 PRO
- BUSY -> 15, RST -> 2, DC -> 0, CS -> 5, CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V
- note: use explicit value for CS, as SS is re-defined to TF_CS(4) in pins_arduino.h for Board: "LOLIN D32 PRO"

#### mapping suggestion for ESP32, e.g. TTGO T8 ESP32-WROVER
- BUSY -> 4, RST -> 0, DC -> 2, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V
- for use with Board: "ESP32 Dev Module":

#### new mapping suggestion for STM32F1, e.g. STM32F103C8T6 "BluePill"
- BUSY -> A1, RST -> A2, DC -> A3, CS-> A4, CLK -> A5, DIN -> A7

#### mapping suggestion for AVR, UNO, NANO etc.
- BUSY -> 7, RST -> 9, DC -> 8, CS-> 10, CLK -> 13, DIN -> 11

#### mapping suggestion for AVR, Arduino Micro, Leonardo
- note: on Leonardo board HW SPI pins are on 6-pin ICSP header
- BUSY -> 7, RST -> 9, DC -> 8, CS-> 10, CLK -> 15, DIN -> 16

#### mapping of Waveshare Universal e-Paper Raw Panel Driver Shield for Arduino / NUCLEO
- BUSY -> 7, RST -> 8, DC -> 9, CS-> 10, CLK -> 13, DIN -> 11

#### mapping suggestion for Arduino MEGA
- BUSY -> 7, RST -> 9, DC -> 8, CS-> 53, CLK -> 52, DIN -> 51

#### mapping suggestion for Arduino DUE, note: pin 77 is on board pin 10, SS is 10
- BUSY -> 7, RST -> 9, DC -> 8, CS-> 10, CLK -> 76, DIN -> 75
- SPI pins are on 6 pin 2x3 SPI header, no SS on SPI header!

#### mapping suggestion for Arduino MKR1000 or MKRZERO
- note: can't use SS on MKR1000: is defined as 24, should be 4
- BUSY -> 5, RST -> 6, DC -> 7, CS-> 4, CLK -> 9, DIN -> 8

#### mapping suggestion for Arduino Nano RP2040 Connect (Arduino MBED OS Nano Boards)
- BUSY -> 7, RST -> 9, DC -> 8, CS-> 10, CLK -> 13, DIN -> 11

#### mapping suggestion for Raspberry Pi Pico RP2040 (Arduino MBED OS RP2040 Boards)
- BUSY -> 7, RST -> 9, DC -> 8, CS-> 5, CLK -> 18, DIN -> 19

#### mapping of my proto board for Raspberry Pi Pico RP2040 (previous default SPI pins)
- BUSY -> 7, RST -> 9, DC -> 8, CS-> 5, CLK -> 2, DIN -> 3

## connection scheme for (discontinued) DESTM32-S2 connection board for e-paper panels:

```
   connection to the e-Paper display is through DESTM32-S2 connection board, available from Good Display

   DESTM32-S2 pinout (top, component side view, solder side of connector):

         |-------------------------------------------------
         |  VCC  |o o| VCC 5V  not needed
         |  GND  |o o| GND     GND
         |  3.3  |o o| 3.3     3.3V
         |  nc   |o o| nc
         |  nc   |o o| nc
         |  nc   |o o| nc
   MOSI  |  DIN  |o o| CLK     SCK
   SS    |  CS   |o o| DC      e.g. D3
   D4    |  RST  |o o| BUSY    e.g. D2
         |  nc   |o o| BS      GND
         |-------------------------------------------------
```
