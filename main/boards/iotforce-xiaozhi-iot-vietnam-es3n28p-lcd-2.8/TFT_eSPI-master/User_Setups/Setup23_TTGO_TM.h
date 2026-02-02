// Setup for the TTGO TM (Music) ESP32 board with 2.4" ST7789V display
#define USER_SETUP_ID 23

// See SetupX_Template.h for all options available

#define ST7789_DRIVER

#define TFT_SDA_READ // Read from display, it only provides an SDA pin

#define TFT_MISO 19  // Must be defined even though it is not used
#define TFT_MOSI 23  // Connected to display SDA line
#define TFT_SCLK 18

#define TFT_CS   05
#define TFT_DC   16
#define TFT_RST  17

#define TOUCH_CS -1

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

//#define TFT_RGB_ORDER TFT_RGB  // Colour order Red-Green-Blue
#define TFT_RGB_ORDER TFT_BGR // Colour order Blue-Green-Red

// Black and white swap option
//#define TFT_INVERSION_ON
#define TFT_INVERSION_OFF

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

#define SPI_FREQUENCY  40000000     // This display also seems to work reliably at 80MHz
//#define SPI_FREQUENCY  80000000

#define SPI_READ_FREQUENCY  6000000 // 6 MHz is the maximum SPI read speed for the ST7789V
