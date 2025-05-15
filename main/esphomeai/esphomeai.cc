#include "eez_ui/ui/ui.h"
#include "eez_ui/ui/screens.h"
#include "esphomeai.h"

ESPHomeAI::ESPHomeAI()
{
}

void ESPHomeAI::SetupUI(lv_obj_t **screen, lv_obj_t **container_, lv_obj_t **status_bar_, lv_obj_t **content_)
{
    ui_init();
    *screen = objects.main;
    *container_ = objects.container_;
    *status_bar_ = objects.status_bar;
    *content_ = objects.content_;
}

void ESPHomeAI::UpdateUI()
{
    ui_tick();
}