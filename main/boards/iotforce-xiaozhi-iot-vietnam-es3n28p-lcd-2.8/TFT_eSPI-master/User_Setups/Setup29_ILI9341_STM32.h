
        ///////////////////////////////////////////////////
        //  Setup for STM32 Nucleo and ILI9341 display   //
        ///////////////////////////////////////////////////
#define USER_SETUP_ID 29

// Last update by Bodmer: 28/11/19

// STM32 optimised functions are not yet compatible with STM32H743 processor.
// The STM32H743 does work with the slower generic processor drivers
//
// REMINDER - Nucleo-F743ZI and Nucleo-F743ZI2 have different pin port allocations
// and require appropriate selection in IDE. ^---- Note the extra 2 in part number!


// Define STM32 to invoke STM32 optimised driver
#define STM32

// Define the TFT display driver
#define ILI9341_DRIVER
//#define ILI9481_DRIVER

// MOSI and SCK do not need to be defined, connect:
//  - Arduino SCK  to TFT SCK
//  - Arduino MOSI to TFT SDI(may be marked SDA or MOSI)
// Typical Arduino SPI port 1 pins are (SCK=D13, MISO=D12, MOSI=D11) this is port pins PA5, PA6 and PA7 on Nucleo-F767ZI
//                 SPI port 2 pins are (SCK=D18, MISO=A7, MOSI=D17) this is port pins PB13, PC2 and PB15 on Nucleo-F767ZI

/*
#define TFT_SPI_PORT 1 // SPI 1 maximum clock rate is 55MHz
#define TFT_MOSI PA7
#define TFT_MISO PA6
#define TFT_SCLK PA5
//*/

/*
#define TFT_SPI_PORT 2 // SPI 2 maximum clock rate is 27MHz
#define TFT_MOSI D17
#define TFT_MISO A7
#define TFT_SCLK D18
//*/

/*
#define TFT_SPI_PORT 2 // SPI 2 maximum clock rate is 27MHz
#define TFT_MOSI PB15
#define TFT_MISO PC2
#define TFT_SCLK PB13
//*/

/*
#define TFT_SPI_PORT 2 // SPI 2 maximum clock rate is 27MHz
#define TFT_MOSI PB15
#define TFT_MISO PB14
#define TFT_SCLK PB13
//*/

// Can use Ardiuno pin references, arbitrary allocation, TFT_eSPI controls chip select
#define TFT_CS   D5 // Chip select control pin to TFT CS
#define TFT_DC   D6 // Data Command control pin to TFT DC (may be labelled RS = Register Select)
#define TFT_RST  D7 // Reset pin to TFT RST (or RESET)

// Alternatively, we can use STM32 port reference names PXnn
//#define TFT_CS   PE11 // Nucleo-F767ZI equivalent of D5
//#define TFT_DC   PE9  // Nucleo-F767ZI equivalent of D6
//#define TFT_RST  PF13 // Nucleo-F767ZI equivalent of D7

//#define TFT_RST  -1   // Set TFT_RST to -1 if the display RESET is connected to processor reset
                        // Use an Arduino pin for initial testing as connecting to processor reset
                        // may not work (pulse too short at power up?)

// Chip select for XPT2046 touch controller
#define TOUCH_CS D4

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

// STM32 support for smooth fonts via program memory (FLASH) arrays
#define SMOOTH_FONT


// Nucleo-F767ZI has a ~216MHZ CPU clock, this is divided by 4, 8, 16 etc.

#define SPI_FREQUENCY  27000000   // 27MHz SPI clock
//#define SPI_FREQUENCY  55000000   // 55MHz is over-clocking ILI9341 but seems to work reliably!

#define SPI_READ_FREQUENCY  15000000 // Reads need a slower SPI clock, probably ends up at 13.75MHz (CPU clock/16)

#define SPI_TOUCH_FREQUENCY  2500000 // Must be very slow

// This has no effect, transactions for STM32 are automatically enabled
#define SUPPORT_TRANSACTIONS
