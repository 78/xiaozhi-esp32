#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
#include <esp_timer.h>
#include <esp_log.h>

#include <string>

struct DisplayFonts {
    const lv_font_t* text_font = nullptr;
    const lv_font_t* icon_font = nullptr;
    const lv_font_t* emoji_font = nullptr;
};

class Display {
public:
    Display();
    virtual ~Display();

    virtual void SetStatus(const std::string &status);
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000);
    virtual void SetEmotion(const std::string &emotion);
    virtual void SetChatMessage(const std::string &role, const std::string &content);
    virtual void SetIcon(const char* icon);

    int width() const { return width_; }
    int height() const { return height_; }

protected:
    int width_ = 0;
    int height_ = 0;

    lv_display_t *display_ = nullptr;

    lv_obj_t *emotion_label_ = nullptr;
    lv_obj_t *network_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *notification_label_ = nullptr;
    lv_obj_t *mute_label_ = nullptr;
    lv_obj_t *battery_label_ = nullptr;
    const char* battery_icon_ = nullptr;
    const char* network_icon_ = nullptr;
    bool muted_ = false;

    esp_timer_handle_t notification_timer_ = nullptr;
    esp_timer_handle_t update_timer_ = nullptr;

    friend class DisplayLockGuard;
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;

    virtual void Update();
};


class DisplayLockGuard {
public:
    DisplayLockGuard(Display *display) : display_(display) {
        if (!display_->Lock(3000)) {
            ESP_LOGE("Display", "Failed to lock display");
        }
    }
    ~DisplayLockGuard() {
        display_->Unlock();
    }

private:
    Display *display_;
};

#endif
