#include "otto_emoji_display.h"

#include <esp_log.h>

#include <algorithm>
#include <cstring>

#define TAG "OttoEmojiDisplay"

// 表情映射表 - 将原版21种表情映射到现有6个GIF
const OttoEmojiDisplay::EmotionMap OttoEmojiDisplay::emotion_maps_[] = {
    // 中性/平静类表情 -> staticstate
    {"neutral", &staticstate},
    {"relaxed", &staticstate},
    {"sleepy", &staticstate},

    // 积极/开心类表情 -> happy
    {"happy", &happy},
    {"laughing", &happy},
    {"funny", &happy},
    {"loving", &happy},
    {"confident", &happy},
    {"winking", &happy},
    {"cool", &happy},
    {"delicious", &happy},
    {"kissy", &happy},
    {"silly", &happy},

    // 悲伤类表情 -> sad
    {"sad", &sad},
    {"crying", &sad},

    // 愤怒类表情 -> anger
    {"angry", &anger},

    // 惊讶类表情 -> scare
    {"surprised", &scare},
    {"shocked", &scare},

    // 思考/困惑类表情 -> buxue
    {"thinking", &buxue},
    {"confused", &buxue},
    {"embarrassed", &buxue},

    {nullptr, nullptr}  // 结束标记
};

OttoEmojiDisplay::OttoEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                   int width, int height, int offset_x, int offset_y, bool mirror_x,
                                   bool mirror_y, bool swap_xy, DisplayFonts fonts)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                    fonts),
      emotion_gif_(nullptr) {
    // 设置Otto专用的暗黑主题
    current_theme_name_ = "dark";
    current_theme_ = {
        .background = lv_color_hex(0x000000),        // 纯黑背景
        .text = lv_color_hex(0xFFFFFF),              // 纯白文字
        .chat_background = lv_color_hex(0x111111),   // 深灰聊天背景
        .user_bubble = lv_color_hex(0x1A6C37),       // 深绿用户气泡
        .assistant_bubble = lv_color_hex(0x222222),  // 深灰助手气泡
        .system_bubble = lv_color_hex(0x1A1A1A),     // 深灰系统气泡
        .system_text = lv_color_hex(0xAAAAAA),       // 浅灰系统文字
        .border = lv_color_hex(0x333333),            // 深灰边框
        .low_battery = lv_color_hex(0xFF0000)        // 红色低电量
    };

    SetupGifContainer();
    ESP_LOGI(TAG, "Otto GIF表情显示初始化完成，使用暗黑主题");
}

void OttoEmojiDisplay::SetupGifContainer() {
    DisplayLockGuard lock(this);

    if (emotion_label_) {
        lv_obj_del(emotion_label_);
    }
    if (chat_message_label_) {
        lv_obj_del(chat_message_label_);
    }
    if (content_) {
        lv_obj_del(content_);
    }

    lv_obj_t* overlay_container = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(overlay_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(overlay_container, LV_HOR_RES, LV_HOR_RES);
    lv_obj_set_style_bg_opa(overlay_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(overlay_container, 0, 0);
    lv_obj_set_flex_grow(overlay_container, 1);
    lv_obj_center(overlay_container);

    emotion_gif_ = lv_gif_create(overlay_container);
    int gif_size = LV_HOR_RES;
    lv_obj_set_size(emotion_gif_, gif_size, gif_size);
    lv_obj_set_style_border_width(emotion_gif_, 0, 0);
    lv_obj_set_style_bg_opa(emotion_gif_, LV_OPA_TRANSP, 0);
    lv_obj_center(emotion_gif_);
    lv_gif_set_src(emotion_gif_, &staticstate);

    chat_message_label_ = lv_label_create(overlay_container);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.9);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(chat_message_label_, current_theme_.text, 0);
    lv_obj_set_style_border_width(chat_message_label_, 0, 0);

    lv_obj_set_style_bg_opa(chat_message_label_, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(chat_message_label_, current_theme_.background, 0);
    lv_obj_set_style_pad_ver(chat_message_label_, 5, 0);

    lv_obj_align(chat_message_label_, LV_ALIGN_BOTTOM_MID, 0, 0);

    ApplyThemeToStatusBar();

    ESP_LOGI(TAG, "Otto GIF容器创建完成，大小: %dx%d", gif_size, gif_size);
}

void OttoEmojiDisplay::ApplyThemeToStatusBar() {
    if (!status_bar_)
        return;

    lv_obj_set_style_bg_color(status_bar_, current_theme_.background, 0);
    lv_obj_set_style_text_color(status_bar_, current_theme_.text, 0);

    if (network_label_) {
        lv_obj_set_style_text_color(network_label_, current_theme_.text, 0);
    }
    if (status_label_) {
        lv_obj_set_style_text_color(status_label_, current_theme_.text, 0);
    }
    if (notification_label_) {
        lv_obj_set_style_text_color(notification_label_, current_theme_.text, 0);
    }
    if (mute_label_) {
        lv_obj_set_style_text_color(mute_label_, current_theme_.text, 0);
    }
    if (battery_label_) {
        lv_obj_set_style_text_color(battery_label_, current_theme_.text, 0);
    }

    if (container_) {
        lv_obj_set_style_bg_color(container_, current_theme_.background, 0);
        lv_obj_set_style_border_color(container_, current_theme_.border, 0);
    }

    auto screen = lv_screen_active();
    if (screen) {
        lv_obj_set_style_bg_color(screen, current_theme_.background, 0);
        lv_obj_set_style_text_color(screen, current_theme_.text, 0);
    }

    ESP_LOGI(TAG, "Otto主题应用完成");
}

void OttoEmojiDisplay::SetEmotion(const char* emotion) {
    if (!emotion || !emotion_gif_) {
        return;
    }

    DisplayLockGuard lock(this);

    for (const auto& map : emotion_maps_) {
        if (map.name && strcmp(map.name, emotion) == 0) {
            lv_gif_set_src(emotion_gif_, map.gif);
            ESP_LOGI(TAG, "设置表情: %s", emotion);
            return;
        }
    }

    lv_gif_set_src(emotion_gif_, &staticstate);
    ESP_LOGI(TAG, "未知表情'%s'，使用默认", emotion);
}

void OttoEmojiDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    if (chat_message_label_ == nullptr) {
        return;
    }

    if (content == nullptr || strlen(content) == 0) {
        lv_obj_add_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_label_set_text(chat_message_label_, content);
    lv_obj_clear_flag(chat_message_label_, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "设置聊天消息 [%s]: %s", role, content);
}
