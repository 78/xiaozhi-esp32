// Setup for the ESP32 S3 with GC9A01 display
#define USER_SETUP_ID 70

#define GC9A01_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 240
                    // Typical board default pins - change to match your board
#define TFT_CS   10 //34 //     10 or 34 (FSPI CS0) 
#define TFT_MOSI 11 //35 //     11 or 35 (FSPI D)
#define TFT_SCLK 12 //36 //     12 or 36 (FSPI CLK)
#define TFT_MISO 13 //37 //     13 or 37 (FSPI Q)

// Use pins in range 0-31
#define TFT_DC    7
#define TFT_RST   6

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

// FSPI port (SPI2) used unless following defined
#define USE_HSPI_PORT

#define SPI_FREQUENCY  80000000   // Maximum for ILI9341
