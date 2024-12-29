#ifndef ILI9341_DISPLAY_H
#define ILI9341_DISPLAY_H

#include "display.h"
#include "esp_lcd_ili9341.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>

class Ili9341Display : public Display {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    gpio_num_t backlight_pin_ = GPIO_NUM_NC;
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

    lv_obj_t *user_messge_label_ = nullptr;
    lv_obj_t *ai_messge_label_ = nullptr;
    
    void InitializeBacklight(gpio_num_t backlight_pin);
    void SetBacklight(uint8_t brightness);
    void SetupUI();
    void LvglTask();

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;
    virtual void SetChatMessage(const std::string &role, const std::string &content) override;

public:
    Ili9341Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  gpio_num_t backlight_pin, bool backlight_output_invert,
                  int width, int height,  int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy);
    ~Ili9341Display();
};

#endif // Ili9341_DISPLAY_H
