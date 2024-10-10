/**
 * @file ili9481.h
 */

#ifndef ILI9481_H
#define ILI9481_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>
#include <stdint.h>

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#include "../lvgl_helpers.h"

/*********************
 *      DEFINES
 *********************/
#define ILI9481_DC        CONFIG_LV_DISP_PIN_DC
#define ILI9481_RST       CONFIG_LV_DISP_PIN_RST
#define ILI9481_USE_RST   CONFIG_LV_DISP_USE_RST
#define ILI9481_BCKL      CONFIG_LV_DISP_PIN_BCKL

#define ILI9481_ENABLE_BACKLIGHT_CONTROL CONFIG_LV_ENABLE_BACKLIGHT_CONTROL
#define ILI9481_INVERT_COLORS CONFIG_LV_INVERT_COLORS
#define ILI9481_DISPLAY_ORIENTATION CONFIG_LV_DISPLAY_ORIENTATION

#if CONFIG_LV_BACKLIGHT_ACTIVE_LVL
  #define ILI9481_BCKL_ACTIVE_LVL 1
#else
  #define ILI9481_BCKL_ACTIVE_LVL 0
#endif

/*******************
 * ILI9481 REGS
*********************/

/* MIPI DCS Type1  */
#define ILI9481_CMD_NOP                             0x00
#define ILI9481_CMD_SOFTWARE_RESET                  0x01
#define ILI9481_CMD_READ_DISP_POWER_MODE            0x0A
#define ILI9481_CMD_READ_DISP_MADCTRL               0x0B    // bits 7:3 only
#define ILI9481_CMD_READ_DISP_PIXEL_FORMAT          0x0C
#define ILI9481_CMD_READ_DISP_IMAGE_MODE            0x0D
#define ILI9481_CMD_READ_DISP_SIGNAL_MODE           0x0E
#define ILI9481_CMD_READ_DISP_SELF_DIAGNOSTIC       0x0F    // bits 7:6 only
#define ILI9481_CMD_ENTER_SLEEP_MODE                0x10
#define ILI9481_CMD_SLEEP_OUT                       0x11
#define ILI9481_CMD_PARTIAL_MODE_ON                 0x12
#define ILI9481_CMD_NORMAL_DISP_MODE_ON             0x13
#define ILI9481_CMD_DISP_INVERSION_OFF              0x20
#define ILI9481_CMD_DISP_INVERSION_ON               0x21
#define ILI9481_CMD_DISPLAY_OFF                     0x28
#define ILI9481_CMD_DISPLAY_ON                      0x29
#define ILI9481_CMD_COLUMN_ADDRESS_SET              0x2A
#define ILI9481_CMD_PAGE_ADDRESS_SET                0x2B
#define ILI9481_CMD_MEMORY_WRITE                    0x2C
#define ILI9481_CMD_MEMORY_READ                     0x2E
#define ILI9481_CMD_PARTIAL_AREA                    0x30
#define ILI9481_CMD_VERT_SCROLL_DEFINITION          0x33
#define ILI9481_CMD_TEARING_EFFECT_LINE_OFF         0x34
#define ILI9481_CMD_TEARING_EFFECT_LINE_ON          0x35
#define ILI9481_CMD_MEMORY_ACCESS_CONTROL           0x36    // bits 7:3,1:0 only
#define ILI9481_CMD_VERT_SCROLL_START_ADDRESS       0x37
#define ILI9481_CMD_IDLE_MODE_OFF                   0x38
#define ILI9481_CMD_IDLE_MODE_ON                    0x39
#define ILI9481_CMD_COLMOD_PIXEL_FORMAT_SET         0x3A
#define ILI9481_CMD_WRITE_MEMORY_CONTINUE           0x3C
#define ILI9481_CMD_READ_MEMORY_CONTINUE            0x3E
#define ILI9481_CMD_SET_TEAR_SCANLINE               0x44
#define ILI9481_CMD_GET_SCANLINE                    0x45

#define ILI9481_DDB_START                           0xA1
#define ILI9481_DDB_CONTINUE                        0xA8

/* other */
#define ILI9481_CMD_ACCESS_PROTECT                  0xB0
#define ILI9481_CMD_LOW_POWER_CONTROL               0xB1
#define ILI9481_CMD_FRAME_MEMORY_ACCESS             0xB3
#define ILI9481_CMD_DISPLAY_MODE                    0xB4
#define ILI9481_CMD_DEVICE_CODE                     0xBF

#define ILI9481_CMD_PANEL_DRIVE                     0xC0
#define ILI9481_CMD_DISP_TIMING_NORMAL              0xC1
#define ILI9481_CMD_DISP_TIMING_PARTIAL             0xC2
#define ILI9481_CMD_DISP_TIMING_IDLE                0xC3
#define ILI9481_CMD_FRAME_RATE                      0xC5
#define ILI9481_CMD_INTERFACE_CONTROL               0xC6
#define ILI9481_CMD_GAMMA_SETTING                   0xC8

#define ILI9481_CMD_POWER_SETTING                   0xD0
#define ILI9481_CMD_VCOM_CONTROL                    0xD1
#define ILI9481_CMD_POWER_CONTROL_NORMAL            0xD2
#define ILI9481_CMD_POWER_CONTROL_IDEL              0xD3
#define ILI9481_CMD_POWER_CONTROL_PARTIAL           0xD4

#define ILI9481_CMD_NVMEM_WRITE                     0xE0
#define ILI9481_CMD_NVMEM_PROTECTION_KEY            0xE1
#define ILI9481_CMD_NVMEM_STATUS_READ               0xE2
#define ILI9481_CMD_NVMEM_PROTECTION                0xE3

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void ili9481_init(void);
void ili9481_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map);
void ili9481_enable_backlight(bool backlight);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*ILI9481_H*/
