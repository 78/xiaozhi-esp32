#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "display.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>
#include <font_emoji.h>

#include <atomic>

class LcdDisplay : public Display {
public:
    enum class PageIndex {
        PAGE_CHAT = 0,
        PAGE_CONFIG = 1
    };
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    gpio_num_t backlight_pin_ = GPIO_NUM_NC;
    bool backlight_output_invert_ = false;
    
    lv_draw_buf_t draw_buf_;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    lv_obj_t* emotion_label_ = nullptr;
    lv_obj_t* config_container_ = nullptr;
    lv_obj_t* config_text_panel_ = nullptr;
    lv_obj_t* config_qrcode_panel_ = nullptr;
    lv_obj_t* qrcode_label_ = nullptr;
    lv_obj_t* smartconfig_qrcode_ = nullptr;

    DisplayFonts fonts_;

    PageIndex lv_page_index = PageIndex::PAGE_CHAT;
    int backlight_brightness_ = 100;
    void InitializeBacklight(gpio_num_t backlight_pin);

    virtual void SetupUI();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

public:
    LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  gpio_num_t backlight_pin, bool backlight_output_invert,
                  int width, int height,  int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
    ~LcdDisplay();

    void SetChatMessage(const std::string &role, const std::string &content) override;
    void SetEmotion(const std::string &emotion) override;
    void SetIcon(const char* icon) override;
    void SetBacklight(int brightness);
    inline int backlight() const { return backlight_brightness_; }
    void SetConfigPage(const std::string& config_text, 
                      const std::string& qrcode_label_text,
                      const std::string& qrcode_content);
    void lv_chat_page();
    void lv_config_page();
    void lv_switch_page();
    void lv_smartconfig_page(const std::string& qrcode_content);
    PageIndex getlvpage() const { return lv_page_index; }
    std::string DisplayType() const override { return "LCD"; }
};

#endif // LCD_DISPLAY_H
