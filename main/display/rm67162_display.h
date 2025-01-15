#ifndef RM67162_DISPLAY_H
#define RM67162_DISPLAY_H

#include "display.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include "settings.h"
#include "esp_lcd_sh8601.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "esp_log.h"

class Rm67162Display : public Display
{
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    // gpio_num_t backlight_pin_ = GPIO_NUM_NC;
    bool backlight_output_invert_ = false;
    bool mirror_x_ = false;
    bool mirror_y_ = false;
    bool swap_xy_ = false;
    int offset_x_ = 0;
    int offset_y_ = 0;
    uint8_t brightness_ = 0;
    SemaphoreHandle_t lvgl_mutex_ = nullptr;
    esp_timer_handle_t lvgl_tick_timer_ = nullptr;

    lv_obj_t *status_bar_ = nullptr;
    lv_obj_t *container_ = nullptr;
    lv_obj_t *side_bar_ = nullptr;

    // void InitializeBacklight(gpio_num_t backlight_pin);
    void SetupUI();
    void LvglTask();

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void InitBrightness();

public:
    Rm67162Display(esp_lcd_spi_bus_handle_t spi_bus, int cs, int rst,
                   int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy);
    ~Rm67162Display();

    virtual int GetBacklight() override
    {
        return brightness_;
    }

    virtual void SetBacklight(uint8_t brightness) override
    {
        brightness_ = brightness;
        if (brightness > 100)
        {
            brightness = 100;
        }
        Settings settings("display", true);
        settings.SetInt("bright", brightness_);

        ESP_LOGI("Rm67162Display", "Setting LCD backlight: %d%%", brightness);
        // LEDC resolution set to 10bits, thus: 100% = 255
        uint8_t data[1] = {((uint8_t)((255 * brightness) / 100))};
        int lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
        esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
    }
};

#endif // RM67162_DISPLAY_H
