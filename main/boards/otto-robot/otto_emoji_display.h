#pragma once

#include "display/lcd_display.h"

/**
 * @brief Otto机器人GIF表情显示类
 * 继承SpiLcdDisplay，通过EmojiCollection添加GIF表情支持
 */
class OttoEmojiDisplay : public SpiLcdDisplay {
   public:
    /**
     * @brief 构造函数，参数与SpiLcdDisplay相同
     */
    OttoEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy);

    virtual ~OttoEmojiDisplay() = default;
    virtual void SetStatus(const char* status) override;
    virtual void SetPreviewImage(std::unique_ptr<LvglImage> image) override;

   private:
    void InitializeOttoEmojis();
    void SetupPreviewImage();
};