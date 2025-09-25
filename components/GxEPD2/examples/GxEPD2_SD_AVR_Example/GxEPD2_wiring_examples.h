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

// mapping of Good Display Arduino UNO Development Kit DEArduino-L, e.g. to DESPI-C02
// BUSY -> 4, RES -> 5, D/C -> 6, CS-> 7, SCK -> 13, SDI -> 11

// mapping of Good Display ESP8266 Development Kit ESP8266-L, e.g. to DESPI-C02
// BUSY -> GPIO16, RES -> GPIO5, D/C -> GPIO4, CS-> GPIO2, SCK -> GPIO14, SDI -> GPIO13

// mapping of Good Display ESP32 Development Kit ESP32-L, e.g. to DESPI-C02
// BUSY -> GPIO13, RES -> GPIO12, D/C -> GPIO14, CS-> GPIO27, SCK -> GPIO18, SDI -> GPIO23

// mapping of Good Display STM32 Development Kit DESPI-L, e.g. to DESPI-C02
// BUSY -> PE13, RES -> PE14, D/C -> PE14, CS-> PD8, SCK -> PD9, SDI -> PD10
// needs jumpers from PA5 (PIN_SPI_SCK) to SCK for EPD and PA7 (PIN_SPI_MOSI) to SDI for EPD. PD9 and PD10 are not HW SPI capable.
// BUSY -> PE13, RES -> PE14, D/C -> PE14, CS-> PD8, SCK -> PA5, SDI -> PA7 // for HW SPI with GxEPD2

// connection suggestions concerning Waveshare e-paper HAT Rev 2.3:
// DON'T FORGET to connect the PWR pin to VCC, to enable power to the board.
// RST is no longer used to disable power to the board, as with earlier revisions.
// Note, for 3.3V processors, no level converters are needed. Use DESPI-C02 instead. Uses less power.
// See https://www.buyepaper.com/products/development-kit-connection-adapter-board-for-eaper-display-demo-kit

// mapping suggestion from Waveshare SPI e-Paper to Wemos D1 mini
// BUSY -> D2, RST -> D4, DC -> D3, CS -> D8, CLK -> D5, DIN -> D7, GND -> GND, 3.3V -> 3.3V
// NOTE: connect 3.3k pull-down from D8 to GND if your board or shield has level converters
// NOTE for ESP8266: using SS (GPIO15) for CS may cause boot mode problems, use different pin in case, or 3.3k pull-down
// NOTE: connect 1k pull-up from D4 (RST) to 3.3V if your board or shield has the "clever" reset circuit, or use a different pin

// mapping suggestion from Waveshare SPI e-Paper to generic ESP8266
// BUSY -> GPIO4, RST -> GPIO2, DC -> GPIO0, CS -> GPIO15, CLK -> GPIO14, DIN -> GPIO13, GND -> GND, 3.3V -> 3.3V
// NOTE: connect 3.3k pull-down from GPIO15 to GND if your board or shield has level converters
// NOTE for ESP8266: using SS (GPIO15) for CS may cause boot mode problems, use different pin in case, or 3.3k pull-down
// NOTE: connect 1k pull-up from GPIO2 (RST) to 3.3V if your board or shield has the "clever" reset circuit, or use a different pin

// mapping of Waveshare e-Paper ESP8266 Driver Board, new version
// BUSY -> GPIO5, RST -> GPIO2, DC -> GPIO4, CS -> GPIO15, CLK -> GPIO14, DIN -> GPIO13, GND -> GND, 3.3V -> 3.3V
// NOTE for ESP8266: using SS (GPIO15) for CS may cause boot mode problems, add a 3.3k pull-down in case
//      the e-Paper ESP8266 Driver Board should have no boot mode issue, as it doesn't use level converters

// mapping of Waveshare e-Paper ESP8266 Driver Board, old version
// BUSY -> GPIO16, RST -> GPIO5, DC -> GPIO4, CS -> GPIO15, CLK -> GPIO14, DIN -> GPIO13, GND -> GND, 3.3V -> 3.3V
// NOTE for ESP8266: using SS (GPIO15) for CS may cause boot mode problems, add a 3.3k pull-down in case
//      the e-Paper ESP8266 Driver Board should have no boot mode issue, as it doesn't use level converters

// mapping suggestion for ESP32, e.g. LOLIN32, see .../variants/.../pins_arduino.h for your board
// NOTE: there are variants with different pins for SPI ! CHECK SPI PINS OF YOUR BOARD
// BUSY -> 4, RST -> 16, DC -> 17, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V

// mapping of Waveshare ESP32 Driver Board
// BUSY -> 25, RST -> 26, DC -> 27, CS-> 15, CLK -> 13, DIN -> 14
// NOTE: this board uses "unusual" SPI pins and requires re-mapping of HW SPI to these pins in SPIClass
//       see example GxEPD2_WS_ESP32_Driver.ino, it shows how this can be done easily

// mapping suggestion for ESP32, e.g. LOLIN32 D32 PRO
// BUSY -> 15, RST -> 2, DC -> 0, CS -> 5, CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V
// note: use explicit value for CS, as SS is re-defined to TF_CS(4) in pins_arduino.h for Board: "LOLIN D32 PRO"

// mapping suggestion for ESP32, e.g. TTGO T8 ESP32-WROVER
// BUSY -> 4, RST -> 0, DC -> 2, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V
// for use with Board: "ESP32 Dev Module":

// mapping suggestion for ESP32S2, e.g. LOLIN ESP32 S2 mini, direct connection of DESPI-C02
// BUSY -> 39, RST -> 37, DC -> 35, CS -> 33, CLK -> 18, DIN -> 16, GND -> GND, 3.3V -> 3.3V
// for use with Board: "LOLIN S2 MINI":

// new mapping suggestion for STM32F1, e.g. STM32F103C8T6 "BluePill"
// BUSY -> A1, RST -> A2, DC -> A3, CS-> A4, CLK -> A5, DIN -> A7

// mapping suggestion for AVR, UNO, NANO etc.
// BUSY -> 7, RST -> 9, DC -> 8, CS-> 10, CLK -> 13, DIN -> 11

// mapping suggestion for AVR, Arduino Micro, Leonardo
// note: on Leonardo board HW SPI pins are on 6-pin ICSP header
// BUSY -> 7, RST -> 9, DC -> 8, CS-> 10, CLK -> 15, DIN -> 16

// mapping of Waveshare Universal e-Paper Raw Panel Driver Shield for Arduino / NUCLEO
// BUSY -> 7, RST -> 8, DC -> 9, CS-> 10, CLK -> 13, DIN -> 11

// mapping suggestion for Arduino MEGA
// BUSY -> 7, RST -> 9, DC -> 8, CS-> 53, CLK -> 52, DIN -> 51

// mapping suggestion for Arduino DUE, note: pin 77 is on board pin 10, SS is 10
// BUSY -> 7, RST -> 9, DC -> 8, CS-> 10, CLK -> 76, DIN -> 75
// SPI pins are on 6 pin 2x3 SPI header, no SS on SPI header!

// mapping suggestion for Arduino MKR1000 or MKRZERO
// note: can't use SS on MKR1000: is defined as 24, should be 4
// BUSY -> 5, RST -> 6, DC -> 7, CS-> 4, CLK -> 9, DIN -> 8

// mapping suggestion for Arduino Nano RP2040 Connect (Arduino MBED OS Nano Boards)
// BUSY -> 7, RST -> 9, DC -> 8, CS-> 10, CLK -> 13, DIN -> 11

// mapping suggestion for Raspberry Pi Pico RP2040 (Arduino MBED OS RP2040 Boards)
// BUSY -> 7, RST -> 9, DC -> 8, CS-> 5, CLK -> 18, DIN -> 19

// mapping of my proto board for Raspberry Pi Pico RP2040 (previous default SPI pins)
// BUSY -> 7, RST -> 9, DC -> 8, CS-> 5, CLK -> 2, DIN -> 3

// mapping of my new proto board like Waveshare Pico-ePaper-2.9
// needs 10k pull-up on RST when used with Arduino MBED OS RP2040 Boards
// BUSY -> 13, RST -> 12, DC -> 8, CS-> 9, CLK -> 10, DIN -> 11

// mapping of Waveshare Pico-ePaper-2.9
// BUSY -> 13, RST -> 12, DC -> 8, CS-> 9, CLK -> 10, DIN -> 11
