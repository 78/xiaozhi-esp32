#include "dual_network_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "power_save_timer.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "power_manager.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <wifi_station.h>

#include <driver/rtc_io.h>
#include <esp_sleep.h>

#define TAG "PHYSTON_ESP32S3_1_54TFT"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);


class PHYSTON_ESP32S3_1_54TFT : public DualNetworkBoard {
private:
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    SpiLcdDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    esp_timer_handle_t volume_up_timer_ = nullptr;
    esp_timer_handle_t volume_down_timer_ = nullptr;
    bool is_pressed_volume_up_ = false;
    bool is_pressed_volume_down_ = false;

    void InitializePowerManager() {
        power_manager_ = new PowerManager(GPIO_NUM_47);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        rtc_gpio_init(GPIO_NUM_21);
        rtc_gpio_set_direction(GPIO_NUM_21, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_set_level(GPIO_NUM_21, 1);

        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            ESP_LOGI(TAG, "Enabling sleep mode");
            display_->SetChatMessage("system", "");
            display_->SetEmotion("sleepy");
            GetBacklight()->SetBrightness(1);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            display_->SetChatMessage("system", "");
            display_->SetEmotion("neutral");
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            // ESP_LOGI(TAG, "Shutting down");
            // rtc_gpio_set_level(GPIO_NUM_21, 0);
            // // 启用保持功能，确保睡眠期间电平不变
            // rtc_gpio_hold_en(GPIO_NUM_21);
            // esp_lcd_panel_disp_on_off(panel_, false); //关闭显示
            // esp_deep_sleep_start();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SDA;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SCL;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            if (GetNetworkType() == NetworkType::WIFI) {
                if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                    // cast to WifiBoard
                    auto& wifi_board = static_cast<WifiBoard&>(GetCurrentBoard());
                    wifi_board.ResetWifiConfiguration();
                }
            }
            app.ToggleChatState();
        });
        boot_button_.OnDoubleClick([this]() {
            SwitchNetworkType();
            // auto& app = Application::GetInstance();
            // if (app.GetDeviceState() == kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring) {
            //     SwitchNetworkType();
            // }
        });

        volume_up_button_.OnPressDown([this]() {
            is_pressed_volume_up_ = true;
        });

        volume_up_button_.OnPressUp([this]() {
            is_pressed_volume_up_ = false;
             // 停止定时器
             if (volume_up_timer_) {
                esp_timer_stop(volume_up_timer_);
                esp_timer_delete(volume_up_timer_);
                volume_up_timer_ = nullptr;
            }
        });

        volume_up_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            // power_save_timer_->WakeUp();
            // GetAudioCodec()->SetOutputVolume(100);
            // GetDisplay()->ShowNotification(Lang::Strings::MUTED);

            esp_timer_create_args_t timer_args = {
                .callback = [](void* arg) {
                    auto self = static_cast<PHYSTON_ESP32S3_1_54TFT*>(arg);
                    auto codec = self->GetAudioCodec();
                    auto volume = codec->output_volume() + 1;
                    if (volume > 100) volume = 100;
                    codec->SetOutputVolume(volume);
                    self->GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
                },
                .arg = this,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "volume_up_timer"
            };
            ESP_ERROR_CHECK(esp_timer_create(&timer_args, &volume_up_timer_));
            ESP_ERROR_CHECK(esp_timer_start_periodic(volume_up_timer_, 100000));  // 100ms周期触发
            power_save_timer_->WakeUp();
        });

        volume_down_button_.OnPressDown([this]() {
            is_pressed_volume_down_ = true;
        });

        volume_down_button_.OnPressUp([this]() {
            is_pressed_volume_down_ = false;
             // 停止定时器
             if (volume_down_timer_) {
                esp_timer_stop(volume_down_timer_);
                esp_timer_delete(volume_down_timer_);
                volume_down_timer_ = nullptr;
            }
        });

        volume_down_button_.OnClick([this]() {
            power_save_timer_->WakeUp();
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            // power_save_timer_->WakeUp();
            // GetAudioCodec()->SetOutputVolume(0);
            // GetDisplay()->ShowNotification(Lang::Strings::MUTED);

            esp_timer_create_args_t timer_args = {
                .callback = [](void* arg) {
                    auto self = static_cast<PHYSTON_ESP32S3_1_54TFT*>(arg);
                    auto codec = self->GetAudioCodec();
                    auto volume = codec->output_volume() - 1;
                    if (volume < 0) volume = 0;
                    codec->SetOutputVolume(volume);
                    self->GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
                },
                .arg = this,
                .dispatch_method = ESP_TIMER_TASK,
                .name = "volume_down_timer"
            };
            ESP_ERROR_CHECK(esp_timer_create(&timer_args, &volume_down_timer_));
            ESP_ERROR_CHECK(esp_timer_start_periodic(volume_down_timer_, 100000));  // 100ms周期触发
            power_save_timer_->WakeUp();
        });
    }

    void InitializeSt7789Display() {
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS;
        io_config.dc_gpio_num = DISPLAY_DC;
        io_config.spi_mode = 3;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io_));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RES;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io_, &panel_config, &panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));
        ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_, DISPLAY_SWAP_XY));
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y));
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, true));

        display_ = new SpiLcdDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
        {
            .text_font = &font_puhui_16_4,
            .icon_font = &font_awesome_16_4,
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
            .emoji_font = font_emoji_32_init(),
#else
            .emoji_font = font_emoji_64_init(),
#endif
        });
    }

    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
        thing_manager.AddThing(iot::CreateThing("Battery"));
    }

public:
    PHYSTON_ESP32S3_1_54TFT() :
        DualNetworkBoard(ML307_TX_PIN, ML307_RX_PIN, 4096),
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeSpi();
        InitializeButtons();
        InitializeSt7789Display();
        InitializeIot();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = power_manager_->GetBatteryLevel();
        return true;
    }

    virtual bool GetBatteryVoltage(float& voltage) override {
        voltage = power_manager_->GetBatteryVoltage();
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        DualNetworkBoard::SetPowerSaveMode(enabled);
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }
};

DECLARE_BOARD(PHYSTON_ESP32S3_1_54TFT);
