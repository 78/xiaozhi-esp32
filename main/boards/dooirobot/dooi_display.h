#ifndef CUSTOM_DISPLAY_H
#define CUSTOM_DISPLAY_H

#include "lvgl_display.h"

class DooiDisplay : public LvglDisplay {
public:
    DooiDisplay();
    ~DooiDisplay() override;

   virtual void SetStatus(const char* status) override;
   virtual void ShowNotification(const std::string &notification, int duration_ms = 3000) override;
   virtual void ShowNotification(const char* notification, int duration_ms = 3000) override;
   virtual void UpdateStatusBar(bool update_all = false) override;
   virtual void SetEmotion(const char* emotion) override;
   virtual void SetChatMessage(const char* role, const char* content) override;
   virtual void SetTheme(Theme* theme) override;
   virtual void SetPowerSaveMode(bool on) override;
   virtual void SetPreviewImage(std::unique_ptr<LvglImage> image) override;

protected:
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;
};

#endif // CUSTOM_DISPLAY_H