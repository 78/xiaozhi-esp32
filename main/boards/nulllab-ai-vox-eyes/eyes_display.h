#pragma once
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <libs/gif/lv_gif.h>

#include "display/lcd_display.h"
#include "misc/lv_area.h"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

LV_IMG_DECLARE(staticstate);
LV_IMG_DECLARE(sad);
LV_IMG_DECLARE(happy);
LV_IMG_DECLARE(scare);
LV_IMG_DECLARE(buxue);
LV_IMG_DECLARE(anger);

// 全屏眼睛（没有状态栏）
#define FULL_SCREEN_EYES (0)

class EyesDisplay : public SpiLcdDisplay {
  private:
    lv_obj_t *eyes_emotion_gif_ = nullptr;
    lv_obj_t *eyes_message_label_ = nullptr;

  public:
    EyesDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, int offset_x,
                int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                        {
                            .text_font = &font_puhui_16_4,
                            .icon_font = &font_awesome_16_4,
                        }) {

        // 启用暗色模式，匹配眼睛动画
        // SetTheme("dark");

        DisplayLockGuard lock(this);
        auto screen = lv_screen_active();

#if FULL_SCREEN_EYES
        lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
        eyes_emotion_gif_ = lv_gif_create(screen);
#else
        lv_obj_set_scrollbar_mode(container_, LV_SCROLLBAR_MODE_OFF);
        eyes_emotion_gif_ = lv_gif_create(container_);
#endif

        // 动画眼睛
        lv_obj_set_size(eyes_emotion_gif_, LV_HOR_RES, LV_HOR_RES);
        lv_obj_set_style_bg_opa(eyes_emotion_gif_, LV_OPA_TRANSP, 0);
        lv_gif_set_src(eyes_emotion_gif_, &staticstate);
        lv_obj_center(eyes_emotion_gif_);

        eyes_message_label_ = lv_label_create(screen);
        lv_label_set_text(eyes_message_label_, "");
        lv_obj_set_width(eyes_message_label_, LV_HOR_RES * 0.9);
        lv_label_set_long_mode(eyes_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_align(eyes_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(eyes_message_label_, lv_color_white(), 0);
        lv_obj_align(eyes_message_label_, LV_ALIGN_BOTTOM_MID, 0, -10);
    }

    virtual void SetChatMessage(const char *role, const char *content) override {
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
        SpiLcdDisplay::SetChatMessage(role, content);
#else
        DisplayLockGuard lock(this);
        if (chat_message_label_ != nullptr) {
            lv_label_set_text(eyes_message_label_, content);
        }
#endif
    }

    virtual void SetEmotion(const char *emotion) override {
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
        SpiLcdDisplay::SetEmotion(emotion);
#else
        struct Emotion {
            const lv_img_dsc_t *gif;
            const char *text;
        };

        static const std::vector<Emotion> emotions = {
            {&staticstate, "neutral"}, {&happy, "happy"},     {&happy, "laughing"}, {&happy, "funny"}, {&sad, "sad"},
            {&anger, "angry"},         {&scare, "surprised"}, {&buxue, "confused"},
        };

        std::string_view emotion_view(emotion);
        auto it = std::find_if(emotions.begin(), emotions.end(),
                               [&emotion_view](const Emotion &e) { return e.text == emotion_view; });

        DisplayLockGuard lock(this);
        if (eyes_emotion_gif_ == nullptr)
            return;

        if (it != emotions.end()) {
            lv_gif_set_src(eyes_emotion_gif_, it->gif);
        } else {
            lv_gif_set_src(eyes_emotion_gif_, &staticstate);
        }
#endif
    }
};
