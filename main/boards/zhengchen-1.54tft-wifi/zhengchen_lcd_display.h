#ifndef ZHENGCHEN_LCD_DISPLAY_H
#define ZHENGCHEN_LCD_DISPLAY_H

#include "display/display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <font_emoji.h>

#include <atomic>

class ZHENGCHEN_LcdDisplay : public Display {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    
    lv_draw_buf_t draw_buf_;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;

    lv_obj_t* high_temp_popup_ = nullptr;
    lv_obj_t* high_temp_label_ = nullptr;

    DisplayFonts fonts_;

    void SetupUI();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

protected:
    // 添加protected构造函数
    ZHENGCHEN_LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, DisplayFonts fonts)
        : panel_io_(panel_io), panel_(panel), fonts_(fonts) {}
    
public:
    ~ZHENGCHEN_LcdDisplay();
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetIcon(const char* icon) override;
     // 获取高温提示框指针
    lv_obj_t* GetHighTempPopup() const { 
        return high_temp_popup_; 
    }
    /*
    // 控制高温提示框显示/隐藏
    void ShowHighTempPopup(bool show) {
        if (!high_temp_popup_) {
            ESP_LOGE("TAG", "弹窗对象已被删除！");
            return;
        }
        if (high_temp_popup_) {
            if (show) {
                lv_obj_clear_flag(high_temp_popup_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(high_temp_popup_, LV_OBJ_FLAG_HIDDEN);
            }
        }
    } */
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    virtual void SetChatMessage(const char* role, const char* content) override; 
#endif  

    // Add theme switching function
    virtual void SetTheme(const std::string& theme_name) override;
};

// RGB LCD显示器
class RgbZHENGCHEN_LcdDisplay : public ZHENGCHEN_LcdDisplay {
public:
    RgbZHENGCHEN_LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// MIPI LCD显示器
class MipiZHENGCHEN_LcdDisplay : public ZHENGCHEN_LcdDisplay {
public:
    MipiZHENGCHEN_LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// // SPI LCD显示器
class SpiZHENGCHEN_LcdDisplay : public ZHENGCHEN_LcdDisplay {
public:
    SpiZHENGCHEN_LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// QSPI LCD显示器
class QspiZHENGCHEN_LcdDisplay : public ZHENGCHEN_LcdDisplay {
public:
    QspiZHENGCHEN_LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// MCU8080 LCD显示器
class Mcu8080ZHENGCHEN_LcdDisplay : public ZHENGCHEN_LcdDisplay {
public:
    Mcu8080ZHENGCHEN_LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                      int width, int height, int offset_x, int offset_y,
                      bool mirror_x, bool mirror_y, bool swap_xy,
                      DisplayFonts fonts);
};
#endif // ZHENGCHEN_LCD_DISPLAY_H
