#include "oled_display.h"
#include "assets/lang_config.h"
#include "lvgl_theme.h"
#include "lvgl_font.h"

#include <string>
#include <algorithm>
#include <cstring>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <esp_random.h>
#include <font_awesome.h>

#define TAG "OledDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_awesome_30_1);

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
    int width, int height, bool mirror_x, bool mirror_y)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    auto text_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_TEXT_FONT);
    auto icon_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_awesome_30_1);
    
    auto dark_theme = new LvglTheme("dark");
    dark_theme->set_text_font(text_font);
    dark_theme->set_icon_font(icon_font);
    dark_theme->set_large_icon_font(large_icon_font);

    auto& theme_manager = LvglThemeManager::GetInstance();
    theme_manager.RegisterTheme("dark", dark_theme);
    current_theme_ = dark_theme;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding OLED display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    // Note: SetupUI() should be called by Application::Initialize(), not in constructor
    // to ensure lvgl objects are created after the display is fully initialized.
}

void OledDisplay::SetupUI() {
    // Prevent duplicate calls - if already called, return early
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }
    
    Display::SetupUI();  // Mark SetupUI as called
    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }
}

OledDisplay::~OledDisplay() {
    if (blink_timer_ != nullptr) {
        lv_timer_del(blink_timer_);
        blink_timer_ = nullptr;
    }
    if (standby_cycle_timer_ != nullptr) {
        lv_timer_del(standby_cycle_timer_);
        standby_cycle_timer_ = nullptr;
    }

    if (content_ != nullptr) {
        lv_obj_del(content_);
    }

    bool is_128x64_layout = (top_bar_ != nullptr);
    if (status_bar_ != nullptr && is_128x64_layout) {
        status_label_ = nullptr;
        notification_label_ = nullptr;
        lv_obj_del(status_bar_);
    }
    if (top_bar_ != nullptr) {
        network_label_ = nullptr;
        mute_label_ = nullptr;
        battery_label_ = nullptr;
        lv_obj_del(top_bar_);
    }
    if (side_bar_ != nullptr) {
        if (!is_128x64_layout) {
            status_label_ = nullptr;
            notification_label_ = nullptr;
            network_label_ = nullptr;
            mute_label_ = nullptr;
            battery_label_ = nullptr;
        }
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

bool OledDisplay::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void OledDisplay::Unlock() {
    lvgl_port_unlock();
}

void OledDisplay::SetStatus(const char* status) {
    LvglDisplay::SetStatus(status);

    DisplayLockGuard lock(this);
    standby_cycle_mode_ = (status != nullptr && strcmp(status, Lang::Strings::STANDBY) == 0);
    force_eye_renderer_ = false;
    suppress_face_render_ = false;

    if (standby_cycle_mode_) {
        EnsureStandbyCycleTimer();
        standby_emotion_index_ = 0;
        ApplyEmotionVisual("neutral");
        if (standby_cycle_timer_ != nullptr) {
            lv_timer_resume(standby_cycle_timer_);
            lv_timer_set_period(standby_cycle_timer_, 10000);
        }
        return;
    }

    if (standby_cycle_timer_ != nullptr) {
        lv_timer_pause(standby_cycle_timer_);
    }

    if (status != nullptr && strcmp(status, Lang::Strings::INITIALIZING) == 0) {
        suppress_face_render_ = true;
        if (left_eye_ != nullptr) lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        if (right_eye_ != nullptr) lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        if (emotion_label_ != nullptr) lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (status != nullptr && strcmp(status, Lang::Strings::CONNECTING) == 0) {
        suppress_face_render_ = true;
        if (left_eye_ != nullptr) lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        if (right_eye_ != nullptr) lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        if (emotion_label_ != nullptr) lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (status != nullptr && strcmp(status, Lang::Strings::LISTENING) == 0) {
        force_eye_renderer_ = true;
        ApplyEmotionVisual("thinking");
        return;
    }

    if (status != nullptr && strcmp(status, Lang::Strings::SPEAKING) == 0) {
        force_eye_renderer_ = true;
        ApplyEmotionVisual("confident");
        return;
    }

    if (status != nullptr && strcmp(status, Lang::Strings::ERROR) == 0) {
        force_eye_renderer_ = true;
        ApplyEmotionVisual("crying");
        return;
    }
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }

    // Chat/message text area disabled by user preference.
    lv_label_set_text(chat_message_label_, "");
    if (content_right_ != nullptr) {
        lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
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

    /* Layer 1: Top bar - for status icons */
    top_bar_ = lv_obj_create(container_);
    lv_obj_set_size(top_bar_, LV_HOR_RES, 16);
    lv_obj_set_style_radius(top_bar_, 0, 0);
    lv_obj_set_style_bg_opa(top_bar_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top_bar_, 0, 0);
    lv_obj_set_style_pad_all(top_bar_, 0, 0);
    lv_obj_set_flex_flow(top_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(top_bar_, LV_SCROLLBAR_MODE_OFF);

    network_label_ = lv_label_create(top_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);

    lv_obj_t* right_icons = lv_obj_create(top_bar_);
    lv_obj_set_size(right_icons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right_icons, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_icons, 0, 0);
    lv_obj_set_style_pad_all(right_icons, 0, 0);
    lv_obj_set_flex_flow(right_icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right_icons, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    mute_label_ = lv_label_create(right_icons);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);

    battery_label_ = lv_label_create(right_icons);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);

    /* Layer 2: Status bar - for center text labels */
    status_bar_ = lv_obj_create(screen);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_scrollbar_mode(status_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_layout(status_bar_, LV_LAYOUT_NONE, 0);  // Use absolute positioning
    lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 0);  // Overlap with top_bar_

    notification_label_ = lv_label_create(status_bar_);
    // Keep center text clear of left/right icon zones.
    lv_obj_set_width(notification_label_, LV_HOR_RES - 52);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    // Keep scrolling status text away from Wi-Fi icon area.
    lv_obj_set_width(status_label_, LV_HOR_RES - 52);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);
    lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);

    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(content_, LV_FLEX_ALIGN_CENTER, 0);

    content_left_ = lv_obj_create(content_);
    lv_obj_set_size(content_left_, 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content_left_, 0, 0);
    lv_obj_set_style_border_width(content_left_, 0, 0);

    emotion_label_ = lv_label_create(content_left_);
    lv_obj_set_style_text_font(emotion_label_, large_icon_font, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_MICROCHIP_AI);
    lv_obj_center(emotion_label_);
    lv_obj_set_style_pad_top(emotion_label_, 8, 0);
    // Hide old emoji icon path in 128x64, we'll render esp32-eyes style face instead.
    lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);

    left_eye_ = lv_obj_create(content_);
    lv_obj_remove_style_all(left_eye_);
    lv_obj_set_style_bg_color(left_eye_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(left_eye_, LV_OPA_COVER, 0);
    lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_IGNORE_LAYOUT);

    right_eye_ = lv_obj_create(content_);
    lv_obj_remove_style_all(right_eye_);
    lv_obj_set_style_bg_color(right_eye_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(right_eye_, LV_OPA_COVER, 0);
    lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_IGNORE_LAYOUT);

    SetEyeState(0);
    // Prevent power-on overlap during early boot (before first status mapping).
    lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
    EnsureBlinkTimer();

    content_right_ = lv_obj_create(content_);
    lv_obj_set_size(content_right_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(content_right_, 0, 0);
    lv_obj_set_style_border_width(content_right_, 0, 0);
    lv_obj_set_flex_grow(content_right_, 1);
    lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);

    chat_message_label_ = lv_label_create(content_right_);
    lv_label_set_text(chat_message_label_, "");
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(chat_message_label_, width_ - 32);
    lv_obj_set_style_pad_top(chat_message_label_, 14, 0);

    // Start scrolling subtitle after a delay
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}

void OledDisplay::SetupUI_128x32() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_column(container_, 0, 0);

    /* Emotion label on the left side */
    content_ = lv_obj_create(container_);
    lv_obj_set_size(content_, 32, 32);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_radius(content_, 0, 0);

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, large_icon_font, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_MICROCHIP_AI);
    lv_obj_center(emotion_label_);
    EnsureBlinkTimer();

    /* Right side */
    side_bar_ = lv_obj_create(container_);
    lv_obj_set_size(side_bar_, width_ - 32, 32);
    lv_obj_set_flex_flow(side_bar_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(side_bar_, 0, 0);
    lv_obj_set_style_border_width(side_bar_, 0, 0);
    lv_obj_set_style_radius(side_bar_, 0, 0);
    lv_obj_set_style_pad_row(side_bar_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(side_bar_);
    lv_obj_set_size(status_bar_, width_ - 32, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_obj_set_style_pad_left(status_label_, 2, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_pad_left(notification_label_, 2, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);

    chat_message_label_ = lv_label_create(side_bar_);
    lv_obj_set_size(chat_message_label_, width_ - 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(chat_message_label_, 2, 0);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chat_message_label_, "");

    // Start scrolling subtitle after a delay
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000), LV_PART_MAIN);
}

void OledDisplay::SetEmotion(const char* emotion) {
    DisplayLockGuard lock(this);
    if (emotion_label_ == nullptr && left_eye_ == nullptr) {
        return;
    }

    if (standby_cycle_mode_) {
        return;
    }

    ApplyEmotionVisual(emotion);
}

void OledDisplay::BlinkTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<OledDisplay*>(lv_timer_get_user_data(timer));
    if (self == nullptr) {
        return;
    }

    // Optional: skip blink while right content panel is visible (active message region on 128x64).
    if (self->content_right_ != nullptr && !lv_obj_has_flag(self->content_right_, LV_OBJ_FLAG_HIDDEN)) {
        self->blink_phase_ = false;
        if (self->left_eye_ != nullptr && self->right_eye_ != nullptr) {
            self->SetEyeState(0);
        } else if (self->emotion_label_ != nullptr) {
            lv_obj_set_style_opa(self->emotion_label_, LV_OPA_COVER, 0);
        }
        lv_timer_set_period(timer, 2500 + (esp_random() % 2000));
        return;
    }

    if (self->neutral_wink_mode_) {
        self->SetNeutralWinkFace();
        lv_timer_set_period(timer, 3000);
        return;
    }

    if (!self->blink_phase_) {
        self->blink_phase_ = true;
        if (self->left_eye_ != nullptr && self->right_eye_ != nullptr) {
            self->SetEyeState(2);
        } else if (self->emotion_label_ != nullptr) {
            lv_label_set_text(self->emotion_label_, FONT_AWESOME_WINKING);
        }
        lv_timer_set_period(timer, 140);
    } else {
        self->blink_phase_ = false;
        if (self->left_eye_ != nullptr && self->right_eye_ != nullptr) {
            self->SetEyeState(0);
        } else if (self->emotion_label_ != nullptr) {
            lv_label_set_text(self->emotion_label_, self->current_emotion_utf8_.c_str());
        }
        lv_timer_set_period(timer, 4500 + (esp_random() % 4000));
    }
}

void OledDisplay::StandbyCycleTimerCallback(lv_timer_t* timer) {
    auto* self = static_cast<OledDisplay*>(lv_timer_get_user_data(timer));
    if (self == nullptr || !self->standby_cycle_mode_) {
        return;
    }

    static const char* kCycle[] = {
        "neutral", "happy", "laughing", "funny", "sad", "angry", "crying", "loving",
        "embarrassed", "surprised", "shocked", "thinking", "winking", "cool", "relaxed",
        "delicious", "kissy", "confident", "sleepy", "silly", "confused"
    };
    constexpr size_t kCount = sizeof(kCycle) / sizeof(kCycle[0]);

    self->standby_emotion_index_ = (self->standby_emotion_index_ + 1) % kCount;
    self->ApplyEmotionVisual(kCycle[self->standby_emotion_index_]);
}

void OledDisplay::EnsureBlinkTimer() {
    if (blink_timer_ != nullptr) {
        return;
    }
    blink_phase_ = false;
    if (emotion_label_ != nullptr) {
        current_emotion_utf8_ = lv_label_get_text(emotion_label_);
    }
    blink_timer_ = lv_timer_create(&OledDisplay::BlinkTimerCallback,
                                   4500 + (esp_random() % 4000),
                                   this);
}

void OledDisplay::EnsureStandbyCycleTimer() {
    if (standby_cycle_timer_ != nullptr) {
        return;
    }
    standby_cycle_timer_ = lv_timer_create(&OledDisplay::StandbyCycleTimerCallback,
                                           10000,
                                           this);
}

void OledDisplay::ApplyEyePatternOpen(int pattern) {
    if (left_eye_ == nullptr || right_eye_ == nullptr) {
        return;
    }

    constexpr int kEyeXOffset = 0;   // move 4px right
    constexpr int kEyeYOffset = -10; // move another 5px up

    switch (pattern) {
        case 0: // rounded normal
            lv_obj_set_pos(left_eye_, 18 + kEyeXOffset, 18 + kEyeYOffset);
            lv_obj_set_size(left_eye_, 34, 28);
            lv_obj_set_pos(right_eye_, 76 + kEyeXOffset, 18 + kEyeYOffset);
            lv_obj_set_size(right_eye_, 34, 28);
            lv_obj_set_style_radius(left_eye_, 8, 0);
            lv_obj_set_style_radius(right_eye_, 8, 0);
            break;
        case 1: // wide eyes
            lv_obj_set_pos(left_eye_, 14 + kEyeXOffset, 20 + kEyeYOffset);
            lv_obj_set_size(left_eye_, 40, 22);
            lv_obj_set_pos(right_eye_, 74 + kEyeXOffset, 20 + kEyeYOffset);
            lv_obj_set_size(right_eye_, 40, 22);
            lv_obj_set_style_radius(left_eye_, 6, 0);
            lv_obj_set_style_radius(right_eye_, 6, 0);
            break;
        case 2: // sleepy
            lv_obj_set_pos(left_eye_, 18 + kEyeXOffset, 26 + kEyeYOffset);
            lv_obj_set_size(left_eye_, 34, 12);
            lv_obj_set_pos(right_eye_, 76 + kEyeXOffset, 26 + kEyeYOffset);
            lv_obj_set_size(right_eye_, 34, 12);
            lv_obj_set_style_radius(left_eye_, 6, 0);
            lv_obj_set_style_radius(right_eye_, 6, 0);
            break;
        case 3: // angry tilt
        default:
            lv_obj_set_pos(left_eye_, 18 + kEyeXOffset, 16 + kEyeYOffset);
            lv_obj_set_size(left_eye_, 34, 22);
            lv_obj_set_pos(right_eye_, 76 + kEyeXOffset, 24 + kEyeYOffset);
            lv_obj_set_size(right_eye_, 34, 22);
            lv_obj_set_style_radius(left_eye_, 4, 0);
            lv_obj_set_style_radius(right_eye_, 4, 0);
            break;
    }
}

void OledDisplay::SetNeutralWinkFace() {
    if (left_eye_ == nullptr || right_eye_ == nullptr) {
        return;
    }

    constexpr int kEyeXOffset = 0;   // move 4px right
    constexpr int kEyeYOffset = -10; // move another 5px up

    // esp32-eyes style wink: left eye open, right eye mostly closed.
    lv_obj_set_pos(left_eye_, 18 + kEyeXOffset, 18 + kEyeYOffset);
    lv_obj_set_size(left_eye_, 34, 28);
    lv_obj_set_style_radius(left_eye_, 8, 0);

    lv_obj_set_pos(right_eye_, 76 + kEyeXOffset, 31 + kEyeYOffset);
    lv_obj_set_size(right_eye_, 34, 3);
    lv_obj_set_style_radius(right_eye_, 2, 0);
}

void OledDisplay::SetEyeState(int state) {
    if (left_eye_ == nullptr || right_eye_ == nullptr) {
        return;
    }

    if (state == 0) {
        ApplyEyePatternOpen(eye_pattern_);
        return;
    }

    constexpr int kEyeXOffset = 0;   // move 4px right
    constexpr int kEyeYOffset = -10; // move another 5px up

    int y = 28 + kEyeYOffset;
    int h = 10;
    if (state == 2) {
        y = 32 + kEyeYOffset;
        h = 2;
    }

    lv_obj_set_pos(left_eye_, 18 + kEyeXOffset, y);
    lv_obj_set_size(left_eye_, 34, h);
    lv_obj_set_pos(right_eye_, 76 + kEyeXOffset, y);
    lv_obj_set_size(right_eye_, 34, h);
    lv_obj_set_style_radius(left_eye_, 2, 0);
    lv_obj_set_style_radius(right_eye_, 2, 0);
}

void OledDisplay::UpdateEyePatternFromEmotion(const char* emotion) {
    if (emotion == nullptr) {
        eye_pattern_ = 0;
        return;
    }

    std::string e(emotion);
    if (e == "happy" || e == "laughing" || e == "smiling" || e == "confident") {
        eye_pattern_ = 1;
    } else if (e == "sad" || e == "sleepy" || e == "tired" || e == "thinking") {
        eye_pattern_ = 2;
    } else if (e == "angry" || e == "crying") {
        eye_pattern_ = 3;
    } else {
        eye_pattern_ = 0;
    }
}

void OledDisplay::ApplyEmotionVisual(const char* emotion) {
    if (suppress_face_render_) {
        if (left_eye_ != nullptr) lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        if (right_eye_ != nullptr) lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        if (emotion_label_ != nullptr) lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const char* utf8 = font_awesome_get_utf8(emotion);

    blink_phase_ = false;
    neutral_wink_mode_ = (emotion != nullptr && strcmp(emotion, "neutral") == 0);
    relaxed_eye_mode_ = (emotion != nullptr && strcmp(emotion, "relaxed") == 0);

    if (utf8 != nullptr && emotion_label_ != nullptr) {
        current_emotion_utf8_ = utf8;
        lv_label_set_text(emotion_label_, current_emotion_utf8_.c_str());
    } else if (emotion_label_ != nullptr) {
        current_emotion_utf8_ = FONT_AWESOME_NEUTRAL;
        lv_label_set_text(emotion_label_, current_emotion_utf8_.c_str());
    }

    // In standby loop mode (or explicit status override), force esp32-eyes renderer.
    if ((standby_cycle_mode_ || force_eye_renderer_) && left_eye_ != nullptr && right_eye_ != nullptr && emotion_label_ != nullptr) {
        neutral_wink_mode_ = false; // keep blink animation while cycling
        lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);
        UpdateEyePatternFromEmotion(emotion);
        SetEyeState(0);
        return;
    }

    // Outside standby loop: keep neutral on esp32-eyes renderer.
    if (neutral_wink_mode_ && left_eye_ != nullptr && right_eye_ != nullptr && emotion_label_ != nullptr) {
        lv_obj_clear_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);
        SetNeutralWinkFace();
        return;
    }

    if (left_eye_ != nullptr && right_eye_ != nullptr && emotion_label_ != nullptr) {
        lv_obj_add_flag(left_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(right_eye_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(emotion_label_, LV_OBJ_FLAG_HIDDEN);
    }

    UpdateEyePatternFromEmotion(emotion);
}

void OledDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    auto text_font = lvgl_theme->text_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
}
