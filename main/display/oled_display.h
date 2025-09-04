#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include "display.h"
#include <lvgl.h>

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

// 字体类型转换函数
inline const lv_font_t* to_lv_font(const void* font) {
    return static_cast<const lv_font_t*>(font);
}

class OledDisplay : public Display {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    // LVGL 相关元素
    lv_display_t *display_ = nullptr;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* content_left_ = nullptr;
    lv_obj_t* content_right_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;

    // LVGL UI 元素
    lv_obj_t *emotion_label_ = nullptr;
    lv_obj_t *network_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *notification_label_ = nullptr;
    lv_obj_t *mute_label_ = nullptr;
    lv_obj_t *battery_label_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    lv_obj_t* low_battery_popup_ = nullptr;
    lv_obj_t* low_battery_label_ = nullptr;

    // 从 Display 基类移过来的成员变量
    const char* battery_icon_ = nullptr;
    const char* network_icon_ = nullptr;
    bool muted_ = false;
    esp_timer_handle_t notification_timer_ = nullptr;

    DisplayFonts fonts_;

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetupUI_128x64();
    void SetupUI_128x32();

public:
    OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, bool mirror_x, bool mirror_y,
                DisplayFonts fonts);
    ~OledDisplay();

    // 重写 Display 基类的方法，提供 LVGL 实现
    virtual void SetStatus(const char* status) override;
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override;
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetIcon(const char* icon) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void UpdateStatusBar(bool update_all = false) override;
};

#endif // OLED_DISPLAY_H
