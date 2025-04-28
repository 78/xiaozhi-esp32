#ifndef _LCD_TOUCH_H

#include <lvgl.h>
#include "lcd_display.h"


// SPI LCD 触摸屏
class SpiLcdDisplayEx : public SpiLcdDisplay {
public:
    SpiLcdDisplayEx(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts) :
                  SpiLcdDisplay(panel_io, panel,
                  width, height, offset_x, offset_y,
                  mirror_x, mirror_y, swap_xy,
                  fonts) {
    }

    void InitializeTouch(i2c_master_bus_handle_t i2c_bus);
};

#endif // _LCD_TOUCH_H
