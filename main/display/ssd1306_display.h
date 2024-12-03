#ifndef SSD1306_DISPLAY_H
#define SSD1306_DISPLAY_H

#include "display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

class Ssd1306Display : public Display {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    bool mirror_x_ = false;
    bool mirror_y_ = false;

    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetupUI_128x64();
    void SetupUI_128x32();

public:
    Ssd1306Display(void* i2c_master_handle, int width, int height, bool mirror_x = false, bool mirror_y = false);
    ~Ssd1306Display();
};

#endif // SSD1306_DISPLAY_H
