#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <esp_lvgl_port.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *container_;
    lv_obj_t *status_bar;
    lv_obj_t *content_;
    lv_obj_t *obj0;
    lv_obj_t *tmp_data_label_;
    lv_obj_t *obj1;
    lv_obj_t *obj2;
    lv_obj_t *humi_data_label_;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
};

void create_screen_main();
void tick_screen_main();

void create_screens();
void tick_screen(int screen_index);


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/