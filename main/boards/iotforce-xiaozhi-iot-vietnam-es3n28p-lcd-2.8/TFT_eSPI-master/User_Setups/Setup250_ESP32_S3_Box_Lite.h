// Display configuration for ST7789-based ESP32-S3-Box-Lite

#define USER_SETUP_ID 250
#define USER_SETUP_INFO "ESP32-S3-BOX-LITE"

#define ST7789_DRIVER
#define TFT_RGB_ORDER TFT_BGR

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_BL   45
#define TFT_BACKLIGHT_ON LOW

#define TFT_CS   5
#define TFT_DC   4
#define TFT_RST  48

#define TFT_MOSI 6
#define TFT_SCLK 7

#define LOAD_GLCD 
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

#define SPI_FREQUENCY  40000000
