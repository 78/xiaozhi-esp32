#ifndef DISPLAY_H
#define DISPLAY_H

#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <lvgl.h>
#include <esp_timer.h>

#include <string>

class Display {
public:
    Display(int sda_pin, int scl_pin);
    ~Display();

    void SetText(const std::string &text);
    void ShowNotification(const std::string &text);

private:
    int sda_pin_;
    int scl_pin_;

    i2c_master_bus_handle_t i2c_bus_ = nullptr;

    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    lv_disp_t *disp_ = nullptr;
    lv_obj_t *label_ = nullptr;
    lv_obj_t *notification_ = nullptr;
    esp_timer_handle_t notification_timer_ = nullptr;

    std::string text_;
};

#endif
