#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
#include <esp_timer.h>

#include <string>

class Display {
public:
    virtual ~Display();

    void SetupUI();
    void SetText(const std::string &text);
    void ShowNotification(const std::string &text);

    void UpdateDisplay();

    int width() const { return width_; }
    int height() const { return height_; }

protected:
    lv_disp_t *disp_ = nullptr;
    lv_font_t *font_ = nullptr;
    lv_obj_t *label_ = nullptr;
    lv_obj_t *notification_ = nullptr;
    esp_timer_handle_t notification_timer_ = nullptr;
    esp_timer_handle_t update_display_timer_ = nullptr;

    int width_ = 0;
    int height_ = 0;

    std::string text_;

    virtual void Lock() = 0;
    virtual void Unlock() = 0;
};

#endif
