#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "lvgl_display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <lvgl.h>


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

    // esp32-eyes style face (used for standby/idle on OLED)
    lv_obj_t* left_eye_ = nullptr;
    lv_obj_t* right_eye_ = nullptr;
    int eye_pattern_ = 0;
    bool neutral_wink_mode_ = false;
    bool relaxed_eye_mode_ = false;
    bool standby_cycle_mode_ = false;
    bool force_eye_renderer_ = false;
    bool suppress_face_render_ = false;
    size_t standby_emotion_index_ = 0;

    // Idle blink overlay (single-OLED reuse): emoji-frame blink.
    lv_timer_t* blink_timer_ = nullptr;
    lv_timer_t* standby_cycle_timer_ = nullptr;
    bool blink_phase_ = false;
    std::string current_emotion_utf8_;

    static void BlinkTimerCallback(lv_timer_t* timer);
    static void StandbyCycleTimerCallback(lv_timer_t* timer);
    void EnsureBlinkTimer();
    void EnsureStandbyCycleTimer();
    void ApplyEyePatternOpen(int pattern);
    void SetEyeState(int state);
    void SetNeutralWinkFace();
    void UpdateEyePatternFromEmotion(const char* emotion);
    void ApplyEmotionVisual(const char* emotion);

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetupUI_128x64();
    void SetupUI_128x32();

public:
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, bool mirror_x, bool mirror_y);
    ~OledDisplay();

    virtual void SetupUI() override;
    virtual void SetStatus(const char* status) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetTheme(Theme* theme) override;
};

#endif // OLED_DISPLAY_H
