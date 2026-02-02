// For sprite only functionality (untested)
#define USER_SETUP_ID 0

#define TFT_WIDTH  1000
#define TFT_HEIGHT 1000

#define TFT_INIT_DELAY 0

#define TFT_NOP     0x00
#define TFT_SWRST   0x00

#define TFT_CASET   0x00
#define TFT_PASET   0x00
#define TFT_RAMWR   0x00

#define TFT_RAMRD   0x00
#define TFT_IDXRD   0x00

/*
#define TFT_MADCTL  0x00
#define TFT_MAD_MY  0x00
#define TFT_MAD_MX  0x00
#define TFT_MAD_MV  0x00
#define TFT_MAD_ML  0x00
#define TFT_MAD_BGR 0x00
#define TFT_MAD_MH  0x00
#define TFT_MAD_RGB 0x00
*/
#define TFT_INVOFF  0x00
#define TFT_INVON   0x00


#define TFT_MISO -1
#define TFT_MOSI -1
#define TFT_SCLK -1


#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
//#define LOAD_FONT8N // Font 8. Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT
