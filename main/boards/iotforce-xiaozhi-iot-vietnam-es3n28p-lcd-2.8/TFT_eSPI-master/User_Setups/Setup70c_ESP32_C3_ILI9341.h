// Setup for the ESP32 C3 with ILI9341 display
// Note SPI DMA with ESP32 C3 is not currently supported

#define USER_SETUP_ID 70
// See SetupX_Template.h for all options available

#define ILI9341_DRIVER
//#define ST7796_DRIVER
//#define ILI9488_DRIVER

// Adafruit qtpy default
//TFT_CS 6
//TFT_MOSI 7
//TFT_MISO 8
//TFT_SCLK 10

// Lolin C3 mini default
//TFT_CS 5
//TFT_MOSI 4
//TFT_MISO 3
//TFT_SCLK 2

//ESP32 C3 generic default
//TFT_CS 7
//TFT_MOSI 6
//TFT_MISO 5
//TFT_SCLK 4


#define TFT_CS   7

#define TFT_MOSI 6
#define TFT_MISO 5
#define TFT_SCLK 4

#define TFT_DC    8
#define TFT_RST   10

//#define TOUCH_CS  1 // Optional for touch screen

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

#define SPI_READ_FREQUENCY  6000000 // 6 MHz is the maximum SPI read speed for the ST7789V

#define SPI_TOUCH_FREQUENCY 2500000
