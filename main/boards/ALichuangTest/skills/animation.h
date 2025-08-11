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

    // 画布相关方法 - 用于在UI顶层显示图片
    virtual void CreateCanvas();
    virtual void DestroyCanvas();
    virtual void DrawImageOnCanvas(int x, int y, int width, int height, const uint8_t* img_data);
    virtual bool HasCanvas() const { return canvas_ != nullptr; }

protected:
    // 画布对象 - 用于在顶层显示图片
    lv_obj_t* canvas_ = nullptr;
    void* canvas_buffer_ = nullptr;
            
    // 情感变化回调函数
    std::function<void(const std::string&)> emotion_callback_ = nullptr;
};
#endif
