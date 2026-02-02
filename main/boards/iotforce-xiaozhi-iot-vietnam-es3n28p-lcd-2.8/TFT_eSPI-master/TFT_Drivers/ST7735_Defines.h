// Change the width and height if required (defined in portrait mode)
// or use the constructor to over-ride defaults
#ifndef TFT_WIDTH
  #define TFT_WIDTH  128
#endif
#ifndef TFT_HEIGHT
  #define TFT_HEIGHT 160
#endif


// Enumerate the different configurations
#define INITR_GREENTAB       0x0
#define INITR_REDTAB         0x1
#define INITR_BLACKTAB       0x2 // Display with no offsets
#define INITR_GREENTAB2      0x3 // Use if you get random pixels on two edges of green tab display
#define INITR_GREENTAB3      0x4 // Use if you get random pixels on edge(s) of 128x128 screen
#define INITR_GREENTAB128    0x5 // Use if you only get part of 128x128 screen in rotation 0 & 1
#define INITR_GREENTAB160x80 0x6 // Use if you only get part of 128x128 screen in rotation 0 & 1
#define INITR_REDTAB160x80   0x7 // Added for https://www.aliexpress.com/item/ShengYang-1pcs-IPS-0-96-inch-7P-SPI-HD-65K-Full-Color-OLED-Module-ST7735-Drive/32918394604.html
#define INITR_ROBOTLCD       0x8
#define INITB                0xB


// Setup the tab color that will be used by the library setRotation() and setup command list
#if defined (ST7735_INITB)
  #define TAB_COLOUR INITB
  
#elif defined (ST7735_GREENTAB)
  #define TAB_COLOUR INITR_GREENTAB
  #define CGRAM_OFFSET
  
#elif defined (ST7735_GREENTAB2)
  #define TAB_COLOUR INITR_GREENTAB2
  #define CGRAM_OFFSET

#elif defined (ST7735_GREENTAB3)
  #define TAB_COLOUR INITR_GREENTAB3
  #define CGRAM_OFFSET

#elif defined (ST7735_GREENTAB128)
  #define TAB_COLOUR INITR_GREENTAB128
  #define CGRAM_OFFSET
 
#elif defined (ST7735_GREENTAB160x80)
  #define TAB_COLOUR INITR_GREENTAB160x80
  #define CGRAM_OFFSET

#elif defined (ST7735_ROBOTLCD)
  #define TAB_COLOUR INITR_ROBOTLCD
  #define CGRAM_OFFSET

#elif defined (ST7735_REDTAB160x80)
  #define TAB_COLOUR INITR_REDTAB160x80
  #define CGRAM_OFFSET
 
#elif defined (ST7735_REDTAB)
  #define TAB_COLOUR INITR_REDTAB
  
#elif defined (ST7735_BLACKTAB)
  #define TAB_COLOUR INITR_BLACKTAB

#else // Make sure it is not undefined
  #define TAB_COLOUR INITR_BLACKTAB
#endif


// Color definitions for backwards compatibility with old sketches
// use colour definitions like TFT_BLACK to make sketches more portable
#define ST7735_BLACK       0x0000      /*   0,   0,   0 */
#define ST7735_NAVY        0x000F      /*   0,   0, 128 */
#define ST7735_DARKGREEN   0x03E0      /*   0, 128,   0 */
#define ST7735_DARKCYAN    0x03EF      /*   0, 128, 128 */
#define ST7735_MAROON      0x7800      /* 128,   0,   0 */
#define ST7735_PURPLE      0x780F      /* 128,   0, 128 */
#define ST7735_OLIVE       0x7BE0      /* 128, 128,   0 */
#define ST7735_LIGHTGREY   0xC618      /* 192, 192, 192 */
#define ST7735_DARKGREY    0x7BEF      /* 128, 128, 128 */
#define ST7735_BLUE        0x001F      /*   0,   0, 255 */
#define ST7735_GREEN       0x07E0      /*   0, 255,   0 */
#define ST7735_CYAN        0x07FF      /*   0, 255, 255 */
#define ST7735_RED         0xF800      /* 255,   0,   0 */
#define ST7735_MAGENTA     0xF81F      /* 255,   0, 255 */
#define ST7735_YELLOW      0xFFE0      /* 255, 255,   0 */
#define ST7735_WHITE       0xFFFF      /* 255, 255, 255 */
#define ST7735_ORANGE      0xFD20      /* 255, 165,   0 */
#define ST7735_GREENYELLOW 0xAFE5      /* 173, 255,  47 */
#define ST7735_PINK        0xF81F


// Delay between some initialisation commands
#define TFT_INIT_DELAY 0x80


// Generic commands used by TFT_eSPI.cpp
#define TFT_NOP     0x00
#define TFT_SWRST   0x01

#define TFT_INVOFF  0x20
#define TFT_INVON   0x21

#define TFT_DISPOFF 0x28
#define TFT_DISPON  0x29

#define TFT_CASET   0x2A
#define TFT_PASET   0x2B
#define TFT_RAMWR   0x2C

#define TFT_RAMRD   0x2E
#define TFT_IDXRD   0x00 //0xDD // ILI9341 only, indexed control register read

#define TFT_MADCTL  0x36
#define TFT_MAD_MY  0x80
#define TFT_MAD_MX  0x40
#define TFT_MAD_MV  0x20
#define TFT_MAD_ML  0x10
#define TFT_MAD_BGR 0x08
#define TFT_MAD_MH  0x04
#define TFT_MAD_RGB 0x00

#ifndef TFT_RGB_ORDER
  #if defined(ST7735_BLACKTAB) || defined(ST7735_GREENTAB2) || defined(ST7735_INITB)
    #define TFT_MAD_COLOR_ORDER TFT_MAD_RGB
  #else
    #define TFT_MAD_COLOR_ORDER TFT_MAD_BGR
  #endif
#else
  #if (TFT_RGB_ORDER == 1)
    #define TFT_MAD_COLOR_ORDER TFT_MAD_RGB
  #else
    #define TFT_MAD_COLOR_ORDER TFT_MAD_BGR
  #endif
#endif

// ST7735 specific commands used in init
#define ST7735_NOP     0x00
#define ST7735_SWRESET 0x01
#define ST7735_RDDID   0x04
#define ST7735_RDDST   0x09

#define ST7735_SLPIN   0x10
#define ST7735_SLPOUT  0x11
#define ST7735_PTLON   0x12
#define ST7735_NORON   0x13

#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B // PASET
#define ST7735_RAMWR   0x2C
#define ST7735_RAMRD   0x2E

#define ST7735_PTLAR   0x30
#define ST7735_COLMOD  0x3A
#define ST7735_MADCTL  0x36

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5

#define ST7735_RDID1   0xDA
#define ST7735_RDID2   0xDB
#define ST7735_RDID3   0xDC
#define ST7735_RDID4   0xDD

#define ST7735_PWCTR6  0xFC

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1
