#pragma once
#include "display.h"
#include <vector>
#include <memory>

class DisplayManager : public Display {
private:
    static std::vector<Display*> displays_;
    static Display* primary_display_;

public:
    // 添加显示设备
    static void AddDisplay(Display* display, bool is_primary = false);
    
    // 移除显示设备
    static void RemoveDisplay(Display* display);
    
    // 获取显示设备数量
    static size_t GetDisplayCount();
    
    // 获取主显示设备
    static Display* GetPrimaryDisplay();
    
    // 获取所有显示设备
    static const std::vector<Display*>& GetAllDisplays();
    
    // Display 接口实现 ，应用到所有屏幕
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetIcon(const char* icon) override;
    virtual void SetPreviewImage(const lv_img_dsc_t* img_dsc) override;
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void SetTheme(const std::string& theme_name) override;
    virtual void SetStatus(const char* status) override;
    
    virtual void ShowNotification(const char* message);
    
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;
};