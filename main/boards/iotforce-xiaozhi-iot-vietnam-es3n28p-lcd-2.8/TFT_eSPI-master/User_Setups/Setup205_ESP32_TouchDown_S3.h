// User Setup for the ESP32 TouchDown S3 V1.1
// ILI9488 using 8-bit Parallel and using an FT6206 touch controller

#define USER_SETUP_ID 205

#define ESP32_PARALLEL

#define ILI9488_DRIVER    

#define TFT_DC   5
#define TFT_RST  46

#define TFT_WR   7
#define TFT_RD   6

#define TFT_D0   21
#define TFT_D1   14
#define TFT_D2   13
#define TFT_D3   12
#define TFT_D4   11
#define TFT_D5   10
#define TFT_D6   9
#define TFT_D7   8

#define TFT_BL	 48
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT