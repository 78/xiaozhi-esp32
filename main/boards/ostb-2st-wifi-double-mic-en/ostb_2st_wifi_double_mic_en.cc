#include "wifi_board.h"
#include "codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "assets/lang_config.h"
#include "power_save_timer.h"
#include "power_manager.h"

#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_nv3023.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <wifi_station.h>

#include <algorithm>

#define TAG "OstbDoubleMic"

// Board-specific NV3023 initialization sequence. The generic NV3023 defaults
// target a different panel variant and do not initialize this display.
static const nv3023_lcd_init_cmd_t kVendorNv3023Init[] = {
    {0xFD, (uint8_t[]){0x06, 0x08}, 2, 0},
    {0x61, (uint8_t[]){0x07, 0x04}, 2, 0},
    {0x62, (uint8_t[]){0x00, 0x44, 0x45}, 3, 0},
    {0x63, (uint8_t[]){0x41, 0x07, 0x12, 0x12}, 4, 0},
    {0x64, (uint8_t[]){0x37}, 1, 0},
    {0x65, (uint8_t[]){0x09, 0x10, 0x21}, 3, 0},
    {0x66, (uint8_t[]){0x09, 0x10, 0x21}, 3, 0},
    {0x67, (uint8_t[]){0x20, 0x40}, 2, 0},
    {0x68, (uint8_t[]){0x90, 0x4C, 0x7C, 0x66}, 4, 0},
    {0xB1, (uint8_t[]){0x0F, 0x02, 0x01}, 3, 0},
    {0xB4, (uint8_t[]){0x01}, 1, 0},
    {0xB5, (uint8_t[]){0x02, 0x02, 0x0A, 0x14}, 4, 0},
    {0xB6, (uint8_t[]){0x04, 0x01, 0x9F, 0x00, 0x02}, 5, 0},
    {0xDF, (uint8_t[]){0x11}, 1, 0},
    {0xE2, (uint8_t[]){0x13, 0x00, 0x00, 0x30, 0x33, 0x3F}, 6, 0},
    {0xE5, (uint8_t[]){0x3F, 0x33, 0x30, 0x00, 0x00, 0x13}, 6, 0},
    {0xE1, (uint8_t[]){0x00, 0x57}, 2, 0},
    {0xE4, (uint8_t[]){0x58, 0x00}, 2, 0},
    {0xE0, (uint8_t[]){0x01, 0x03, 0x0D, 0x0E, 0x0E, 0x0C, 0x15, 0x19}, 8, 0},
    {0xE3, (uint8_t[]){0x1A, 0x16, 0x0C, 0x0F, 0x0E, 0x0D, 0x02, 0x01}, 8, 0},
    {0xE6, (uint8_t[]){0x00, 0xFF}, 2, 0},
    {0xE7, (uint8_t[]){0x01, 0x04, 0x03, 0x03, 0x00, 0x12}, 6, 0},
    {0xE8, (uint8_t[]){0x00, 0x70, 0x00}, 3, 0},
    {0xEC, (uint8_t[]){0x52}, 1, 0},
    {0xF1, (uint8_t[]){0x01, 0x01, 0x02}, 3, 0},
    {0xF6, (uint8_t[]){0x09, 0x10, 0x00, 0x00}, 4, 0},
    {0xFD, (uint8_t[]){0xFA, 0xFC}, 2, 0},
    {0x3A, (uint8_t[]){0x05}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x36, (uint8_t[]){0x08}, 1, 0},
    {0x21, nullptr, 0, 0},
    {0x11, nullptr, 0, 200},
    {0x29, nullptr, 0, 10},
};

static_assert(sizeof(kVendorNv3023Init) / sizeof(kVendorNv3023Init[0]) == 33,
              "OSTB NV3023 initialization sequence must contain 33 commands");

class OSTB_2ST_WIFI : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    LcdDisplay* display_ = nullptr;
    PowerSaveTimer* power_save_timer_ = nullptr;
    PowerManager* power_manager_ = nullptr;

    void InitializePowerManager() {
        power_manager_ = new PowerManager(OSTB_CHARGE_DETECT_GPIO);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            power_save_timer_->SetEnabled(!is_charging);
            ESP_LOGI(TAG, "Charging %s", is_charging ? "started" : "stopped");
        });
    }

    void InitializeCodecI2c() {
        i2c_master_bus_config_t config = {
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&config, &codec_i2c_bus_));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
        });
        boot_button_.OnPressDown([this]() {
            power_save_timer_->WakeUp();
            Application::GetInstance().StartListening();
        });
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        volume_up_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            int volume = std::min(codec->output_volume() + 10, 100);
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_up_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            int volume = std::max(codec->output_volume() - 10, 0);
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_down_button_.OnLongPress([this]() {
            power_save_timer_->WakeUp();
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(240, 60, -1);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(1);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeDisplay() {
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = DISPLAY_SDA_PIN;
        bus_config.miso_io_num = GPIO_NUM_NC;
        bus_config.sclk_io_num = DISPLAY_SCL_PIN;
        bus_config.quadwp_io_num = GPIO_NUM_NC;
        bus_config.quadhd_io_num = GPIO_NUM_NC;
        bus_config.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &bus_config, SPI_DMA_CH_AUTO));

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        esp_lcd_panel_handle_t panel = nullptr;
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
        panel_config.bits_per_pixel = 16;
        nv3023_vendor_config_t vendor_config = {
            .init_cmds = kVendorNv3023Init,
            .init_cmds_size = sizeof(kVendorNv3023Init) / sizeof(kVendorNv3023Init[0]),
        };
        panel_config.vendor_config = &vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv3023(panel_io, &panel_config, &panel));

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

public:
    OSTB_2ST_WIFI()
        : boot_button_(BOOT_BUTTON_GPIO),
          volume_up_button_(VOLUME_UP_BUTTON_GPIO),
          volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializePowerSaveTimer();
        InitializePowerManager();
        InitializeButtons();
        InitializeDisplay();
        GetBacklight()->RestoreBrightness();
    }

    AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(codec_i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR,
            AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    Display* GetDisplay() override {
        return display_;
    }

    Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        level = power_manager_->GetBatteryLevel();
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        return true;
    }

    void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(OSTB_2ST_WIFI);
