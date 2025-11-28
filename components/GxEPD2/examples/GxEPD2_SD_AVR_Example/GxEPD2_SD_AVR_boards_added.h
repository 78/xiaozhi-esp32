// Display Library example for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Display Library based on Demo Example from Good Display: http://www.e-paper-display.com/download_list/downloadcategoryid=34&isMode=false.html
//
// Author: Jean-Marc Zingg
//
// Version: see library.properties
//
// Library: https://github.com/ZinggJM/GxEPD2

// non-AVR board can also be used with GxEPD2 base display classes, e.g. for SD bitmap drawing

#ifndef _GxEPD2_SD_AVR_boards_added_H_
#define _GxEPD2_SD_AVR_boards_added_H_

#if defined (ESP8266)
#define SD_CS SS  // e.g. for RobotDyn Wemos D1 mini SD board
#define EPD_CS D1 // alternative I use with RobotDyn Wemos D1 mini SD board
GxEPD2_DRIVER_CLASS display(/*CS=*/ EPD_CS, /*DC=D3*/ 0, /*RST=D4*/ 2, /*BUSY=D2*/ 4);
#endif

#if defined(ESP32)
#define SD_CS 2  // adapt to your wiring
#define EPD_CS SS // adapt to your wiring
GxEPD2_DRIVER_CLASS display(/*CS=5*/ EPD_CS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4);
#endif

#if defined(_BOARD_GENERIC_STM32F103C_H_)
#define SD_CS 0  // adapt to your wiring
#define EPD_CS SS // adapt to your wiring
GxEPD2_DRIVER_CLASS display(/*CS=4*/ EPD_CS, /*DC=*/ 3, /*RST=*/ 2, /*BUSY=*/ 1);
#endif

#if defined(ARDUINO_ARCH_SAM)
#define SD_CS 0  // adapt to your wiring
#define EPD_CS SS // adapt to your wiring
GxEPD2_DRIVER_CLASS display(/*CS=10*/ SS, /*DC=*/ 8, /*RST=*/ 9, /*BUSY=*/ 7);
#endif

#if defined(ARDUINO_ARCH_SAMD)
#define SD_CS 0  // adapt to your wiring
#define EPD_CS SS // adapt to your wiring
GxEPD2_DRIVER_CLASS display(/*CS=4*/ 4, /*DC=*/ 7, /*RST=*/ 6, /*BUSY=*/ 5);
#endif

#endif
