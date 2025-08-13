#ifndef ANIMA_DISPLAY_H
#define ANIMA_DISPLAY_H
#include "display/lcd_display.h"
#include <functional>

class AnimaDisplay : public LcdDisplay {
public:
    AnimaDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);

    // 情感变化回调
    virtual void OnEmotionChanged(std::function<void(const std::string&)> callback) { 
        emotion_callback_ = callback; 
    }
    virtual void SetEmotion(const char* emotion) override;

    // UI方法重载为空 - 因为使用canvas替代传统UI
    virtual void SetStatus(const char* status) override {}
    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override {}
    virtual void SetChatMessage(const char* role, const char* content) override {}
    virtual void SetIcon(const char* icon) override {}
    virtual void SetPreviewImage(const lv_img_dsc_t* image) override {}
    virtual void UpdateStatusBar(bool update_all = false) override {}
    virtual void SetPowerSaveMode(bool on) override {}
    virtual void SetTheme(const std::string& theme_name) override;

    // 画布相关方法 - 用于在UI顶层显示图片
    virtual void CreateCanvas();
    virtual void DestroyCanvas();
    virtual void DrawImageOnCanvas(int x, int y, int width, int height, const uint8_t* img_data);
    virtual bool HasCanvas() const { return canvas_ != nullptr; }

protected:
    // 重载SetupUI为简化版本，避免复杂UI初始化
    void SetupUI();

    // 画布对象 - 用于在顶层显示图片
    lv_obj_t* canvas_ = nullptr;
    void* canvas_buffer_ = nullptr;
            
    // 情感变化回调函数
    std::function<void(const std::string&)> emotion_callback_ = nullptr;
};
#endif
