        //////////////////////////////////////////////////////////////
        //           Setup for STM32 and ILI9341 display            //
        //////////////////////////////////////////////////////////////
#define USER_SETUP_ID 35
// Last update by Bodmer: 28/3/20

// Define STM32 to invoke STM32 optimised driver
#define STM32

// Define if Port A (B, C or D) pins 0-7 are used for data bus bits 0-7
// this will improve rendering performance by a factor of ~8x
#define STM_PORTA_DATA_BUS
//#define STM_PORTB_DATA_BUS // Pins below must re re-allocated to use this option
//#define STM_PORTC_DATA_BUS // Pins below must re re-allocated to use this option
//#define STM_PORTD_DATA_BUS // Pins below must re re-allocated to use this option

// Tell the library to use 8-bit parallel mode (otherwise SPI is assumed)
#define TFT_PARALLEL_8_BIT

// Define ONE of the the TFT display drivers
#define ILI9341_DRIVER
//#define ILI9481_DRIVER
//#define ILI9486_DRIVER
//#define ILI9488_DRIVER
//#define ST7796_DRIVER

#define TFT_CS  PB7 // Chip select control pin
#define TFT_DC  PB8 // Data Command control pin
#define TFT_RST PB0 // Reset pin
#define TFT_WR  PB9 // Write strobe control pin
#define TFT_RD  PB1 // Read pin

#define TFT_D0 PA0 // 8-bit Port A parallel bus to TFT
#define TFT_D1 PA1
#define TFT_D2 PA2
#define TFT_D3 PA3
#define TFT_D4 PA4
#define TFT_D5 PA5
#define TFT_D6 PA6
#define TFT_D7 PA7

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
