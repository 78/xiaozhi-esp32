#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "boards/common/adc_battery_monitor.h"

#include "assets/lang_config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_nv3022b.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <driver/sdmmc_host.h>

#define TAG "CyberAiSpkS3"

static const nv3022b_lcd_init_cmd_t nv3030b_lcd_init_cmds[] = {
    {0xFD, (uint8_t[]){0x06, 0x08}, 2, 0},
    {0x61, (uint8_t[]){0x07, 0x04}, 2, 0},
    {0x62, (uint8_t[]){0x00, 0x44, 0x45}, 3, 0},
    {0x63, (uint8_t[]){0x41, 0x07, 0x12, 0x12}, 4, 0},
    {0x64, (uint8_t[]){0x37}, 1, 0},
    {0x65, (uint8_t[]){0x09, 0x10, 0x21}, 3, 0},
    {0x66, (uint8_t[]){0x09, 0x10, 0x21}, 3, 0},
    {0x67, (uint8_t[]){0x20, 0x40}, 2, 0},
    {0x68, (uint8_t[]){0x90, 0x4C, 0x7C, 0x66}, 4, 0},
    {0xB1, (uint8_t[]){0x0F, 0x08, 0x01}, 3, 0},
    {0xB4, (uint8_t[]){0x01}, 1, 0},
    {0xB5, (uint8_t[]){0x02, 0x02, 0x0A, 0x14}, 4, 0},
    {0xB6, (uint8_t[]){0x04, 0x01, 0x9F, 0x00, 0x02}, 5, 0},
    {0xE0, (uint8_t[]){0x01, 0x03, 0x0D, 0x0E, 0x0E, 0x0C, 0x15, 0x19}, 8, 0},
    {0xE1, (uint8_t[]){0x00, 0x57}, 2, 0},
    {0xE2, (uint8_t[]){0x13, 0x00, 0x00, 0x30, 0x33, 0x3F}, 6, 0},
    {0xE3, (uint8_t[]){0x1A, 0x16, 0x0C, 0x0F, 0x0E, 0x0D, 0x02, 0x01}, 8, 0},
    {0xE4, (uint8_t[]){0x58, 0x00}, 2, 0},
    {0xE5, (uint8_t[]){0x3F, 0x33, 0x30, 0x00, 0x00, 0x13}, 6, 0},
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
    {0x21, NULL, 0, 0},
    {0x11, NULL, 0, 120},
    {0x29, NULL, 0, 50},
};

class CyberAiSpkS3Wifi : public WifiBoard {
private:
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    LcdDisplay* display_ = nullptr;
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    AdcBatteryMonitor* adc_battery_monitor_ = nullptr;
    bool last_charge_state_ = false;
    uint32_t last_state_change_time_ = 0;
    int battery_state_ = 0;

    static constexpr uint32_t kStateStableTimeMs = 5000;

    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));

        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << AUDIO_CODEC_PA_PIN),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        gpio_set_level(AUDIO_CODEC_PA_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = 20 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        nv3022b_vendor_config_t nv3030b_vendor_config = {
            .init_cmds = nv3030b_lcd_init_cmds,
            .init_cmds_size = sizeof(nv3030b_lcd_init_cmds) / sizeof(nv3022b_lcd_init_cmd_t),
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = &nv3030b_vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_nv3022b(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X,
                                     DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                     DISPLAY_SWAP_XY);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });
        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    void InitializeBatteryMonitor() {
        adc_battery_monitor_ =
            new AdcBatteryMonitor(ADC_UNIT_1, VBAT_ADC_CHANNEL, 100000, 100000, CHRG_PIN);
    }

    void InitializeTfCard() {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << SD_CD),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        if (gpio_get_level(SD_CD) == 1) {
            ESP_LOGW(TAG, "TF card not inserted");
            return;
        }

        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024,
        };

        sdmmc_card_t* card = nullptr;
        const char mount_point[] = MOUNT_POINT;
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
        slot_config.width = 1;
        slot_config.clk = BSP_SD_CLK;
        slot_config.cmd = BSP_SD_CMD;
        slot_config.d0 = BSP_SD_D0;
        slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

        esp_err_t ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
            return;
        }

        ESP_LOGI(TAG, "SD card mounted at %s", mount_point);
        sdmmc_card_print_info(stdout, card);
    }

    bool DetectChargingState() {
        const int charging_samples = 5;
        int high_count = 0;

        for (int i = 0; i < charging_samples; i++) {
            if (gpio_get_level(CHRG_PIN) == 1) {
                high_count++;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        float high_ratio = static_cast<float>(high_count) / charging_samples;
        bool new_charge_state = last_charge_state_;

        if (high_ratio == 0.0f) {
            new_charge_state = false;
        } else if (high_ratio > 0.1f && high_ratio < 0.8f) {
            new_charge_state = true;
        }

        uint32_t current_time = xTaskGetTickCount();
        if (new_charge_state != last_charge_state_) {
            if (current_time - last_state_change_time_ > pdMS_TO_TICKS(kStateStableTimeMs)) {
                last_charge_state_ = new_charge_state;
                last_state_change_time_ = current_time;
            }
        }

        return last_charge_state_;
    }

public:
    CyberAiSpkS3Wifi()
        : boot_button_(BOOT_BUTTON_GPIO),
          volume_up_button_(VOLUME_UP_BUTTON_GPIO),
          volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeSpi();
        InitializeLcdDisplay();
        InitializeButtons();
        InitializeBatteryMonitor();
        InitializeTfCard();
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }
    }

    virtual Led* GetLed() override {
        static SingleLed led(LAMP_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, I2C_NUM_1, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (adc_battery_monitor_ == nullptr) {
            return false;
        }

        level = adc_battery_monitor_->GetBatteryLevel();
        charging = DetectChargingState();
        discharging = !charging;

        if (level <= 10 && discharging) {
            GetBacklight()->SetBrightness(100);
            battery_state_ = 1;
        }
        if (level > 10 || charging) {
            if (battery_state_ == 1) {
                GetBacklight()->SetBrightness(40);
                battery_state_ = 0;
            }
        }

        return true;
    }
};

DECLARE_BOARD(CyberAiSpkS3Wifi);
