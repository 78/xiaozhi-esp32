// Change the width and height if required (defined in portrait mode)
// or use the constructor to over-ride defaults
// RM68120_DRIVER

#define TFT_WIDTH  480
#define TFT_HEIGHT 800

//Set driver type common to all TBD initialisation options
#ifndef RM68120_DRIVER
  #define RM68120_DRIVER
#endif

// Delay between some initialisation commands
#define TFT_INIT_DELAY 0x80 // Not used unless commandlist invoked

// Generic commands used by TFT_eSPI.cpp
#define TFT_NOP      0x0000
#define TFT_SWRST    0x0100

#define TFT_INVOFF   0x2000
#define TFT_INVON    0x2100

#define TFT_DISPOFF  0x2800
#define TFT_DISPON   0x2900

#define TFT_CASET    0x2A00
#define TFT_PASET    0x2B00
#define TFT_RAMWR    0x2C00

#define TFT_RAMRD    0x2E00
#define TFT_IDXRD    0xDD00 // ILI9341 only, indexed control register read

#define TFT_MADCTL   0x3600
#define TFT_MAD_MY     0x80
#define TFT_MAD_MX     0x40
#define TFT_MAD_MV     0x20
#define TFT_MAD_ML     0x10
#define TFT_MAD_BGR    0x08
#define TFT_MAD_RGB    0x00
#define TFT_MAD_MH     0x04
#define TFT_MAD_H_FLIP 0x02
#define TFT_MAD_V_FLIP 0x01

#ifdef TFT_RGB_ORDER
  #if (TFT_RGB_ORDER == 1)
    #define TFT_MAD_COLOR_ORDER TFT_MAD_RGB
  #else
    #define TFT_MAD_COLOR_ORDER TFT_MAD_BGR
  #endif
#else
  #define TFT_MAD_COLOR_ORDER TFT_MAD_RGB
#endif
