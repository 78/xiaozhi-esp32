#ifndef ATK_ST7789_80I_H
#define ATK_ST7789_80I_H

#include "display.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>

class ATK_ST7789_80_Display : public Display {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    gpio_num_t backlight_pin_ = GPIO_NUM_4;
    bool backlight_output_invert_ = false;
    bool mirror_x_ = false;
    bool mirror_y_ = false;
    bool swap_xy_ = false;
    int offset_x_ = 0;
    int offset_y_ = 0;
    SemaphoreHandle_t lvgl_mutex_ = nullptr;
    esp_timer_handle_t lvgl_tick_timer_ = nullptr;
    
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;

    void InitializeBacklight(gpio_num_t backlight_pin);
    // void SetBacklight(uint8_t brightness);
    void LvglTask();

    virtual void SetupUI();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

public:
    ATK_ST7789_80_Display(gpio_num_t backlight_pin,int width, int height,  int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy);
    ~ATK_ST7789_80_Display();
    // void SetBacklight(uint8_t brightness);
    void SetChatMessage(const std::string &role, const std::string &content) override;
};

#endif // LCD_DISPLAY_H
