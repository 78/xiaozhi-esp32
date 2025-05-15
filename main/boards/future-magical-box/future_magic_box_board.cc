#include "wifi_board.h"
#include "audio_codecs/box_audio_codec.h"
#include "future_magic_box_audio_codec.h"
#include "esp_lcd_panel_rgb.h"
#include "display/lcd_display.h"
#include <esp_lvgl_port_touch.h>
#include "esp_lcd_panel_io_additions.h"
#include "esp_io_expander_tca9554.h"
#include "esp_lcd_touch_ft5x06.h"
#include "esp_lcd_st7701.h"
#include "font_awesome_symbols.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>

#define TAG "MagicBox"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

#define MY_ST7701_480_480_PANEL_60HZ_RGB_TIMING()   \
{                                                   \
    .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,          \
    .h_res = DISPLAY_WIDTH,                         \
    .v_res = DISPLAY_HEIGHT,                        \
    .hsync_pulse_width = EXAMPLE_LCD_HSYNC,         \
    .hsync_back_porch = EXAMPLE_LCD_HBP,            \
    .hsync_front_porch = EXAMPLE_LCD_HFP,           \
    .vsync_pulse_width = EXAMPLE_LCD_VSYNC,         \
    .vsync_back_porch = EXAMPLE_LCD_VBP,            \
    .vsync_front_porch = EXAMPLE_LCD_VFP,           \
    .flags = {                                      \
        .hsync_idle_low = 0,                        \
        .vsync_idle_low = 0,                        \
        .de_idle_high = 0,                          \
        .pclk_active_neg = 0,                       \
        .pclk_idle_high = 0,                        \
    },                                              \
}


/**
 * Uncomment these line if use custom initialization commands.
 * The array should be declared as static const and positioned outside the function.
 */
static const st7701_lcd_init_cmd_t lcd_init_cmds[] = {
    //  {cmd, { data }, data_size, delay_ms}
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x3B, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0E, 0x0C}, 2, 0},
    {0xC2, (uint8_t[]){0x07, 0x0A}, 2, 0},
    {0xCC, (uint8_t[]){0x30}, 1, 0},
    {0xB0, (uint8_t[]){0x40, 0x07, 0x53, 0x0E, 0x12, 0x07, 0x0A, 0x09, 0x09, 0x26, 0x05, 0x10, 0x0D, 0x6E, 0x3B, 0xD6}, 16, 0},
    {0xB1, (uint8_t[]){0x40, 0x17, 0x5C, 0x0D, 0x11, 0x06, 0x08, 0x08, 0x08, 0x22, 0x03, 0x12, 0x11, 0x65, 0x28, 0xE8}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t[]){0x4D}, 1, 0},
    {0xB1, (uint8_t[]){0x4C}, 1, 0},
    {0xB2, (uint8_t[]){0x81}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4C}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x33}, 1, 0},
    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},
    {0xE0, (uint8_t[]){0x00, 0x00, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x05, 0x30, 0x00, 0x00, 0x06, 0x30, 0x00, 0x00, 0x0E, 0x30, 0x30}, 11, 0},
    {0xE2, (uint8_t[]){0x10, 0x10, 0x30, 0x30, 0xF4, 0x00, 0x00, 0x00, 0xF4, 0x00, 0x00, 0x00}, 12, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x0A, 0xF4, 0x30, 0xF0, 0x0C, 0xF6, 0x30, 0xF0, 0x06, 0xF0, 0x30, 0xF0, 0x08, 0xF2, 0x30, 0xF0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x0B, 0xF5, 0x30, 0xF0, 0x0D, 0xF7, 0x30, 0xF0, 0x07, 0xF1, 0x30, 0xF0, 0x09, 0xF3, 0x30, 0xF0}, 16, 0},
    {0xE9, (uint8_t[]){0x36, 0x01}, 2, 0},
    {0xEB, (uint8_t[]){0x00, 0x01, 0xE4, 0xE4, 0x44, 0x88, 0x33}, 7, 0},
    {0xED, (uint8_t[]){0x20, 0xFA, 0xB7, 0x76, 0x65, 0x54, 0x4F, 0xFF, 0xFF, 0xF4, 0x45, 0x56, 0x67, 0x7B, 0xAF, 0x02}, 16, 0},
    {0xEF, (uint8_t[]){0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0x3A, (uint8_t[]){0x66}, 1, 0},
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0x29, (uint8_t[]){0x00}, 0, 0},
};

class MagicBox : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    esp_io_expander_handle_t io_expander_;
    Button boot_button_;
    LcdDisplay* display_;

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeST7701Display() {

        ESP_LOGI(TAG, "Install 3-wire SPI panel IO");

        esp_io_expander_new_i2c_tca9554(i2c_bus_, BSP_IO_EXPANDER_I2C_ADDRESS, &io_expander_);
        spi_line_config_t line_config = {
            .cs_io_type = IO_TYPE_EXPANDER,             // Set to `IO_TYPE_GPIO` if using GPIO, same to below
            .cs_gpio_num = EXAMPLE_LCD_IO_SPI_CS,
            .scl_io_type = IO_TYPE_EXPANDER,
            .scl_gpio_num = EXAMPLE_LCD_IO_SPI_SCK,
            .sda_io_type = IO_TYPE_EXPANDER,
            .sda_gpio_num = EXAMPLE_LCD_IO_SPI_SDO,
            .io_expander = io_expander_,                        // Set to NULL if not using IO expander
        };

        esp_lcd_panel_io_3wire_spi_config_t io_config = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
        esp_lcd_panel_io_handle_t panel_io = NULL;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &panel_io));
#if 1
        ESP_LOGI(TAG, "Install ST7701 panel driver");
        esp_lcd_panel_handle_t panel_handle = NULL;
        esp_lcd_rgb_panel_config_t rgb_config = {
            .clk_src = LCD_CLK_SRC_DEFAULT,
            // .psram_trans_align = 64,
            .timings = MY_ST7701_480_480_PANEL_60HZ_RGB_TIMING(),
            .data_width = EXAMPLE_RGB_DATA_WIDTH, // RGB565 in parallel mode, thus 16bit in width
            .bits_per_pixel = EXAMPLE_RGB_BIT_PER_PIXEL,
            .num_fbs = EXAMPLE_LCD_NUM_FB,
            .bounce_buffer_size_px = EXAMPLE_RGB_BOUNCE_BUFFER_SIZE,
            .dma_burst_size = EXAMPLE_LCD_DMA_SZIE,
            .hsync_gpio_num = EXAMPLE_LCD_IO_RGB_HSYNC,
            .vsync_gpio_num = EXAMPLE_LCD_IO_RGB_VSYNC,
            .de_gpio_num = EXAMPLE_LCD_IO_RGB_DE,
            .pclk_gpio_num = EXAMPLE_LCD_IO_RGB_PCLK,
            .disp_gpio_num = EXAMPLE_LCD_IO_RGB_DISP,
            .data_gpio_nums = {
                EXAMPLE_LCD_IO_RGB_DATA0,
                EXAMPLE_LCD_IO_RGB_DATA1,
                EXAMPLE_LCD_IO_RGB_DATA2,
                EXAMPLE_LCD_IO_RGB_DATA3,
                EXAMPLE_LCD_IO_RGB_DATA4,
                EXAMPLE_LCD_IO_RGB_DATA5,
                EXAMPLE_LCD_IO_RGB_DATA6,
                EXAMPLE_LCD_IO_RGB_DATA7,
                EXAMPLE_LCD_IO_RGB_DATA8,
                EXAMPLE_LCD_IO_RGB_DATA9,
                EXAMPLE_LCD_IO_RGB_DATA10,
                EXAMPLE_LCD_IO_RGB_DATA11,
                EXAMPLE_LCD_IO_RGB_DATA12,
                EXAMPLE_LCD_IO_RGB_DATA13,
                EXAMPLE_LCD_IO_RGB_DATA14,
                EXAMPLE_LCD_IO_RGB_DATA15,
            },
            .flags= {
                .fb_in_psram = true, // allocate frame buffer in PSRAM
            }
        };
    
        ESP_LOGI(TAG, "Initialize RGB LCD panel");
    
        st7701_vendor_config_t vendor_config = {
            .init_cmds = lcd_init_cmds,      // Uncomment these line if use custom initialization commands
            .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(st7701_lcd_init_cmd_t),
            .rgb_config = &rgb_config,
            .flags = {
            .mirror_by_cmd = 0,             // Only work when `enable_io_multiplex` is set to 0
            .enable_io_multiplex = 1,         /**
                                             * Set to 1 if panel IO is no longer needed after LCD initialization.
                                             * If the panel IO pins are sharing other pins of the RGB interface to save GPIOs,
                                             * Please set it to 1 to release the pins.
                                             */
            },
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = -1,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = EXAMPLE_LCD_BIT_PER_PIXEL,
            .vendor_config = &vendor_config,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(panel_io, &panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

#if 0
        esp_lcd_rgb_panel_event_callbacks_t cbs = {
#if EXAMPLE_RGB_BOUNCE_BUFFER_SIZE > 0
        .on_bounce_frame_finish = rgb_lcd_on_vsync_event,
#else
        .on_vsync = rgb_lcd_on_vsync_event,
#endif
    };
    esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL);

#endif

#endif

        display_ = new RgbLcdDisplay(panel_io, panel_handle,
        DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
        {
            .text_font = &font_puhui_20_4,
            .icon_font = &font_awesome_20_4,
            #if CONFIG_USE_WECHAT_MESSAGE_STYLE
                .emoji_font = DISPLAY_HEIGHT >= 240 ? font_emoji_64_init() : font_emoji_32_init(),
            #else
                .emoji_font = DISPLAY_HEIGHT >= 240 ? font_emoji_64_init() : font_emoji_32_init(),
            #endif
        });
    }

    void InitializeTouch()
    {
        ESP_LOGI(TAG, "Initialize panel touch");
        esp_lcd_touch_handle_t tp;
        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = GPIO_NUM_NC, // Shared with LCD reset
            .int_gpio_num = GPIO_NUM_NC, 
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = 1,
                .mirror_x = 1,
                .mirror_y = 0,
            },
        };
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
        tp_io_config.scl_speed_hz = 400000;

        esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &tp_io_handle);
        esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp);
        assert(tp);

        /* Add touch input (for selected screen) */
        const lvgl_port_touch_cfg_t touch_cfg = {
            .disp = lv_display_get_default(), 
            .handle = tp,
        };

        lvgl_port_add_touch(&touch_cfg);
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
    }

public:
    MagicBox() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeST7701Display();
        InitializeTouch();
        InitializeIot();
        GetBacklight()->RestoreBrightness(); 
    }

    virtual AudioCodec* GetAudioCodec() override {
        static MagicBoxAudioCodec audio_codec(
            i2c_bus_,
            io_expander_,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, 
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(MagicBox);
