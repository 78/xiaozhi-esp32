#include "wifi_board.h"
#include "display/lcd_display.h"
#include "esp_lcd_st7701.h"
#include "audio/codecs/no_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <driver/ledc.h>
#include "esp_io_expander_tca9554.h"
#include "settings.h"

#include <esp_lcd_touch_gt911.h>
#include <esp_lvgl_port.h>
#include <lvgl.h>

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_io_additions.h>
#include <esp_ota_ops.h>

#define TAG "CocodeRenEsp32s3TouchAqara"

static const st7701_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x3B, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x31, 0x05}, 2, 0},
    {0xCD, (uint8_t[]){0x08}, 1, 0},
    {0xB0, (uint8_t[]){0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08, 0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18}, 16, 0},
    {0xB1, (uint8_t[]){0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08, 0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x60}, 1, 0},
    {0xB1, (uint8_t[]){0x32}, 1, 0},
    {0xB2, (uint8_t[]){0x07}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x49}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x1B, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x44, 0x44}, 11, 0},
    {0xE2, (uint8_t[]){0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00, 0xEC, 0xA0, 0x00, 0x00}, 12, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0, 0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0, 0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0}, 16, 0},
    {0xEB, (uint8_t[]){0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40}, 7, 0},
    {0xEC, (uint8_t[]){0x3C, 0x00}, 2, 0},
    {0xED, (uint8_t[]){0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xE5, (uint8_t[]){0xE4}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},
    {0x20, (uint8_t[]){0x00}, 0, 0},
    {0x3A, (uint8_t[]){0x50}, 1, 0},
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x29, (uint8_t[]){0x00}, 0, 0},
};

class AqaraBacklight : public Backlight {
public:
    AqaraBacklight(gpio_num_t pin, bool output_invert = false) : Backlight(), pin_(pin), output_invert_(output_invert) {
        const ledc_timer_config_t backlight_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_10_BIT,
            .timer_num = LEDC_TIMER_0,
            .freq_hz = 2000,
            .clk_cfg = LEDC_AUTO_CLK,
            .deconfigure = false
        };
        ESP_ERROR_CHECK(ledc_timer_config(&backlight_timer));

        const ledc_channel_config_t backlight_channel = {
            .gpio_num = pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
            .flags = {
                .output_invert = output_invert,
            }
        };
        ESP_ERROR_CHECK(ledc_channel_config(&backlight_channel));
    }

    void SetBrightnessImpl(uint8_t brightness) override {
        uint32_t duty_cycle = 0;
        if (brightness > 0) {
            duty_cycle = 240 + (uint32_t)(brightness * (1024 - 240) / 100);
        }
        if (duty_cycle > 1024) duty_cycle = 1024;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty_cycle);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    }

private:
    gpio_num_t pin_;
    bool output_invert_;
};

class CocodeRenEsp32s3TouchAqara : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    LcdDisplay* display_;
    esp_io_expander_handle_t io_expander = NULL;

    void InitializeI2c() {
        // Force reset new mic pins 1, 2, 42
        gpio_reset_pin(GPIO_NUM_1);
        gpio_reset_pin(GPIO_NUM_2);
        gpio_reset_pin(GPIO_NUM_42);

        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = TOUCH_GT911_SDA,
            .scl_io_num = TOUCH_GT911_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeTca9554() {
        ESP_LOGI(TAG, "Initialize TCA9554 expander at 0x%02X", I2C_ADDRESS_EXPANDER);
        ESP_ERROR_CHECK(esp_io_expander_new_i2c_tca9554(i2c_bus_, I2C_ADDRESS_EXPANDER, &io_expander));
    }

    void InitializeRGB() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;

        spi_line_config_t line_config = {
            .cs_io_type = IO_TYPE_EXPANDER,
            .cs_expander_pin = LCD_ST7701_CS,
            .scl_io_type = IO_TYPE_EXPANDER,
            .scl_expander_pin = LCD_ST7701_SCL,
            .sda_io_type = IO_TYPE_EXPANDER,
            .sda_expander_pin = LCD_ST7701_SDA,
            .io_expander = io_expander,
        };
        esp_lcd_panel_io_3wire_spi_config_t io_config = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &panel_io));
        esp_lcd_panel_handle_t panel_handle = NULL;
        esp_lcd_rgb_panel_config_t rgb_config = {
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .timings = {
                .pclk_hz = 8 * 1000 * 1000,
                .h_res = DISPLAY_WIDTH,
                .v_res = DISPLAY_HEIGHT,
                .hsync_pulse_width = 8,
                .hsync_back_porch = 60,
                .hsync_front_porch = 10,
                .vsync_pulse_width = 8,
                .vsync_back_porch = 20,
                .vsync_front_porch = 10,
                .flags = {
                    .hsync_idle_low = 0,
                    .vsync_idle_low = 0,
                    .de_idle_high = 0,
                    .pclk_active_neg = 0,
                    .pclk_idle_high = 0,
                }
            },
            .data_width = 16,
            .bits_per_pixel = 16,
            .num_fbs = 2,
            .bounce_buffer_size_px = 480 * 20,
            .psram_trans_align = 64,
            .hsync_gpio_num = BSP_LCD_HSYNC,
            .vsync_gpio_num = BSP_LCD_VSYNC,
            .de_gpio_num = BSP_LCD_DE,
            .pclk_gpio_num = BSP_LCD_PCLK,
            .disp_gpio_num = BSP_LCD_DISP,
            .data_gpio_nums = {
                BSP_LCD_DATA0, BSP_LCD_DATA1, BSP_LCD_DATA2, BSP_LCD_DATA3,
                BSP_LCD_DATA4, BSP_LCD_DATA5, BSP_LCD_DATA6, BSP_LCD_DATA7,
                BSP_LCD_DATA8, BSP_LCD_DATA9, BSP_LCD_DATA10, BSP_LCD_DATA11,
                BSP_LCD_DATA12, BSP_LCD_DATA13, BSP_LCD_DATA14, BSP_LCD_DATA15
            },
            .flags = {
                .fb_in_psram = 1,
            },
        };

        st7701_vendor_config_t vendor_config = {
            .init_cmds = lcd_init_cmds,
            .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
            .rgb_config = &rgb_config,
            .flags = {
                .mirror_by_cmd = 0,
                .auto_del_panel_io = 1,
            }
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = GPIO_NUM_NC,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = 16, // Match the 16-line RGB interface
            .vendor_config = &vendor_config,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(panel_io, &panel_config, &panel_handle));
        esp_lcd_panel_init(panel_handle);

        display_ = new RgbLcdDisplay(panel_io, panel_handle,
                                  DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                  DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeTouch() {
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH - 1,
            .y_max = DISPLAY_HEIGHT - 1,
            .rst_gpio_num = TOUCH_GT911_RST,
            .int_gpio_num = TOUCH_GT911_INT,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = {
            .dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS, 
            .control_phase_bytes = 1,
            .dc_bit_offset = 0,
            .lcd_cmd_bits = 16,                            
            .flags =
            {
                .disable_control_phase = 1,
            }
	    };
        tp_io_config.scl_speed_hz = 400 * 1000;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle));
        ESP_LOGI(TAG, "Initialize touch controller");
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp));
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(),
            .handle = tp,
        };
        lvgl_port_add_touch(&touch_cfg);
        ESP_LOGI(TAG, "Touch panel initialized successfully");
    }

public:
    CocodeRenEsp32s3TouchAqara() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeTca9554();
        InitializeRGB();
        InitializeTouch();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, I2S_STD_SLOT_LEFT,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DIN, I2S_STD_SLOT_LEFT);
            
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static AqaraBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging) override {
        level = 100;
        charging = false;
        discharging = true;
        return true;
    }
};

DECLARE_BOARD(CocodeRenEsp32s3TouchAqara);
