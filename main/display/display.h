#ifndef DISPLAY_H
#define DISPLAY_H

#include "emoji_collection.h"

#ifndef CONFIG_USE_EMOTE_MESSAGE_STYLE
#define HAVE_LVGL 1
#include <lvgl.h>
#endif

#include <esp_timer.h>
#include <esp_log.h>
#include <esp_pm.h>

#include <string>
#include <chrono>

// Forward declarations for safe casting return types
class LvglDisplay;
class OledDisplay;
class LcdDisplay;
namespace emote { class EmoteDisplay; }

class Theme {
public:
    Theme(const std::string& name) : name_(name) {}
    virtual ~Theme() = default;

    inline std::string name() const { return name_; }
private:
    std::string name_;
};

class Display {
public:
    Display();
    virtual ~Display();

    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000);
    virtual void SetEmotion(const char* emotion);
    virtual void SetChatMessage(const char* role, const char* content);
    virtual void ClearChatMessages();
    virtual void SetTheme(Theme* theme);
    virtual Theme* GetTheme() { return current_theme_; }
    virtual void UpdateStatusBar(bool update_all = false);
    virtual void SetPowerSaveMode(bool on);
    virtual void SetupUI() { 
        setup_ui_called_ = true;
    }

    inline int width() const { return width_; }
    inline int height() const { return height_; }
    inline bool IsSetupUICalled() const { return setup_ui_called_; }

    // Type checking methods (for boolean checks)
    virtual bool IsLvglDisplay() const { return false; }
    virtual bool IsOledDisplay() const { return false; }
    virtual bool IsLcdDisplay() const { return false; }
    virtual bool IsEmoteDisplay() const { return false; }

    // Safe casting methods - returns nullptr if type doesn't match
    virtual class LvglDisplay* AsLvglDisplay() { return nullptr; }
    virtual const class LvglDisplay* AsLvglDisplay() const { return nullptr; }
    virtual class OledDisplay* AsOledDisplay() { return nullptr; }
    virtual const class OledDisplay* AsOledDisplay() const { return nullptr; }
    virtual class LcdDisplay* AsLcdDisplay() { return nullptr; }
    virtual const class LcdDisplay* AsLcdDisplay() const { return nullptr; }
    virtual class emote::EmoteDisplay* AsEmoteDisplay() { return nullptr; }
    virtual const class emote::EmoteDisplay* AsEmoteDisplay() const { return nullptr; }

protected:
    int width_ = 0;
    int height_ = 0;
    bool setup_ui_called_ = false;  // Track if SetupUI() has been called

    Theme* current_theme_ = nullptr;

    friend class DisplayLockGuard;
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;
};


class DisplayLockGuard {
public:
    DisplayLockGuard(Display *display) : display_(display) {
        if (!display_->Lock(30000)) {
            ESP_LOGE("Display", "Failed to lock display");
        }
    }
    ~DisplayLockGuard() {
        display_->Unlock();
    }

private:
    Display *display_;
};

class NoDisplay : public Display {
private:
    virtual bool Lock(int timeout_ms = 0) override {
        return true;
    }
    virtual void Unlock() override {}
};

#endif
