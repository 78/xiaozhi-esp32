/**
 * @file lv_templ.h
 *
 */

#ifndef GC9A01_H
#define GC9A01_H

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
#define GC9A01_DC       CONFIG_LV_DISP_PIN_DC
#define GC9A01_RST      CONFIG_LV_DISP_PIN_RST
#define GC9A01_USE_RST  CONFIG_LV_DISP_USE_RST
#define GC9A01_BCKL     CONFIG_LV_DISP_PIN_BCKL

#define GC9A01_ENABLE_BACKLIGHT_CONTROL CONFIG_LV_ENABLE_BACKLIGHT_CONTROL

#if CONFIG_LV_BACKLIGHT_ACTIVE_LVL
  #define GC9A01_BCKL_ACTIVE_LVL 1
#else
  #define GC9A01_BCKL_ACTIVE_LVL 0
#endif

#define GC9A01_INVERT_COLORS CONFIG_LV_INVERT_COLORS

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void GC9A01_init(void);
void GC9A01_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map);
void GC9A01_enable_backlight(bool backlight);
void GC9A01_sleep_in(void);
void GC9A01_sleep_out(void);

/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*GC9A01_H*/
