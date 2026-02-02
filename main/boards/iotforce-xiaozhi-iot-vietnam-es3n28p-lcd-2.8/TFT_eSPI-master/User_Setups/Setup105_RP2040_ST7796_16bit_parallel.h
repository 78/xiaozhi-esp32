// This setup is for the RP2040 processor only when used with 8-bit parallel displays
// See SetupX_Template.h for all options available
#define USER_SETUP_ID 105

////////////////////////////////////////////////////////////////////////////////////////////
// Interface
////////////////////////////////////////////////////////////////////////////////////////////
#define TFT_PARALLEL_16_BIT

////////////////////////////////////////////////////////////////////////////////////////////
// Display driver type
////////////////////////////////////////////////////////////////////////////////////////////
#define ST7796_DRIVER

////////////////////////////////////////////////////////////////////////////////////////////
// RP2040 pins used
////////////////////////////////////////////////////////////////////////////////////////////

//#define TFT_CS   -1  // Do not define, chip select control pin permanently connected to 0V

// These pins can be moved and are controlled directly by the library software
#define TFT_DC    3    // Data Command control pin
#define TFT_RST   2    // Reset pin

//#define TFT_RD   -1  // Do not define, read pin must be permanently connected to 3V3
#define TFT_WR   4

// PIO requires these to be sequentially increasing GPIO with no gaps
#define TFT_D0    6
#define TFT_D1    7
#define TFT_D2    8
#define TFT_D3    9
#define TFT_D4   10
#define TFT_D5   11
#define TFT_D6   12
#define TFT_D7   13
#define TFT_D8   14
#define TFT_D9   15
#define TFT_D10  16
#define TFT_D11  17
#define TFT_D12  18
#define TFT_D13  19
#define TFT_D14  20
#define TFT_D15  21


////////////////////////////////////////////////////////////////////////////////////////////
// Fonts to be available
////////////////////////////////////////////////////////////////////////////////////////////
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT

////////////////////////////////////////////////////////////////////////////////////////////