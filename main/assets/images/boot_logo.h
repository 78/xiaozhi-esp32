#ifndef BOOT_LOGO_H
#define BOOT_LOGO_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#elif defined(LV_BUILD_TEST)
#include "../lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

// 声明开机Logo图片
extern const lv_image_dsc_t boot_logo;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* BOOT_LOGO_H */

