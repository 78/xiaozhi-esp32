#pragma once

#include "display/lcd_display.h"

/**
 * @brief Electron机器人GIF表情显示类
 * 继承SpiLcdDisplay，通过EmojiCollection添加GIF表情支持
 */
class ElectronEmojiDisplay : public SpiLcdDisplay {
   public:
    /**
     * @brief 构造函数，参数与SpiLcdDisplay相同
     */
    ElectronEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy);

    virtual ~ElectronEmojiDisplay() = default;
    virtual void SetStatus(const char* status) override;

   private:
    void InitializeElectronEmojis();
    void SetupChatLabel();
};