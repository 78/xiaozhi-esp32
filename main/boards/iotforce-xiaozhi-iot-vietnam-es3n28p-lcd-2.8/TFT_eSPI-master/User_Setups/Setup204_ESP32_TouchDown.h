// User Setup for the ESP32 TouchDown V1.0 and V1.1
// ILI9488 using 4-wire SPI and using an FT6206 touch controller

#define USER_SETUP_ID 204

#define ILI9488_DRIVER

#define TFT_BL   32
#define TFT_BACKLIGHT_ON HIGH

#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4

#define TOUCH_CS 21

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000