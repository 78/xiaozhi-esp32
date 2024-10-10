/**
    @file uc8151d.h
    @brief   GoodDisplay GDEW0154M09 e-paper display w/ FitiPower JD79653A
    @version 1.0
    @date    2020-08-28
    @author  Jackson Ming Hu <huming2207@gmail.com>


    @section LICENSE

    MIT License

    Copyright (c) 2020 Jackson Ming Hu

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute,
    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LVGL_DEMO_UC8151D_H
#define LVGL_DEMO_UC8151D_H

#include <lvgl.h>

void uc8151d_init();
void uc8151d_lv_set_fb_cb(struct _disp_drv_t *disp_drv, uint8_t *buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
                          lv_color_t color, lv_opa_t opa);

void uc8151d_lv_rounder_cb(struct _disp_drv_t *disp_drv, lv_area_t *area);
void uc8151d_lv_fb_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);

#endif //LVGL_DEMO_UC8151D_H
