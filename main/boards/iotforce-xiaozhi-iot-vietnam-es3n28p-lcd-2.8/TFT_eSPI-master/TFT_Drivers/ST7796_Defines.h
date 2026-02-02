// Change the width and height if required (defined in portrait mode)
// or use the constructor to over-ride defaults
#ifndef TFT_WIDTH
#define TFT_WIDTH  320
#endif

#ifndef TFT_HEIGHT
#define TFT_HEIGHT 480
#endif

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

#define TFT_MADCTL  0x36
#define TFT_MAD_MY  0x80
#define TFT_MAD_MX  0x40
#define TFT_MAD_MV  0x20
#define TFT_MAD_ML  0x10
#define TFT_MAD_BGR 0x08
#define TFT_MAD_MH  0x04
#define TFT_MAD_RGB 0x00

#ifdef TFT_RGB_ORDER
  #if (TFT_RGB_ORDER == 1)
    #define TFT_MAD_COLOR_ORDER TFT_MAD_RGB
  #else
    #define TFT_MAD_COLOR_ORDER TFT_MAD_BGR
  #endif
#else
  #define TFT_MAD_COLOR_ORDER TFT_MAD_BGR
#endif

// ST7796 specific commands
#define ST7796_NOP     0x00
#define ST7796_SWRESET 0x01
#define ST7796_RDDID   0x04
#define ST7796_RDDST   0x09

#define ST7796_SLPIN   0x10
#define ST7796_SLPOUT  0x11
#define ST7796_PTLON   0x12
#define ST7796_NORON   0x13

#define ST7796_RDMODE  0x0A
#define ST7796_RDMADCTL  0x0B
#define ST7796_RDPIXFMT  0x0C
#define ST7796_RDIMGFMT  0x0A
#define ST7796_RDSELFDIAG  0x0F

#define ST7796_INVOFF  0x20
#define ST7796_INVON   0x21

#define ST7796_DISPOFF 0x28
#define ST7796_DISPON  0x29

#define ST7796_CASET   0x2A
#define ST7796_PASET   0x2B
#define ST7796_RAMWR   0x2C
#define ST7796_RAMRD   0x2E

#define ST7796_PTLAR   0x30
#define ST7796_VSCRDEF 0x33
#define ST7796_MADCTL  0x36
#define ST7796_VSCRSADD 0x37
#define ST7796_PIXFMT  0x3A

#define ST7796_WRDISBV  0x51
#define ST7796_RDDISBV  0x52
#define ST7796_WRCTRLD  0x53

#define ST7796_FRMCTR1 0xB1
#define ST7796_FRMCTR2 0xB2
#define ST7796_FRMCTR3 0xB3
#define ST7796_INVCTR  0xB4
#define ST7796_DFUNCTR 0xB6

#define ST7796_PWCTR1  0xC0
#define ST7796_PWCTR2  0xC1
#define ST7796_PWCTR3  0xC2

#define ST7796_VMCTR1  0xC5
#define ST7796_VMCOFF  0xC6

#define ST7796_RDID4   0xD3

#define ST7796_GMCTRP1 0xE0
#define ST7796_GMCTRN1 0xE1

#define ST7796_MADCTL_MY  0x80
#define ST7796_MADCTL_MX  0x40
#define ST7796_MADCTL_MV  0x20
#define ST7796_MADCTL_ML  0x10
#define ST7796_MADCTL_RGB 0x00
#define ST7796_MADCTL_BGR 0x08
#define ST7796_MADCTL_MH  0x04
