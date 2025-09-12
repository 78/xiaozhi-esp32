#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "pin_config.h"

#include "config.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include "esp_lcd_gc9503.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_io_additions.h>

#define TAG "Yuying_313lcd"

class Yuying_313lcd : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    LcdDisplay* display_;

    void InitializeRGB_GC9503V_Display() {
        ESP_LOGI(TAG, "Init GC9503V");

        esp_lcd_panel_io_handle_t panel_io = nullptr;

        ESP_LOGI(TAG, "Install 3-wire SPI panel IO");
        spi_line_config_t line_config = {
            .cs_io_type = IO_TYPE_GPIO,
            .cs_gpio_num = GC9503V_LCD_IO_SPI_CS_1,
            .scl_io_type = IO_TYPE_GPIO,
            .scl_gpio_num = GC9503V_LCD_IO_SPI_SCL_1,
            .sda_io_type = IO_TYPE_GPIO,
            .sda_gpio_num = GC9503V_LCD_IO_SPI_SDO_1,
            .io_expander = NULL,
        };
        esp_lcd_panel_io_3wire_spi_config_t io_config = GC9503_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
        (esp_lcd_new_panel_io_3wire_spi(&io_config, &panel_io));
    
        ESP_LOGI(TAG, "Install RGB LCD panel driver");
        esp_lcd_panel_handle_t panel_handle = NULL;
        esp_lcd_rgb_panel_config_t rgb_config = {
            .clk_src = LCD_CLK_SRC_PLL160M,
            .timings = GC9503_376_960_PANEL_60HZ_RGB_TIMING(),
            .data_width = 16, // RGB565 in parallel mode, thus 16bit in width
            .bits_per_pixel = 16,
            .num_fbs = GC9503V_LCD_RGB_BUFFER_NUMS,
            .bounce_buffer_size_px = GC9503V_LCD_H_RES * GC9503V_LCD_RGB_BOUNCE_BUFFER_HEIGHT,
            .dma_burst_size = 64,
            .hsync_gpio_num = GC9503V_PIN_NUM_HSYNC,
            .vsync_gpio_num = GC9503V_PIN_NUM_VSYNC,
            .de_gpio_num = GC9503V_PIN_NUM_DE,
            .pclk_gpio_num = GC9503V_PIN_NUM_PCLK,
            .disp_gpio_num = GC9503V_PIN_NUM_DISP_EN,
            .data_gpio_nums = {
                GC9503V_PIN_NUM_DATA0,
                GC9503V_PIN_NUM_DATA1,
                GC9503V_PIN_NUM_DATA2,
                GC9503V_PIN_NUM_DATA3,
                GC9503V_PIN_NUM_DATA4,
                GC9503V_PIN_NUM_DATA5,
                GC9503V_PIN_NUM_DATA6,
                GC9503V_PIN_NUM_DATA7,
                GC9503V_PIN_NUM_DATA8,
                GC9503V_PIN_NUM_DATA9,
                GC9503V_PIN_NUM_DATA10,
                GC9503V_PIN_NUM_DATA11,
                GC9503V_PIN_NUM_DATA12,
                GC9503V_PIN_NUM_DATA13,
                GC9503V_PIN_NUM_DATA14,
                GC9503V_PIN_NUM_DATA15,
            },
            .flags= {
                .fb_in_psram = true, // allocate frame buffer in PSRAM
            }
        };
    
        ESP_LOGI(TAG, "Initialize RGB LCD panel");
    
        gc9503_vendor_config_t vendor_config = {
            .rgb_config = &rgb_config,
            .flags = {
                .mirror_by_cmd = 0,
                .auto_del_panel_io = 1,
            },
        };
        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = -1,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = 16,
            .vendor_config = &vendor_config,
        };
        (esp_lcd_new_panel_gc9503(panel_io, &panel_config, &panel_handle));
        (esp_lcd_panel_reset(panel_handle));
        (esp_lcd_panel_init(panel_handle));

        display_ = new RgbLcdDisplay(panel_io, panel_handle,
                                  DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                  DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
        });
        boot_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });
    }

public:
    Yuying_313lcd() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeButtons();
        InitializeRGB_GC9503V_Display();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
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

DECLARE_BOARD(Yuying_313lcd);
