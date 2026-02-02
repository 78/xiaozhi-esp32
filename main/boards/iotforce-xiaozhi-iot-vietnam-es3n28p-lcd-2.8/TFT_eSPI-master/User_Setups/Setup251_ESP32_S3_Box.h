// Display configuration for ILI9342-based ESP32-S3-Box

#define USER_SETUP_ID 251
#define USER_SETUP_INFO "ESP32-S3-BOX"

#define M5STACK // S3-Box uses the same ILI9342 display, don't remove

#define ILI9341_DRIVER

#define TFT_MISO 0
#define TFT_MOSI 6
#define TFT_SCLK 7
#define TFT_CS   5 // Chip select control pin
#define TFT_DC   4 // Data Command control pin
#define TFT_RST 48 // Reset pin (could connect to Arduino RESET pin)
#define TFT_BL  45 // LED back-light

#define LOAD_GLCD  // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2 // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4 // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6 // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7 // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
#define LOAD_FONT8 // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT

#define SPI_FREQUENCY 27000000
