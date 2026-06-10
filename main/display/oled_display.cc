#include "oled_display.h"
#include "animated_eyes.h"
#include "assets/lang_config.h"
#include "lvgl_theme.h"
#include "lvgl_font.h"

#include <string>
#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
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
    if (anim_timer_ != nullptr) {
        esp_timer_stop(anim_timer_);
        esp_timer_delete(anim_timer_);
        anim_timer_ = nullptr;
    }

    if (eye_draw_buf_ != nullptr) {
        lv_draw_buf_destroy(eye_draw_buf_);
        eye_draw_buf_ = nullptr;
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
    // In 128x64 mode with eye canvas, suppress all status text (clock, etc.)
    if (eye_canvas_ != nullptr) {
        return;
    }
    // Fall through to base class for 128x32 layout
    LvglDisplay::SetStatus(status);
}

void OledDisplay::ShowNotification(const char* notification, int duration_ms) {
    if (eye_canvas_ != nullptr) return;
    LvglDisplay::ShowNotification(notification, duration_ms);
}

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }

    // Replace all newlines with spaces
    std::string content_str = content;
    std::replace(content_str.begin(), content_str.end(), '\n', ' ');

    if (content_right_ == nullptr) {
        lv_label_set_text(chat_message_label_, content_str.c_str());
    } else {
        if (content == nullptr || content[0] == '\0') {
            lv_obj_add_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(chat_message_label_, content_str.c_str());
            lv_obj_remove_flag(content_right_, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void OledDisplay::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_pad_all(screen, 0, 0);

    /* Full-screen container for the eye canvas */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_radius(container_, 0, 0);
    lv_obj_set_scrollbar_mode(container_, LV_SCROLLBAR_MODE_OFF);

    // Create canvas for eye rendering using LVGL-managed draw buffer
    eye_canvas_ = lv_canvas_create(container_);
    eye_draw_buf_ = lv_draw_buf_create(EYE_CANVAS_W, EYE_CANVAS_H, LV_COLOR_FORMAT_I1, 0);
    lv_canvas_set_draw_buf(eye_canvas_, eye_draw_buf_);
    lv_canvas_set_palette(eye_canvas_, 0, lv_color32_make(0, 0, 0, 0xFF));          // index 0 = black
    lv_canvas_set_palette(eye_canvas_, 1, lv_color32_make(0xFF, 0xFF, 0xFF, 0xFF));  // index 1 = white
    lv_obj_align(eye_canvas_, LV_ALIGN_CENTER, 0, 0);

    // Initialize face state to neutral
    current_face_ = get_emotion_face(EMOTION_NEUTRAL);
    target_face_ = current_face_;
    transition_t_ = 256;

    // Draw initial face - pixel data starts after 8-byte palette
    {
        uint8_t* pixel_data = eye_draw_buf_->data + 8;
        int stride = eye_draw_buf_->header.stride;
        draw_face(pixel_data, EYE_CANVAS_W, EYE_CANVAS_H, stride, &current_face_);
    }
    lv_obj_invalidate(eye_canvas_);

    // Full-screen eye mode: no text labels needed.
    // Leave all label pointers as nullptr so base class methods skip them.
    network_label_ = nullptr;
    battery_label_ = nullptr;
    mute_label_ = nullptr;
    status_label_ = nullptr;
    notification_label_ = nullptr;
    chat_message_label_ = nullptr;

    // Start animation timer (~15 FPS = 66ms period)
    esp_timer_create_args_t timer_args = {
        .callback = AnimTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "eye_anim",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &anim_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(anim_timer_, 66000)); // 66ms = ~15fps

    // Low battery popup disabled - running on USB power, no battery connected
    low_battery_popup_ = nullptr;
    low_battery_label_ = nullptr;
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
    if (eye_canvas_ == nullptr) {
        // Fallback for 128x32 layout which still uses emotion_label_
        const char* utf8 = font_awesome_get_utf8(emotion);
        DisplayLockGuard lock(this);
        if (emotion_label_ == nullptr) return;
        if (utf8 != nullptr) {
            lv_label_set_text(emotion_label_, utf8);
        } else {
            lv_label_set_text(emotion_label_, FONT_AWESOME_NEUTRAL);
        }
        return;
    }

    // Set target emotion for animated transition
    EmotionPreset preset = emotion_string_to_preset(emotion);
    target_face_ = get_emotion_face(preset);
    transition_t_ = 0;
}

void OledDisplay::AnimTimerCallback(void* arg) {
    auto* self = static_cast<OledDisplay*>(arg);
    self->AnimationTick();
}

void OledDisplay::AnimationTick() {
    if (eye_canvas_ == nullptr) return;

    bool needs_redraw = false;

    // Emotion transition
    if (transition_t_ < 256) {
        transition_t_ += transition_speed_;
        if (transition_t_ > 256) transition_t_ = 256;
        face_lerp(&current_face_, &current_face_, &target_face_, transition_speed_);
        if (transition_t_ >= 256) {
            current_face_ = target_face_;
        }
        needs_redraw = true;
    }

    // Blink logic
    blink_timer_++;
    if (!blinking_ && blink_timer_ >= blink_interval_) {
        blinking_ = true;
        blink_phase_ = 0;
        pre_blink_lid_top_ = current_face_.right_eye.lid_top;
        blink_timer_ = 0;
        // Randomize next blink interval: 45-75 frames (~3-5s at 15fps)
        blink_interval_ = 45 + (rand() % 30);
    }

    if (blinking_) {
        blink_phase_++;
        int re_h = current_face_.right_eye.height;
        int le_h = current_face_.left_eye.height;
        if (blink_phase_ <= 4) {
            // Closing both eyes
            current_face_.right_eye.lid_top = pre_blink_lid_top_ +
                (re_h - pre_blink_lid_top_) * blink_phase_ / 4;
            current_face_.left_eye.lid_top = pre_blink_lid_top_ +
                (le_h - pre_blink_lid_top_) * blink_phase_ / 4;
        } else if (blink_phase_ <= 8) {
            // Opening both eyes
            int open_phase = blink_phase_ - 4;
            current_face_.right_eye.lid_top = re_h -
                (re_h - pre_blink_lid_top_) * open_phase / 4;
            current_face_.left_eye.lid_top = le_h -
                (le_h - pre_blink_lid_top_) * open_phase / 4;
        } else {
            current_face_.right_eye.lid_top = pre_blink_lid_top_;
            current_face_.left_eye.lid_top = pre_blink_lid_top_;
            blinking_ = false;
        }
        needs_redraw = true;
    }

    // Pupil micro-drift (subtle random movement every ~30 frames)
    drift_timer_++;
    if (drift_timer_ >= 30) {
        drift_timer_ = 0;
        drift_dx_ = (rand() % 3) - 1; // -1, 0, or 1
        drift_dy_ = (rand() % 3) - 1;
        needs_redraw = true;
    }

    if (needs_redraw) {
            RedrawEyes();
    }
}

void OledDisplay::RedrawEyes() {
    DisplayLockGuard lock(this);
    if (eye_canvas_ == nullptr || eye_draw_buf_ == nullptr) return;

    // Apply drift to both eyes for rendering
    FaceState render_face = current_face_;
    render_face.right_eye.pupil_dx += drift_dx_;
    render_face.right_eye.pupil_dy += drift_dy_;
    render_face.left_eye.pupil_dx += drift_dx_;
    render_face.left_eye.pupil_dy += drift_dy_;

    uint8_t* pixel_data = eye_draw_buf_->data + 8;  // skip 8-byte palette
    int stride = eye_draw_buf_->header.stride;
    draw_face(pixel_data, EYE_CANVAS_W, EYE_CANVAS_H, stride, &render_face);
    lv_obj_invalidate(eye_canvas_);
}

void OledDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    auto text_font = lvgl_theme->text_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
}
