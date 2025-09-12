#ifndef ESPLOG_DISPLAY_H_
#define ESPLOG_DISPLAY_H_

#include "display.h"

#include <string>

class EspLogDisplay : public Display { 
public:
    EspLogDisplay();
    ~EspLogDisplay();

    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification, int duration_ms = 3000);
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000);
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    
    // 音乐播放相关（无屏版本用日志模拟）
    virtual void SetMusicInfo(const char* info) override;
    virtual void start() override;
    virtual void stopFft() override;
    
    virtual inline void SetPreviewImage(const lv_img_dsc_t* image) {}
    virtual inline void SetTheme(const std::string& theme_name) {}
    virtual inline void UpdateStatusBar(bool update_all = false) override {}

protected:
    virtual inline bool Lock(int timeout_ms = 0) override { return true; } 
    virtual inline void Unlock() override {}
};

#endif
