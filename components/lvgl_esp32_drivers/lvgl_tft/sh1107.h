/**
 * @file lv_templ.h
 *
 */

#ifndef SH1107_H
#define SH1107_H

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
#define SH1107_DC       CONFIG_LV_DISP_PIN_DC
#define SH1107_RST      CONFIG_LV_DISP_PIN_RST
#define SH1107_USE_RST  CONFIG_LV_DISP_USE_RST

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void sh1107_init(void);
void sh1107_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map);
void sh1107_rounder(struct _disp_drv_t * disp_drv, lv_area_t *area);
void sh1107_set_px_cb(struct _disp_drv_t * disp_drv, uint8_t * buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
    lv_color_t color, lv_opa_t opa);
void sh1107_sleep_in(void);
void sh1107_sleep_out(void);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*SH1107_H*/
