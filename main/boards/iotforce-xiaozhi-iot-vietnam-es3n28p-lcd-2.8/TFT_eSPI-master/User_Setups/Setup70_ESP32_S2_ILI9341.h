// Setup for the ESP32 S2 with ILI9341 display
// Note SPI DMA with ESP32 S2 is not currently supported
#define USER_SETUP_ID 70
// See SetupX_Template.h for all options available
#define ILI9341_DRIVER

                    // Typical board default pins
#define TFT_CS   10 //     10 or 34

#define TFT_MOSI 11 //     11 or 35
#define TFT_SCLK 12 //     12 or 36
#define TFT_MISO 13 //     13 or 37

#define TFT_DC   14
#define TFT_RST  15

//#define TOUCH_CS 16 // Optional for touch screen

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

// FSPI port will be used unless the following is defined
#define USE_HSPI_PORT

//#define SPI_FREQUENCY  27000000
#define SPI_FREQUENCY  40000000   // Maximum for ILI9341

#define SPI_READ_FREQUENCY  6000000 // 6 MHz is the maximum SPI read speed for the ST7789V

#define SPI_TOUCH_FREQUENCY 2500000
