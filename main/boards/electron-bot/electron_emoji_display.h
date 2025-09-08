#pragma once

#include <libs/gif/lv_gif.h>

#include "display/lcd_display.h"

// Electron Bot表情GIF声明 - 使用与Otto相同的6个表情
LV_IMAGE_DECLARE(staticstate);  // 静态状态/中性表情
LV_IMAGE_DECLARE(sad);          // 悲伤
LV_IMAGE_DECLARE(happy);        // 开心
LV_IMAGE_DECLARE(scare);        // 惊吓/惊讶
LV_IMAGE_DECLARE(buxue);        // 不学/困惑
LV_IMAGE_DECLARE(anger);        // 愤怒

/**
 * @brief Electron Bot GIF表情显示类
 * 继承LcdDisplay，添加GIF表情支持
 */
class ElectronEmojiDisplay : public SpiLcdDisplay {
public:
    /**
     * @brief 构造函数，参数与SpiLcdDisplay相同
     */
    ElectronEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height, int offset_x, int offset_y, bool mirror_x,
                         bool mirror_y, bool swap_xy);

    virtual ~ElectronEmojiDisplay() = default;

    // 重写表情设置方法
    virtual void SetEmotion(const char* emotion) override;

    // 重写聊天消息设置方法
    virtual void SetChatMessage(const char* role, const char* content) override;

private:
    void SetupGifContainer();

    lv_obj_t* emotion_gif_;  ///< GIF表情组件

    // 表情映射
    struct EmotionMap {
        const char* name;
        const lv_image_dsc_t* gif;
    };

    static const EmotionMap emotion_maps_[];
};