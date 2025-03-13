#ifndef SSD1306_DISPLAY_H
#define SSD1306_DISPLAY_H

#include "display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

class Ssd1306Display : public Display {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* content_left_ = nullptr;
    lv_obj_t* content_right_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;

    const lv_font_t* text_font_ = nullptr;
    const lv_font_t* icon_font_ = nullptr;

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetupUI_128x64();
    void SetupUI_128x32();

public:
    Ssd1306Display(void* i2c_master_handle, int width, int height, bool mirror_x, bool mirror_y,
                   const lv_font_t* text_font, const lv_font_t* icon_font);
    ~Ssd1306Display();

    virtual void SetChatMessage(const char* role, const char* content) override;
};

#endif // SSD1306_DISPLAY_H
