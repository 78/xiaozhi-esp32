#ifndef ESPHOMEAI_H
#define ESPHOMEAI_H

#include <esp_lvgl_port.h>

class ESPHomeAI
{
public:
    ESPHomeAI();
    static void SetupUI(lv_obj_t **screen, lv_obj_t **container_, lv_obj_t **status_bar_, lv_obj_t **content_);

    static void UpdateUI();
};

#endif // ESPHOMEAI_H