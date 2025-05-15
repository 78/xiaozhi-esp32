#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

#include <esp_lvgl_port.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t img_setting48;
extern const lv_img_dsc_t img_light32;
extern const lv_img_dsc_t img_fan32;
extern const lv_img_dsc_t img_tv32;
extern const lv_img_dsc_t img_thermostat32;
extern const lv_img_dsc_t img_humidity32;

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_img_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

extern const ext_img_desc_t images[6];


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/