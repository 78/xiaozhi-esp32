#include "wifi_board.h"
#include "audio/codecs/es8311_audio_codec.h"
// Display
#include "display/display.h"
#include "display/lcd_display.h"
#include "lvgl_theme.h"
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

// MIPI-DSI / LCD vendor includes (library may replace some)
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_ek79007.h"
#include "esp_lcd_touch_gt911.h"

// Library includes
#include "bsp/esp32_p4_function_ev_board.h"
#include "bsp/touch.h"

#define TAG "ESP32P4FuncEV"

class ESP32P4FunctionEvBoard : public WifiBoard
{
private:
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    Button boot_button_;
    LcdDisplay *display_ = nullptr;
    esp_lcd_touch_handle_t tp_ = nullptr;

    void InitializeI2cBuses()
    {
        ESP_ERROR_CHECK(bsp_i2c_init());
        codec_i2c_bus_ = bsp_i2c_get_handle();
    }

    // Touch I2C bus initialization is not required for this board (handled elsewhere)
    void InitializeTouchI2cBus()
    {
        // No implementation needed
    }

    void InitializeLCD()
    {
        bsp_display_config_t config = {
            .hdmi_resolution = BSP_HDMI_RES_NONE,
            .dsi_bus = {
                .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
                .lane_bit_rate_mbps = 1000,
            },
        };

        bsp_lcd_handles_t handles;
        ESP_ERROR_CHECK(bsp_display_new_with_handles(&config, &handles));

        display_ = new MipiLcdDisplay(handles.io, handles.panel, 1024, 600, 0, 0, true, true, false);
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
        ESP_ERROR_CHECK(bsp_touch_new(NULL, &tp_));
    }

    void InitializeSdCard()
    {
        ESP_LOGI(TAG, "Initializing SD card");
        esp_err_t ret = bsp_sdcard_mount();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "SD card mounted successfully");
        }
    }

    void InitializeCamera()
    {
        ESP_LOGI(TAG, "Initializing camera");
        bsp_camera_cfg_t camera_cfg = {0};
        esp_err_t ret = bsp_camera_start(&camera_cfg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize camera: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Camera initialized successfully");
        }
    }

    void InitializeFonts()
    {
        ESP_LOGI(TAG, "Initializing font support");
        // Font initialization is handled by the Assets system
        // The board supports loading fonts from assets partition
        // Verify that fonts are properly loaded by checking theme
        auto& theme_manager = LvglThemeManager::GetInstance();
        auto current_theme = theme_manager.GetTheme("light");
        if (current_theme != nullptr) {
            auto text_font = current_theme->text_font();
            if (text_font != nullptr && text_font->font() != nullptr) {
                ESP_LOGI(TAG, "Custom font loaded successfully: line_height=%d", text_font->font()->line_height);
            } else {
                ESP_LOGW(TAG, "Custom font not loaded, using built-in font");
            }
        }
    }

public:

    ESP32P4FunctionEvBoard() : boot_button_(0)
    {
        InitializeI2cBuses();
        // Audio is initialized by Es8311AudioCodec
        InitializeLCD();
        InitializeButtons();
        InitializeTouch();
        InitializeSdCard();
        InitializeCamera();
        InitializeFonts();
        GetBacklight()->RestoreBrightness();
    }

    ~ESP32P4FunctionEvBoard()
    {
        // Clean up display pointer
        delete display_;
        display_ = nullptr;
        // Unmount SD card
        esp_err_t ret = bsp_sdcard_unmount();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to unmount SD card: %s", esp_err_to_name(ret));
        }
        // If other resources need cleanup, add here
    }

    virtual AudioCodec *GetAudioCodec() override
    {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, (i2c_port_t)BSP_I2C_NUM, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            BSP_I2S_MCLK, BSP_I2S_SCLK, BSP_I2S_LCLK, BSP_I2S_DOUT, BSP_I2S_DSIN,
            BSP_POWER_AMP_IO, ES8311_CODEC_DEFAULT_ADDR, true, false);
        return &audio_codec;
    }

    virtual Display *GetDisplay() override { return display_; }

    virtual Backlight *GetBacklight() override
    {
        static PwmBacklight backlight(BSP_LCD_BACKLIGHT, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(ESP32P4FunctionEvBoard);
