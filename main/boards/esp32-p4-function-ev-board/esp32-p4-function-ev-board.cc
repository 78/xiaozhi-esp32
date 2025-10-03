#include "wifi_board.h"
#include "audio/codecs/es8311_audio_codec.h"
// Display
#include "display/display.h"
#include "display/lcd_display.h"
// Backlight
// PwmBacklight is declared in backlight headers pulled by display/lcd_display includes via lvgl stack

#include "application.h"
#include "button.h"
#include "config.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <inttypes.h>
#include <driver/i2c_master.h>
#include <esp_lvgl_port.h>
// SD card
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>
#include <driver/sdspi_host.h>
// SD power control (on-chip LDO)
#include "sd_pwr_ctrl_by_on_chip_ldo.h"

// MIPI-DSI / LCD vendor includes
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_touch_gt911.h"

#define TAG "ESP32P4FuncEV"

class ESP32P4FunctionEvBoard : public WifiBoard
{
private:
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    i2c_master_bus_handle_t touch_i2c_bus_ = nullptr;
    Button boot_button_;
    LcdDisplay *display_ = nullptr;
    esp_lcd_touch_handle_t tp_ = nullptr;

    static esp_err_t EnableDsiPhyPower()
    {
#if MIPI_DSI_PHY_PWR_LDO_CHAN > 0
        static esp_ldo_channel_handle_t phy_pwr_chan = NULL;
        esp_ldo_channel_config_t ldo_cfg = {
            .chan_id = MIPI_DSI_PHY_PWR_LDO_CHAN,
            .voltage_mv = MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
        };
        esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan);
        ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
#endif
        return ESP_OK;
    }

    void InitializeI2cBuses()
    {
        // Codec I2C bus
        i2c_master_bus_config_t codec_cfg = {
            .i2c_port = I2C_NUM_1,
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&codec_cfg, &codec_i2c_bus_));
    }

    void InitializeTouchI2cBus()
    {
        // Touch I2C bus
        // i2c_master_bus_config_t touch_cfg = {
        //     .i2c_port = I2C_NUM_0,
        //     .sda_io_num = TOUCH_I2C_SDA_PIN,
        //     .scl_io_num = TOUCH_I2C_SCL_PIN,
        //     .clk_source = I2C_CLK_SRC_DEFAULT,
        //     .glitch_ignore_cnt = 7,
        //     .intr_priority = 0,
        //     .trans_queue_depth = 0,
        //     .flags = {
        //         .enable_internal_pullup = 1,
        //     },
        // };
        // ESP_ERROR_CHECK(i2c_new_master_bus(&touch_cfg, &touch_i2c_bus_));
    }

    void InitializeLCD()
    {
#ifdef LVGL_VERSION_MAJOR
        EnableDsiPhyPower();

        esp_lcd_panel_io_handle_t io = NULL;
        esp_lcd_panel_handle_t panel = NULL;

        esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
        esp_lcd_dsi_bus_config_t bus_config = {
            .bus_id = 0,
            .num_data_lanes = 2,
            .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
            .lane_bit_rate_mbps = 1000,
        };
        ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus));

        ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
        esp_lcd_dbi_io_config_t dbi_config = {
            .virtual_channel = 0,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_config, &io));

        // 1024x600 DPI timing; tune per actual panel if needed
        esp_lcd_dpi_panel_config_t dpi_config = {
            .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
            .dpi_clock_freq_mhz = 52,
            .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
            .num_fbs = 1,
            .video_timing = {
                .h_size = DISPLAY_WIDTH,
                .v_size = DISPLAY_HEIGHT,
                .hsync_pulse_width = 10,
                .hsync_back_porch = 160,
                .hsync_front_porch = 160,
                .vsync_pulse_width = 1,
                .vsync_back_porch = 23,
                .vsync_front_porch = 12,
            },
            .flags = {
                .use_dma2d = true,
            },
        };

        ek79007_vendor_config_t vendor_config = {
            .mipi_config = {
                .dsi_bus = dsi_bus,
                .dpi_config = &dpi_config,
            },
        };

        const esp_lcd_panel_dev_config_t lcd_dev_config = {
            .reset_gpio_num = PIN_NUM_LCD_RST,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = 16,
            .vendor_config = &vendor_config,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(io, &lcd_dev_config, &panel));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

        display_ = new MipiLcdDisplay(io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                      DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                      DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, false);
#endif // LVGL_VERSION_MAJOR
    }

    void InitializeButtons()
    {
        boot_button_.OnClick([this]()
                             {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState(); });
    }

    void InitializeTouch()
    {
        // ESP_LOGI(TAG, "Init GT911");

        // /* Initialize Touch Panel */
        // ESP_LOGI(TAG, "Initialize touch IO (I2C)");

        // const esp_lcd_touch_config_t tp_cfg = {
        //     .x_max = DISPLAY_WIDTH,
        //     .y_max = DISPLAY_HEIGHT,
        //     .rst_gpio_num = GPIO_NUM_NC, 
        //     .int_gpio_num = TOUCH_INT_GPIO, 
        //     .levels = {
        //         .reset = 0,
        //         .interrupt = 0,
        //     },
        //     .flags = {
        //         .swap_xy = 0,
        //         .mirror_x = 0,
        //         .mirror_y = 0,
        //     },
        // };

        // esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        // esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        // ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(touch_i2c_bus_, &tp_io_config, &tp_io_handle));
        // ESP_LOGI(TAG, "Initialize GT911 touch at addr 0x%02" PRIX32, tp_io_config.dev_addr);
        // esp_err_t tp_ret = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_);
        // if (tp_ret != ESP_OK) {
        //     ESP_LOGW(TAG, "GT911 init failed at 0x%02" PRIX32 ", trying backup addr 0x%02" PRIX32, tp_io_config.dev_addr, ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP);
        //     // Try backup address 0x14
        //     tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        //     tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP;
        //     ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(touch_i2c_bus_, &tp_io_config, &tp_io_handle));
        //     ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_));
        // }
    }

   

public:
    ESP32P4FunctionEvBoard() : boot_button_(BOOT_BUTTON_GPIO)
    {
        InitializeI2cBuses();
        InitializeTouchI2cBus();
        InitializeLCD();
        InitializeButtons();
        InitializeTouch();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_1, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
                                            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                                            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override { return display_; }

    virtual Backlight *GetBacklight() override
    {
#ifdef DISPLAY_BACKLIGHT_PIN
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
#else
        return nullptr;
#endif
    }
};

DECLARE_BOARD(ESP32P4FunctionEvBoard);
