#ifndef FT81X_H_
#define FT81X_H_

#include <stdint.h>

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#include "../lvgl_helpers.h"

void FT81x_init(void);

void FT81x_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map);

#endif /* FT81X_H_ */
