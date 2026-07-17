#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "lvgl_display.h"
#include "animated_eyes.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>


class OledDisplay : public LvglDisplay {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_obj_t* top_bar_ = nullptr;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* content_left_ = nullptr;
    lv_obj_t* content_right_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t *emotion_label_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;

    // Animated eyes members
    lv_obj_t* eye_canvas_ = nullptr;
    static constexpr int EYE_CANVAS_W = 128;
    static constexpr int EYE_CANVAS_H = 64;
    lv_draw_buf_t* eye_draw_buf_ = nullptr;

    FaceState current_face_;
    FaceState target_face_;
    int transition_t_ = 256;  // 0..256, 256 = done
    int transition_speed_ = 20; // increment per frame

    // Blink state
    int blink_timer_ = 0;
    int blink_interval_ = 200; // frames until next blink (~3-5s at 15fps)
    bool blinking_ = false;
    int blink_phase_ = 0;  // 0..8 close, 8..16 open
    int16_t pre_blink_lid_top_ = 0;

    // Pupil micro-drift
    int16_t drift_dx_ = 0;
    int16_t drift_dy_ = 0;
    int drift_timer_ = 0;

    esp_timer_handle_t anim_timer_ = nullptr;

    void AnimationTick();
    static void AnimTimerCallback(void* arg);
    void RedrawEyes();

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetupUI_128x64();
    void SetupUI_128x32();

public:
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, bool mirror_x, bool mirror_y);
    ~OledDisplay();

    virtual void SetupUI() override;
    virtual void SetStatus(const char* status) override;
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetTheme(Theme* theme) override;
};

#endif // OLED_DISPLAY_H
