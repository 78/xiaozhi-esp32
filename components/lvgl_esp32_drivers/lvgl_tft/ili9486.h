/**
 * @file ili9486.h
 *
 */

#ifndef ILI9486_H
#define ILI9486_H

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
#include "../lvgl_helpers.h"

/*********************
 *      DEFINES
 *********************/
#define ILI9486_DC      CONFIG_LV_DISP_PIN_DC
#define ILI9486_RST     CONFIG_LV_DISP_PIN_RST
#define ILI9486_USE_RST CONFIG_LV_DISP_USE_RST
#define ILI9486_BCKL    CONFIG_LV_DISP_PIN_BCKL

#define ILI9486_ENABLE_BACKLIGHT_CONTROL CONFIG_LV_ENABLE_BACKLIGHT_CONTROL

#if CONFIG_LV_BACKLIGHT_ACTIVE_LVL
  #define ILI9486_BCKL_ACTIVE_LVL 1
#else
  #define ILI9486_BCKL_ACTIVE_LVL 0
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void ili9486_init(void);
void ili9486_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map);
void ili9486_enable_backlight(bool backlight);

/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ILI9486_H*/
