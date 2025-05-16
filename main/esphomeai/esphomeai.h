#ifndef ESPHOMEAI_H
#define ESPHOMEAI_H

#include <esp_lvgl_port.h>
#include "ha/ha.h"

class ESPHomeAI
{
public:
    ESPHomeAI();
    ~ESPHomeAI();
    static void SetupUI(lv_obj_t **screen, lv_obj_t **container_, 
                       lv_obj_t **status_bar_, lv_obj_t **content_);
    static void UpdateUI();

    void publish_device_state(const std::string& state);
private:
    HomeAssistant* ha_;
};

#endif // ESPHOMEAI_H