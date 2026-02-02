#ifndef TFT_WIDTH
  #define TFT_WIDTH 128
#endif
#ifndef TFT_HEIGHT
  #define TFT_HEIGHT 128
#endif

// Delay between some initialisation commands
#define TFT_INIT_DELAY 0x80

// Generic commands used by TFT_eSPI.cpp
#define TFT_NOP     0x00
#define TFT_SWRST   TFT_NOP
#define TFT_CASET   0x15 // SETCOLUMN
#define TFT_PASET   0x75 // SETROW
#define TFT_RAMWR   0x5C // WRITERAM
#define TFT_RAMRD   0x5D // READRAM
#define TFT_IDXRD   TFT_NOP
#define TFT_INVOFF  0xA6 // NORMALDISPLAY
#define TFT_INVON   0xA7 // INVERTDISPLAY
