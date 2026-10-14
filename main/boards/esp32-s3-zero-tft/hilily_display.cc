#include "hilily_display.h"
#include "application.h"
#include "device_state.h"

#include <cstdio>
#include <utility>

namespace {
void SetupText(lv_obj_t* label, int width) {
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xF5FAFF), 0);
}
}

void HiLilyDisplay::SetHomeData(std::string location, std::string condition,
                               std::string lunar_date, double temperature) {
    std::lock_guard<std::mutex> guard(home_mutex_);
    location_ = std::move(location);
    condition_ = std::move(condition);
    lunar_date_ = std::move(lunar_date);
    temperature_ = temperature;
    available_ = true;
    ++revision_;
}

void HiLilyDisplay::SetupUI() {
    SpiLcdDisplay::SetupUI();
    if (!Lock(200)) return;

    panel_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(panel_, 224, 154);
    lv_obj_align(panel_, LV_ALIGN_CENTER, 0, 12);
    lv_obj_set_style_bg_color(panel_, lv_color_hex(0x11243A), 0);
    lv_obj_set_style_bg_opa(panel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel_, 0, 0);
    lv_obj_set_style_radius(panel_, 14, 0);
    lv_obj_set_style_pad_all(panel_, 8, 0);
    lv_obj_set_flex_flow(panel_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel_, 9, 0);
    lv_obj_clear_flag(panel_, LV_OBJ_FLAG_SCROLLABLE);

    location_label_ = lv_label_create(panel_);
    temperature_label_ = lv_label_create(panel_);
    condition_label_ = lv_label_create(panel_);
    lunar_label_ = lv_label_create(panel_);
    for (auto* label : {location_label_, temperature_label_, condition_label_, lunar_label_}) {
        SetupText(label, 207);
    }
    lv_label_set_text(location_label_, "Hi Lily");
    lv_label_set_text(temperature_label_, "--°C");
    lv_label_set_text(condition_label_, "Đang tải thời tiết...");
    lv_label_set_text(lunar_label_, "Âm lịch: --/--");
    lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
    Unlock();
}

void HiLilyDisplay::UpdateStatusBar(bool update_all) {
    SpiLcdDisplay::UpdateStatusBar(update_all);
    if (!panel_) return;

    const bool idle = Application::GetInstance().GetDeviceState() == kDeviceStateIdle;
    std::string loc, condition, lunar;
    double temp = 0;
    bool data_ready;
    uint32_t revision;
    {
        std::lock_guard<std::mutex> guard(home_mutex_);
        data_ready = available_;
        revision = revision_;
        if (revision != rendered_revision_) {
            loc = location_;
            condition = condition_;
            lunar = lunar_date_;
            temp = temperature_;
        }
    }

    if (!Lock(50)) return;
    if (idle != was_idle_) {
        if (idle) {
            lv_obj_remove_flag(panel_, LV_OBJ_FLAG_HIDDEN);
            if (emoji_box_) lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(panel_, LV_OBJ_FLAG_HIDDEN);
            if (emoji_box_) lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
        }
        was_idle_ = idle;
    }
    if (data_ready && revision != rendered_revision_) {
        char number[32];
        std::snprintf(number, sizeof(number), "%.0f°C", temp);
        lv_label_set_text(location_label_, loc.c_str());
        lv_label_set_text(temperature_label_, number);
        lv_label_set_text(condition_label_, condition.c_str());
        lv_label_set_text_fmt(lunar_label_, "Âm lịch: %s", lunar.c_str());
        rendered_revision_ = revision;
    }
    Unlock();
}
