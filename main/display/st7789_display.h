#ifndef ST7789_DISPLAY_H
#define ST7789_DISPLAY_H

#include "display.h"

#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

class St7789Display : public Display {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    bool mirror_x_ = false;
    bool mirror_y_ = false;
    bool bl_output_invert_ = false;

    void InitializeBacklight(gpio_num_t backlight_pin);
    void SetBacklight(uint8_t brightness);

    virtual void Lock() override;
    virtual void Unlock() override;

public:
    St7789Display(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, gpio_num_t backlight_pin,
                  int width, int height, bool mirror_x, bool mirror_y, bool bl_output_invert);
    ~St7789Display();
};

#endif // ST7789_DISPLAY_H
