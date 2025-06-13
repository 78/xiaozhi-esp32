#pragma once

#include <libs/gif/lv_gif.h>
#include <esp_timer.h>

#include "display/lcd_display.h"
#include "otto_emoji_gif.h"

/**
 * @brief Otto机器人GIF表情显示类
 * 继承LcdDisplay，添加GIF表情支持
 */
class SparkbotEmojiDisplay : public SpiLcdDisplay {
public:
    /**
     * @brief 构造函数，参数与SpiLcdDisplay相同
     */
    SparkbotEmojiDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, int width,
                     int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y,
                     bool swap_xy, DisplayFonts fonts);

    virtual ~SparkbotEmojiDisplay();

    // 重写表情设置方法
    virtual void SetEmotion(const char* emotion) override;

    // 重写聊天消息设置方法
    virtual void SetChatMessage(const char* role, const char* content) override;

    // 添加SetIcon方法声明
    virtual void SetIcon(const char* icon) override;
    
    // 重写图片预览方法
    virtual void SetPreviewImage(const lv_img_dsc_t* img_dsc) override;

private:
    void SetupGifContainer();
    void HidePreviewImage();  ///< 隐藏预览图片的方法

    lv_obj_t* emotion_gif_;  ///< GIF表情组件
    lv_obj_t* preview_image_obj_;  ///< 图片预览组件
    esp_timer_handle_t preview_timer_;  ///< 预览图片定时器

    // 表情映射
    struct EmotionMap {
        const char* name;
        const lv_img_dsc_t* gif;
    };

    static const EmotionMap emotion_maps_[];
};