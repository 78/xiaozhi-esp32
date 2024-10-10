/**
 * @file il3820.h
 *
 */

#ifndef JD79653A_H
#define JD79653A_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

void jd79653a_init();
void jd79653a_deep_sleep();

void jd79653a_lv_set_fb_cb(struct _disp_drv_t * disp_drv, uint8_t* buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
                                                 lv_color_t color, lv_opa_t opa);
void jd79653a_lv_rounder_cb(struct _disp_drv_t * disp_drv, lv_area_t *area);
void jd79653a_lv_fb_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);

void jd79653a_fb_set_full_color(uint8_t color);
void jd79653a_fb_full_update(uint8_t *data, size_t len);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // JD79653A_H
