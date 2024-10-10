/**
 * @file lv_templ.h
 *
 */

#ifndef ST7735S_H
#define ST7735S_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

/*********************
 *      DEFINES
 *********************/
#define DISP_BUF_SIZE (LV_HOR_RES_MAX * 40)

#define ST7735S_DC       CONFIG_LV_DISP_PIN_DC
#define ST7735S_RST      CONFIG_LV_DISP_PIN_RST
#define ST7735S_USE_RST  CONFIG_LV_DISP_USE_RST

#define ST7735S_INVERT_COLORS CONFIG_LV_INVERT_COLORS

// Defines are taken from
//      https://raw.githubusercontent.com/m5stack/M5StickC/master/src/utility/ST7735_Defines.h
// and are modified to fit to the M5StickC device, and are taken from
//      https://github.com/adafruit/Adafruit-ST7735-Library
//
#define ST7735_GREENTAB160x80 // For 160 x 80 display (BGR, inverted, 26 / 1 offset)
#define COLSTART            26
#define ROWSTART            1

// Delay between some initialisation commands
#define TFT_INIT_DELAY      0x80

#define TFT_NOP             0x00
#define TFT_SWRST           0x01

#define TFT_CASET           0x2A
#define TFT_PASET           0x2B
#define TFT_RAMWR           0x2C

#define TFT_RAMRD           0x2E
#define TFT_IDXRD           0x00

#define TFT_MADCTL          0x36
#define TFT_MAD_MY          0x80
#define TFT_MAD_MX          0x40
#define TFT_MAD_MV          0x20
#define TFT_MAD_ML          0x10
#define TFT_MAD_BGR         0x08
#define TFT_MAD_MH          0x04
#define TFT_MAD_RGB         0x00

#define TFT_INVOFF          0x20
#define TFT_INVON           0x21

// ST7735 specific commands used in init
#define ST7735_NOP          0x00
#define ST7735_SWRESET      0x01
#define ST7735_RDDID        0x04
#define ST7735_RDDST        0x09

#define ST7735_SLPIN        0x10
#define ST7735_SLPOUT       0x11
#define ST7735_PTLON        0x12
#define ST7735_NORON        0x13

#define ST7735_INVOFF       0x20
#define ST7735_INVON        0x21
#define ST7735_DISPOFF      0x28
#define ST7735_DISPON       0x29
#define ST7735_CASET        0x2A
#define ST7735_RASET        0x2B
#define ST7735_RAMWR        0x2C
#define ST7735_RAMRD        0x2E

#define ST7735_PTLAR        0x30
#define ST7735_VSCRDEF      0x33
#define ST7735_COLMOD       0x3A
#define ST7735_MADCTL       0x36
#define ST7735_VSCRSADD     0x37

#define ST7735_FRMCTR1      0xB1
#define ST7735_FRMCTR2      0xB2
#define ST7735_FRMCTR3      0xB3
#define ST7735_INVCTR       0xB4
#define ST7735_DISSET5      0xB6

#define ST7735_PWCTR1       0xC0
#define ST7735_PWCTR2       0xC1
#define ST7735_PWCTR3       0xC2
#define ST7735_PWCTR4       0xC3
#define ST7735_PWCTR5       0xC4
#define ST7735_VMCTR1       0xC5

#define ST7735_RDID1        0xDA
#define ST7735_RDID2        0xDB
#define ST7735_RDID3        0xDC
#define ST7735_RDID4        0xDD

#define ST7735_PWCTR6       0xFC

#define ST7735_GMCTRP1      0xE0
#define ST7735_GMCTRN1      0xE1

#define ST77XX_MADCTL_MY    0x80
#define ST77XX_MADCTL_MX    0x40
#define ST77XX_MADCTL_MV    0x20
#define ST77XX_MADCTL_ML    0x10
#define ST77XX_MADCTL_RGB   0x00
#define ST77XX_MADCTL_BGR   0x08

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void st7735s_init(void);
void st7735s_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map);
void st7735s_enable_backlight(bool backlight);
void st7735s_sleep_in(void);
void st7735s_sleep_out(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*ST7735S_H*/
