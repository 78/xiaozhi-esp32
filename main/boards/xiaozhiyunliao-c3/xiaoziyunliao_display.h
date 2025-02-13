#ifndef XIAOZIYUNLIAO_DISPLAY_H
#define XIAOZIYUNLIAO_DISPLAY_H

#include "display/lcd_display.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>
#include <font_emoji.h>

#include <atomic>

class XiaoziyunliaoDisplay : public LcdDisplay {
public:
    XiaoziyunliaoDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  gpio_num_t backlight_pin, bool backlight_output_invert,
                  int width, int height,  int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
    void SetupUI() override;  // 添加方法重载声明
    ~XiaoziyunliaoDisplay() override = default; // 显式声明析构函数

    // 添加虚函数覆盖声明
    void SetChatMessage(const std::string &role, const std::string &content) override;
    void SetEmotion(const std::string &emotion) override;
    void SetIcon(const char* icon) override;

protected:

};

#endif // XIAOZIYUNLIAO_DISPLAY_H 