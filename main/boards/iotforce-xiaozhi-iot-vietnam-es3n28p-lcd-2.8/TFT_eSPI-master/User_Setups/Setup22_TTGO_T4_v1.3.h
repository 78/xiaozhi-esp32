// Setup for the TTGO T4 v1.3 ESP32 board with 2.2" ILI9341 display
#define USER_SETUP_ID 99922

// See SetupX_Template.h for all options available

#define ILI9341_DRIVER

#define TFT_BACKLIGHT_ON HIGH
#define TFT_BL 4

#define TFT_MISO 12
#define TFT_MOSI 23
#define TFT_SCLK 18

#define TFT_CS   27
#define TFT_DC   32 // pin 26 for TTGO T4 v1.3
#define TFT_RST   5

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

//#define SPI_FREQUENCY  27000000
  #define SPI_FREQUENCY  40000000   // Maximum for ILI9341

#define USE_HSPI_PORT

#define SPI_READ_FREQUENCY  6000000 // 6 MHz is the maximum SPI read speed for the ST7789V
