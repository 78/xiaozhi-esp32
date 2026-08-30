#include "face_engine.h"

#define TAG "OledDisplay"

FaceEngine* face_engine_ = nullptr;


void OledDisplay::SetStatus(const char* status) {
    // Call base implementation (keeps logging + any future logic)
    //Display::SetStatus(status); //Uncomment in future releases if base class implementation is added

    if (!face_engine_)
        return;

    if (strcmp(status, Lang::Strings::STANDBY) == 0) {
        face_engine_->SetState(FaceState::Idle);
    } else if (strcmp(status, Lang::Strings::LISTENING) == 0) {
        face_engine_->SetState(FaceState::Listening);
    } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
        face_engine_->SetState(FaceState::Speaking);
    } else {
        face_engine_->SetState(FaceState::Idle);
    }
}

void OledDisplay::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_size(content_, 128, 48);
    lv_obj_set_flex_grow(content_, 1);

    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    face_engine_ = new FaceEngine();
    face_engine_->Init(content_);
}