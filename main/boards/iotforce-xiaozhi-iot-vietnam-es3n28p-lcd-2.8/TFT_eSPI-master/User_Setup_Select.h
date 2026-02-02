// This header file contains a list of user setup files and defines which one the
// compiler uses when the IDE performs a Verify/Compile or Upload.
//
// Users can create configurations for different boards and TFT displays.
// This makes selecting between hardware setups easy by "uncommenting" one line.

// The advantage of this hardware configuration method is that the examples provided
// with the library should work with immediately without any other changes being
// needed. It also improves the portability of users sketches to other hardware
// configurations and compatible libraries.
//
// Create a shortcut to this file on your desktop to permit quick access for editing.
// Re-compile and upload after making and saving any changes to this file.

// Example User_Setup files are stored in the "User_Setups" folder. These can be used
// unmodified or adapted for a particular hardware configuration.

#ifndef USER_SETUP_LOADED //  Lets PlatformIO users define settings in
                          //  platformio.ini, see notes in "Tools" folder.

///////////////////////////////////////////////////////
//   User configuration selection lines are below    //
///////////////////////////////////////////////////////

// Only ONE line below should be uncommented to define your setup.  Add extra lines and files as needed.

#include <User_Setup.h>           // Default setup is root library folder

//#include <User_Setups/Setup1_ILI9341.h>  // Setup file for ESP8266 configured for my ILI9341
//#include <User_Setups/Setup2_ST7735.h>   // Setup file for ESP8266 configured for my ST7735
//#include <User_Setups/Setup3_ILI9163.h>  // Setup file for ESP8266 configured for my ILI9163
//#include <User_Setups/Setup4_S6D02A1.h>  // Setup file for ESP8266 configured for my S6D02A1
//#include <User_Setups/Setup5_RPi_ILI9486.h>        // Setup file for ESP8266 configured for my stock RPi TFT
//#include <User_Setups/Setup6_RPi_Wr_ILI9486.h>     // Setup file for ESP8266 configured for my modified RPi TFT
//#include <User_Setups/Setup7_ST7735_128x128.h>     // Setup file for ESP8266 configured for my ST7735 128x128 display
//#include <User_Setups/Setup8_ILI9163_128x128.h>    // Setup file for ESP8266 configured for my ILI9163 128x128 display
//#include <User_Setups/Setup9_ST7735_Overlap.h>     // Setup file for ESP8266 configured for my ST7735
//#include <User_Setups/Setup10_RPi_touch_ILI9486.h> // Setup file for ESP8266 configured for ESP8266 and RPi TFT with touch

//#include <User_Setups/Setup11_RPi_touch_ILI9486.h> // Setup file configured for ESP32 and RPi TFT with touch
//#include <User_Setups/Setup12_M5Stack_Basic_Core.h>// Setup file for the ESP32 based M5Stack (Basic Core only)
//#include <User_Setups/Setup13_ILI9481_Parallel.h>  // Setup file for the ESP32 with parallel bus TFT
//#include <User_Setups/Setup14_ILI9341_Parallel.h>  // Setup file for the ESP32 with parallel bus TFT
//#include <User_Setups/Setup15_HX8357D.h>           // Setup file for ESP8266 configured for HX8357D
//#include <User_Setups/Setup16_ILI9488_Parallel.h>  // Setup file for the ESP32 with parallel bus TFT
//#include <User_Setups/Setup17_ePaper.h>            // Setup file for ESP8266 and any Waveshare ePaper display
//#include <User_Setups/Setup18_ST7789.h>            // Setup file for ESP8266 configured for ST7789

//#include <User_Setups/Setup19_RM68140_Parallel.h>	 // Setup file configured for RM68140 with parallel bus

//#include <User_Setups/Setup20_ILI9488.h>           // Setup file for ESP8266 and ILI9488 SPI bus TFT
//#include <User_Setups/Setup21_ILI9488.h>           // Setup file for ESP32 and ILI9488 SPI bus TFT

//#include <User_Setups/Setup22_TTGO_T4.h>           // Setup file for ESP32 and TTGO T4 version 1.2
//#include <User_Setups/Setup22_TTGO_T4_v1.3.h>      // Setup file for ESP32 and TTGO T4 version 1.3
//#include <User_Setups/Setup23_TTGO_TM.h>           // Setup file for ESP32 and TTGO TM ST7789 SPI bus TFT
//#include <User_Setups/Setup24_ST7789.h>            // Setup file for DSTIKE/ESP32/ESP8266 configured for ST7789 240 x 240
//#include <User_Setups/Setup25_TTGO_T_Display.h>    // Setup file for ESP32 and TTGO T-Display ST7789V SPI bus TFT
//#include <User_Setups/Setup26_TTGO_T_Wristband.h>  // Setup file for ESP32 and TTGO T-Wristband ST7735 SPI bus TFT

//#include <User_Setups/Setup27_RPi_ST7796_ESP32.h>    // ESP32   RPi MHS-4.0 inch Display-B
//#include <User_Setups/Setup28_RPi_ST7796_ESP8266.h>  // ESP8266 RPi MHS-4.0 inch Display-B

//#include <User_Setups/Setup29_ILI9341_STM32.h>          // Setup for Nucleo board
//#include <User_Setups/Setup30_ILI9341_Parallel_STM32.h> // Setup for Nucleo board and parallel display
//#include <User_Setups/Setup31_ST7796_Parallel_STM32.h>  // Setup for Nucleo board and parallel display
//#include <User_Setups/Setup32_ILI9341_STM32F103.h>      // Setup for "Blue/Black Pill"

//#include <User_Setups/Setup33_RPi_ILI9486_STM32.h>      // Setup for Nucleo board

//#include <User_Setups/Setup34_ILI9481_Parallel_STM32.h> // Setup for Nucleo board and parallel display
//#include <User_Setups/Setup35_ILI9341_STM32_Port_Bus.h> // Setup for STM32 port A parallel display

//#include <User_Setups/Setup36_RPi_touch_ST7796.h>      // Setup file configured for ESP32 and RPi ST7796 TFT with touch

//#include <User_Setups/Setup42_ILI9341_ESP32.h>           // Setup file for ESP32 and SPI ILI9341 240x320
//#include <User_Setups/Setup43_ST7735.h>            // Setup file for ESP8266 & ESP32 configured for my ST7735S 80x160
//#include <User_Setups/Setup44_TTGO_CameraPlus.h>   // Setup file for ESP32 and TTGO T-CameraPlus ST7789 SPI bus TFT    240x240
//#include <User_Setups/Setup45_TTGO_T_Watch.h>      // Setup file for ESP32 and TTGO T-Watch ST7789 SPI bus TFT  240x240
//#include <User_Setups/Setup46_GC9A01_ESP32.h>      // Setup file for ESP32 and GC9A01 SPI bus TFT  240x240

//#include <User_Setups/Setup47_ST7735.h>            // Setup file for ESP32 configured for ST7735 128 x 128 animated eyes

//#include <User_Setups/Setup50_SSD1963_Parallel.h>  // Setup file for ESP32 and SSD1963 TFT display

//#include <User_Setups/Setup51_LilyPi_ILI9481.h>    // Setup file for LilyGo LilyPi with ILI9481 display
//#include <User_Setups/Setup52_LilyPi_ST7796.h>     // Setup file for LilyGo LilyPi with ST7796 display

//#include <User_Setups/Setup60_RP2040_ILI9341.h>              // Setup file for RP2040 with SPI ILI9341
//#include <User_Setups/Setup61_RP2040_ILI9341_PIO_SPI.h>      // Setup file for RP2040 with PIO SPI ILI9341
//#include <User_Setups/Setup62_RP2040_Nano_Connect_ILI9341.h> // Setup file for RP2040 with SPI ILI9341

//#include <User_Setups/Setup66_Seeed_XIAO_Round.h>     // Setup file for Seeed XIAO with GC9A01 240x240

//#include <User_Setups/Setup70_ESP32_S2_ILI9341.h>     // Setup file for ESP32 S2 with SPI ILI9341
//#include <User_Setups/Setup70b_ESP32_S3_ILI9341.h>    // Setup file for ESP32 S3 with SPI ILI9341
//#include <User_Setups/Setup70c_ESP32_C3_ILI9341.h>    // Setup file for ESP32 C3 with SPI ILI9341
//#include <User_Setups/Setup70d_ILI9488_S3_Parallel.h> // Setup file for ESP32 S3 with SPI ILI9488

//#include <User_Setups/Setup71_ESP32_S2_ST7789.h>       // Setup file for ESP32 S2 with ST7789
//#include <User_Setups/Setup72_ESP32_ST7789_172x320.h>  // Setup file for ESP32 with ST7789 1.47" 172x320

//#include <User_Setups/Setup100_RP2040_ILI9488_parallel.h> // Setup file for Pico/RP2040 with 8-bit parallel ILI9488
//#include <User_Setups/Setup101_RP2040_ILI9481_parallel.h> // Setup file for Pico/RP2040 with 8-bit parallel ILI9481
//#include <User_Setups/Setup102_RP2040_ILI9341_parallel.h> // Setup file for Pico/RP2040 with 8-bit parallel ILI9341
//#include <User_Setups/Setup103_RP2040_ILI9486_parallel.h> // Setup file for Pico/RP2040 with 8-bit parallel ILI9486
//#include <User_Setups/Setup104_RP2040_ST7796_parallel.h>  // Setup file for Pico/RP2040 with 8-bit parallel ST7796

//#include <User_Setups/Setup105_RP2040_ST7796_16bit_parallel.h>  // Setup file for RP2040 16-bit parallel display
//#include <User_Setups/Setup106_RP2040_ILI9481_16bit_parallel.h> // Setup file for RP2040 16-bit parallel display
//#include <User_Setups/Setup107_RP2040_ILI9341_16bit_parallel.h> // Setup file for RP2040 16-bit parallel display
//#include <User_Setups/Setup108_RP2040_ST7735.h> // Setup file for Waveshare RP2040 board with onboard ST7735 0.96" 160x80 display

//#include <User_Setups/Setup135_ST7789.h>           // Setup file for ESP8266 and ST7789 135 x 240 TFT

//#include <User_Setups/Setup136_LilyGo_TTV.h>       // Setup file for ESP32 and Lilygo TTV ST7789 SPI bus TFT  135x240
//#include <User_Setups/Setup137_LilyGo_TDisplay_RP2040.h>  // Setup file for Lilygo T-Display RP2040 (ST7789 on SPI bus with 135x240 TFT)

//#include <User_Setups/Setup138_Pico_Explorer_Base_RP2040_ST7789.h> // Setup file for Pico Explorer Base by Pimoroni for RP2040 (ST7789 on SPI bus with 240x240 TFT)

//#include <User_Setups/Setup200_GC9A01.h>           // Setup file for ESP32 and GC9A01 240 x 240 TFT

//#include <User_Setups/Setup201_WT32_SC01.h>        // Setup file for ESP32 based WT32_SC01 from Seeed

//#include <User_Setups/Setup202_SSD1351_128.h>      // Setup file for ESP32/ESP8266 based SSD1351 128x128 1.5inch OLED display

//#include <User_Setups/Setup203_ST7789.h>     // Setup file for ESP32/ESP8266 based ST7789 240X280 1.69inch TFT 

//#include <User_Setups/Setup204_ESP32_TouchDown.h>     // Setup file for the ESP32 TouchDown based on ILI9488 480 x 320 TFT 

//#include <User_Setups/Setup205_ESP32_TouchDown_S3.h>     // Setup file for the ESP32 TouchDown S3 based on ILI9488 480 x 320 TFT 

//#include <User_Setups/Setup206_LilyGo_T_Display_S3.h>     // For the LilyGo T-Display S3 based ESP32S3 with ST7789 170 x 320 TFT
//#include <User_Setups/Setup207_LilyGo_T_HMI.h>            // For the LilyGo T-HMI S3 based ESP32S3 with ST7789 240 x 320 TFT
//#include <User_Setups/Setup209_LilyGo_T_Dongle_S3.h>      // For the LilyGo T-Dongle S3 based ESP32 with ST7735 80 x 160 TFT
//#include <User_Setups/Setup210_LilyGo_T_Embed_S3.h>         // For the LilyGo T-Embed S3 based ESP32S3 with ST7789 170 x 320 TFT
//#include <User_Setups/Setup211_LilyGo_T_QT_Pro_S3.h>         // For the LilyGo T-QT Pro S3 based ESP32S3 with GC9A01 128 x 128 TFT
// #include <User_Setups/Setup212_LilyGo_T_PicoPro.h>         // For the LilyGo T-PICO-Pro with ST7796 222 x 480 TFT
// #include <User_Setups/Setup213_LilyGo_T_Beam_Shield.h>         // For the LilyGo T-BEAM V1.x with ST7796 222 x 480 TFT

//#include <User_Setups/Setup250_ESP32_S3_Box_Lite.h>      // For the ESP32 S3 Box Lite
//#include <User_Setups/Setup251_ESP32_S3_Box.h>            // For the ESP32 S3 Box

//#include <User_Setups/Setup301_BW16_ST7735.h>            // Setup file for Bw16-based boards with ST7735 160 x 80 TFT
//#include <User_Setups/Setup302_Waveshare_ESP32S3_GC9A01.h>            // Setup file for Waveshare ESP32-S3-Touch-LCD-1.28 board with GC9A01 240*240 TFT

//#include <User_Setups/SetupX_Template.h>     // Template file for a setup


//#include <User_Setups/Dustin_ILI9488.h>          // Setup file for Dustin Watts PCB with ILI9488
//#include <User_Setups/Dustin_ST7796.h>           // Setup file for Dustin Watts PCB with ST7796
//#include <User_Setups/Dustin_ILI9488_Pico.h>     // Setup file for Dustin Watts Pico PCB with ST7796
//#include <User_Setups/Dustin_ST7789_Pico.h>      // Setup file for Dustin Watts PCB with ST7789 240 x 240 on 3.3V adapter board
//#include <User_Setups/Dustin_GC9A01_Pico.h>      // Setup file for Dustin Watts PCB with GC9A01 240 x 240 on 3.3V adapter board
//#include <User_Setups/Dustin_GC9A01_ESP32.h>     // Setup file for Dustin Watts PCB with GC9A01 240 x 240 on 3.3V adapter board
//#include <User_Setups/Dustin_STT7789_ESP32.h>    // Setup file for Dustin Watts PCB with ST7789 240 x 240 on 3.3V adapter board
//#include <User_Setups/Dustin_ILI9341_ESP32.h>    // Setup file for Dustin Watts PCB with ILI9341
//#include <User_Setups/ILI9225.h>

#endif // USER_SETUP_LOADED



/////////////////////////////////////////////////////////////////////////////////////
//                                                                                 //
//     DON'T TINKER WITH ANY OF THE FOLLOWING LINES, THESE ADD THE TFT DRIVERS     //
//       AND ESP8266 PIN DEFINITONS, THEY ARE HERE FOR BODMER'S CONVENIENCE!       //
//                                                                                 //
/////////////////////////////////////////////////////////////////////////////////////


// Identical looking TFT displays may have a different colour ordering in the 16-bit colour
#define TFT_BGR 0   // Colour order Blue-Green-Red
#define TFT_RGB 1   // Colour order Red-Green-Blue

// Legacy setup support, RPI_DISPLAY_TYPE replaces RPI_DRIVER
#if defined (RPI_DRIVER)
  #if !defined (RPI_DISPLAY_TYPE)
    #define RPI_DISPLAY_TYPE
  #endif
#endif

// Legacy setup support, RPI_ILI9486_DRIVER form is deprecated
// Instead define RPI_DISPLAY_TYPE and also define driver (e.g. ILI9486_DRIVER) 
#if defined (RPI_ILI9486_DRIVER)
  #if !defined (ILI9486_DRIVER)
    #define ILI9486_DRIVER
  #endif
  #if !defined (RPI_DISPLAY_TYPE)
    #define RPI_DISPLAY_TYPE
  #endif
#endif

// Invoke 18-bit colour for selected displays
#if !defined (RPI_DISPLAY_TYPE) && !defined (TFT_PARALLEL_8_BIT) && !defined (TFT_PARALLEL_16_BIT) && !defined (ESP32_PARALLEL)
  #if defined (ILI9481_DRIVER) || defined (ILI9486_DRIVER) || defined (ILI9488_DRIVER)
    #define SPI_18BIT_DRIVER
  #endif
#endif

// Load the right driver definition - do not tinker here !
#if   defined (ILI9341_DRIVER) || defined(ILI9341_2_DRIVER) || defined (ILI9342_DRIVER)
     #include <TFT_Drivers/ILI9341_Defines.h>
     #define  TFT_DRIVER 0x9341
#elif defined (ST7735_DRIVER)
     #include <TFT_Drivers/ST7735_Defines.h>
     #define  TFT_DRIVER 0x7735
#elif defined (ILI9163_DRIVER)
     #include <TFT_Drivers/ILI9163_Defines.h>
     #define  TFT_DRIVER 0x9163
#elif defined (S6D02A1_DRIVER)
     #include <TFT_Drivers/S6D02A1_Defines.h>
     #define  TFT_DRIVER 0x6D02
#elif defined (ST7796_DRIVER)
      #include "TFT_Drivers/ST7796_Defines.h"
      #define  TFT_DRIVER 0x7796
#elif defined (ILI9486_DRIVER)
     #include <TFT_Drivers/ILI9486_Defines.h>
     #define  TFT_DRIVER 0x9486
#elif defined (ILI9481_DRIVER)
     #include <TFT_Drivers/ILI9481_Defines.h>
     #define  TFT_DRIVER 0x9481
#elif defined (ILI9488_DRIVER)
     #include <TFT_Drivers/ILI9488_Defines.h>
     #define  TFT_DRIVER 0x9488
#elif defined (HX8357D_DRIVER)
     #include "TFT_Drivers/HX8357D_Defines.h"
     #define  TFT_DRIVER 0x8357
#elif defined (EPD_DRIVER)
     #include "TFT_Drivers/EPD_Defines.h"
     #define  TFT_DRIVER 0xE9D
#elif defined (ST7789_DRIVER)
     #include "TFT_Drivers/ST7789_Defines.h"
     #define  TFT_DRIVER 0x7789
#elif defined (R61581_DRIVER)
     #include "TFT_Drivers/R61581_Defines.h"
     #define  TFT_DRIVER 0x6158
#elif defined (ST7789_2_DRIVER)
     #include "TFT_Drivers/ST7789_2_Defines.h"
     #define  TFT_DRIVER 0x778B
#elif defined (RM68140_DRIVER)
     #include "TFT_Drivers/RM68140_Defines.h"
     #define  TFT_DRIVER 0x6814
#elif defined (SSD1351_DRIVER)
     #include "TFT_Drivers/SSD1351_Defines.h"
     #define  TFT_DRIVER 0x1351
#elif defined (SSD1963_480_DRIVER)
     #include "TFT_Drivers/SSD1963_Defines.h"
     #define  TFT_DRIVER 0x1963
#elif defined (SSD1963_800_DRIVER)
     #include "TFT_Drivers/SSD1963_Defines.h"
     #define  TFT_DRIVER 0x1963
#elif defined (SSD1963_800ALT_DRIVER)
     #include "TFT_Drivers/SSD1963_Defines.h"
     #define  TFT_DRIVER 0x1963
#elif defined (SSD1963_800BD_DRIVER)
     #include "TFT_Drivers/SSD1963_Defines.h"
     #define  TFT_DRIVER 0x1963
#elif defined (GC9A01_DRIVER)
     #include "TFT_Drivers/GC9A01_Defines.h"
     #define  TFT_DRIVER 0x9A01
#elif defined (ILI9225_DRIVER)
     #include "TFT_Drivers/ILI9225_Defines.h"
     #define  TFT_DRIVER 0x9225
#elif defined (RM68120_DRIVER)
     #include "TFT_Drivers/RM68120_Defines.h"
     #define  TFT_DRIVER 0x6812
#elif defined (HX8357B_DRIVER)
     #include "TFT_Drivers/HX8357B_Defines.h"
     #define  TFT_DRIVER 0x835B
#elif defined (HX8357C_DRIVER)
     #include "TFT_Drivers/HX8357C_Defines.h"
     #define  TFT_DRIVER 0x835C

                              // <<<<<<<<<<<<<<<<<<<<<<<< ADD NEW DRIVER HERE
                              // XYZZY_init.h and XYZZY_rotation.h must also be added in TFT_eSPI.cpp
#elif defined (XYZZY_DRIVER)
     #include "TFT_Drivers/XYZZY_Defines.h"
     #define  TFT_DRIVER 0x0000
#else
     #define  TFT_DRIVER 0x0000
#endif

// These are the pins for ESP8266 boards
//      Name   GPIO    NodeMCU      Function
#define PIN_D0  16  // GPIO16       WAKE
#define PIN_D1   5  // GPIO5        User purpose
#define PIN_D2   4  // GPIO4        User purpose
#define PIN_D3   0  // GPIO0        Low on boot means enter FLASH mode
#define PIN_D4   2  // GPIO2        TXD1 (must be high on boot to go to UART0 FLASH mode)
#define PIN_D5  14  // GPIO14       HSCLK
#define PIN_D6  12  // GPIO12       HMISO
#define PIN_D7  13  // GPIO13       HMOSI  RXD2
#define PIN_D8  15  // GPIO15       HCS    TXD0 (must be low on boot to enter UART0 FLASH mode)
#define PIN_D9   3  //              RXD0
#define PIN_D10  1  //              TXD0

#define PIN_MOSI 8  // SD1          FLASH and overlap mode
#define PIN_MISO 7  // SD0
#define PIN_SCLK 6  // CLK
#define PIN_HWCS 0  // D3

#define PIN_D11  9  // SD2
#define PIN_D12 10  // SD4
