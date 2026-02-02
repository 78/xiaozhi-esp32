        //////////////////////////////////////////////////////////////
        // Setup for STM32F103 (e.g. Blue Pill) and ILI9341 display //
        //////////////////////////////////////////////////////////////
#define USER_SETUP_ID 32

// Last update by Bodmer: 14/1/20

// Define STM32 to invoke STM32 optimised driver
#define STM32

// Define the TFT display driver
#define ILI9341_DRIVER
//#define ILI9481_DRIVER

// MOSI and SCK do not need to be defined, connect:
//  - Arduino SCK (Blue Pill pin A5)  to TFT SCK
//  - Arduino MOSI (Blue Pill pin A7) to TFT SDI (may be marked SDA or MOSI)
//  - Optional to read TFT: Arduino MISO (Blue Pill pin A6) to TFT SDO (may be marked MISO or may not exist on TFT)
//  Note: not all TFT's support reads

// reminder: Blue Pill SPI pins are SCK=A5, MISO = A6, MOSI=A7

// Can use Ardiuno pin references, arbitrary allocation, TFT_eSPI controls chip select
#define TFT_CS   A0 // Chip select control pin to TFT CS
#define TFT_DC   A1 // Data Command control pin to TFT DC (may be labelled RS = Register Select)
#define TFT_RST  A2 // Reset pin to TFT RST (or RESET)

//#define TFT_RST  -1   // Set TFT_RST to -1 if the display RESET is connected to processor reset
                        // Use an Arduino pin for initial testing as connecting to processor reset
                        // may not work (due to variations in display design)

#define TOUCH_CS A4 // Connect to T_CS (Touch controller chip select) if XPT2046 is connected to SPI bus
                    // You may need to wire the touch controller to the processor in addition to the TFT
                    // For example on some boards the pins are labelled:
                    // T_IRQ - no connection to processor
                    // T_DO  - connect to processor MISO
                    // T_DIN - connect to processor MOSI
                    // T_CS  - connect to processor pin specified by TOUCH_CS above
                    // T_SCK - connect to processor SCK

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

// STM32 support for smooth fonts via program memory (FLASH) arrays
#define SMOOTH_FONT

// Assuming the processor clock is 72MHz:
#define SPI_FREQUENCY  36000000   // 36MHz SPI clock
//#define SPI_FREQUENCY  18000000   // 18MHz SPI clock

#define SPI_READ_FREQUENCY  12000000 // Reads need a slower SPI clock

#define SPI_TOUCH_FREQUENCY  2500000 // Must be very slow

// This has no effect, transactions are automatically enabled for STM32
//#define SUPPORT_TRANSACTIONS
