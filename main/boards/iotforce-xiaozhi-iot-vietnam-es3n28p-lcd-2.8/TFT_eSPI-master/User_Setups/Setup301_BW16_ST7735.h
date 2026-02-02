#include "itoa.h"
#include <avr/dtostrf.h>


// Setup for BW16 and ST7735 80 x 160 TFT
#define USER_SETUP_ID 301
// See SetupX_Template.h for all options available

#define ST7735_DRIVER


#define TFT_WIDTH  80
#define TFT_HEIGHT 160


#define ST7735_GREENTAB160x80

// For ST7735, ST7789 and ILI9341 ONLY, define the colour order IF the blue and red are swapped on your display
// Try ONE option at a time to find the correct colour order for your display

//  #define TFT_RGB_ORDER TFT_RGB  // Colour order Red-Green-Blue
//  #define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red

// Pin Mappings for BW16 board
#define TFT_MISO 11
#define TFT_MOSI 12
#define TFT_SCLK 10
#define TFT_CS    9  // Chip select control pin
#define TFT_DC    8  // Data Command control pin
#define TFT_RST   6  // Reset pin (could connect to RST pin)


#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
//#define LOAD_FONT8N // Font 8. Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT


#define SPI_FREQUENCY  20000000
//#define SPI_FREQUENCY  27000000 // Actually sets it to 26.67MHz = 80/3

