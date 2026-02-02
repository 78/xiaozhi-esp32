// Change the width and height if required (defined in portrait mode)
// or use the constructor to over-ride defaults
#if defined (SSD1963_480_DRIVER)
  #define TFT_WIDTH  272
  #define TFT_HEIGHT 480
#elif defined (SSD1963_800_DRIVER)
  #define TFT_WIDTH  480
  #define TFT_HEIGHT 800
#elif defined (SSD1963_800ALT_DRIVER)
  #define TFT_WIDTH  480
  #define TFT_HEIGHT 800
#elif defined (SSD1963_800BD_DRIVER)
  #define TFT_WIDTH  480
  #define TFT_HEIGHT 800
#endif

//Set driver type common to all initialisation options
#ifndef SSD1963_DRIVER
  #define SSD1963_DRIVER
#endif

// Delay between some initialisation commands
#define TFT_INIT_DELAY 0x80 // Not used unless commandlist invoked

// Generic commands used by TFT_eSPI.cpp
#define TFT_NOP     0x00
#define TFT_SWRST   0x01

#define TFT_INVOFF  0x20
#define TFT_INVON   0x21

#define TFT_DISPOFF 0x28
#define TFT_DISPON  0x29

#define TFT_CASET   0x2A
#define TFT_PASET   0x2B
#define TFT_RAMWR   0x2C

#define TFT_RAMRD   0x2E
#define TFT_IDXRD   0xDD // ILI9341 only, indexed control register read

#define TFT_MADCTL  0x36
#define TFT_MAD_MY  0x80
#define TFT_MAD_MX  0x40
#define TFT_MAD_MV  0x20
#define TFT_MAD_ML  0x10
#define TFT_MAD_BGR 0x08
#define TFT_MAD_MH  0x04
#define TFT_MAD_RGB 0x00

#ifdef TFT_RGB_ORDER
  #if (TFT_RGB_ORDER == 1)
    #define TFT_MAD_COLOR_ORDER TFT_MAD_RGB
  #else
    #define TFT_MAD_COLOR_ORDER TFT_MAD_BGR
  #endif
#else
  #define TFT_MAD_COLOR_ORDER TFT_MAD_BGR
#endif
