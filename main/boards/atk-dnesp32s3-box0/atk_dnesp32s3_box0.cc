#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "power_save_timer.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "power_manager.h"

#include "i2c_device.h"
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <wifi_station.h>

#include <driver/rtc_io.h>
#include <esp_sleep.h>

#define TAG "atk_dnesp32s3_box0"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

class atk_dnesp32s3_box0  : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button right_button_;   
    Button left_button_;    
    Button middle_button_;
    LcdDisplay* display_;
    PowerSaveTimer* power_save_timer_;
    PowerManager* power_manager_;
    PowerSupply power_status_;
    LcdStatus LcdStatus_ = kDevicelcdbacklightOn;
    PowerSleep power_sleep_ = kDeviceNoSleep;
    WakeStatus wake_status_ = kDeviceAwakened;
    XiaozhiStatus XiaozhiStatus_ = kDevice_Exit_Distributionnetwork;
    esp_timer_handle_t wake_timer_handle_;
    esp_lcd_panel_io_handle_t panel_io = nullptr;
    esp_lcd_panel_handle_t panel = nullptr;

    int ticks_ = 0;
    const int kChgCtrlInterval = 5;

    void InitializeBoardPowerManager() {
        gpio_config_t gpio_init_struct = {0};
        gpio_init_struct.intr_type = GPIO_INTR_DISABLE;
        gpio_init_struct.mode = GPIO_MODE_INPUT_OUTPUT;
        gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        gpio_init_struct.pin_bit_mask = (1ull << CODEC_PWR_PIN) | (1ull << SYS_POW_PIN);
        gpio_config(&gpio_init_struct);

        gpio_set_level(CODEC_PWR_PIN, 1); 
        gpio_set_level(SYS_POW_PIN, 1); 

        gpio_config_t chg_init_struct = {0};

        chg_init_struct.intr_type = GPIO_INTR_DISABLE;
        chg_init_struct.mode = GPIO_MODE_INPUT;
        chg_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;
        chg_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        chg_init_struct.pin_bit_mask = 1ull << CHRG_PIN;
        ESP_ERROR_CHECK(gpio_config(&chg_init_struct));

        chg_init_struct.mode = GPIO_MODE_OUTPUT;
        chg_init_struct.pull_up_en = GPIO_PULLUP_DISABLE;
        chg_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;
        chg_init_struct.pin_bit_mask = 1ull << CHG_CTRL_PIN;
        ESP_ERROR_CHECK(gpio_config(&chg_init_struct));
        gpio_set_level(CHG_CTRL_PIN, 1);

        if (gpio_get_level(CHRG_PIN) == 0) {
            power_status_ = kDeviceTypecSupply;
        } else {
            power_status_ = kDeviceBatterySupply;
        }

        esp_timer_create_args_t wake_display_timer_args = {
            .callback = [](void *arg) {
                atk_dnesp32s3_box0* self = static_cast<atk_dnesp32s3_box0*>(arg);
                if (self->LcdStatus_ == kDevicelcdbacklightOff && Application::GetInstance().GetDeviceState() == kDeviceStateListening 
                    && self->wake_status_ == kDeviceWaitWake) {

                    if (self->power_sleep_ == kDeviceNeutralSleep) {
                        self->power_save_timer_->WakeUp();
                    }

                    self->GetBacklight()->RestoreBrightness();
                    self->wake_status_ = kDeviceAwakened;
                    self->LcdStatus_ = kDevicelcdbacklightOn;
                } else if (self->power_sleep_ == kDeviceNeutralSleep && Application::GetInstance().GetDeviceState() == kDeviceStateListening 
                         && self->LcdStatus_ != kDevicelcdbacklightOff && self->wake_status_ == kDeviceAwakened) {
                    self->power_save_timer_->WakeUp();
                    self->power_sleep_ = kDeviceNoSleep;
                } else {
                    self->ticks_ ++;
                    if (self->ticks_ % self->kChgCtrlInterval == 0) {
                        if (gpio_get_level(CHRG_PIN) == 0) {
                            self->power_status_ = kDeviceTypecSupply;
                        } else {
                            self->power_status_ = kDeviceBatterySupply;
                        }

                        if (self->power_manager_->low_voltage_ < 2877 && self->power_status_ != kDeviceTypecSupply) {
                            esp_timer_stop(self->power_manager_->timer_handle_);
                            gpio_set_level(CHG_CTRL_PIN, 0);
                            vTaskDelay(pdMS_TO_TICKS(100));
                            gpio_set_level(SYS_POW_PIN, 0);     
                            vTaskDelay(pdMS_TO_TICKS(100));
                        }
                    }
                }
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "wake_update_timer",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&wake_display_timer_args, &wake_timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(wake_timer_handle_, 300000));
    }

    void InitializePowerManager() {
        power_manager_ = new PowerManager(CHRG_PIN);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            power_sleep_ = kDeviceNeutralSleep;
            XiaozhiStatus_ = kDevice_join_Sleep;
            GetDisplay()->SetPowerSaveMode(true);

            if (LcdStatus_ != kDevicelcdbacklightOff) {
                GetBacklight()->SetBrightness(1);
            }
        });
        power_save_timer_->OnExitSleepMode([this]() {
            power_sleep_ = kDeviceNoSleep;
            GetDisplay()->SetPowerSaveMode(false);

            if (XiaozhiStatus_ != kDevice_Exit_Sleep) {
                GetBacklight()->RestoreBrightness();
            }
        });
        power_save_timer_->OnShutdownRequest([this]() {
            if (power_status_ == kDeviceBatterySupply) {
                esp_timer_stop(power_manager_->timer_handle_);
                gpio_set_level(CHG_CTRL_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(SYS_POW_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        });

        power_save_timer_->SetEnabled(true);
    }

    // Initialize I2C peripheral
    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)I2C_NUM_0,
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

    // Initialize spi peripheral
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = LCD_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = LCD_SCLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        middle_button_.OnClick([this]() {
            auto& app = Application::GetInstance();

            if (LcdStatus_ != kDevicelcdbacklightOff) {
                if (power_sleep_ == kDeviceNeutralSleep) {
                    power_save_timer_->WakeUp();
                    power_sleep_ = kDeviceNoSleep;
                }

                app.ToggleChatState();
            }
        });

        middle_button_.OnPressUp([this]() {
            if (LcdStatus_ == kDevicelcdbacklightOff) {
                Application::GetInstance().StopListening();
                Application::GetInstance().SetDeviceState(kDeviceStateIdle);
                wake_status_ = kDeviceWaitWake;
            }

            if (XiaozhiStatus_ == kDevice_Distributionnetwork || XiaozhiStatus_ == kDevice_Exit_Sleep) {
                esp_timer_stop(power_manager_->timer_handle_);
                gpio_set_level(CHG_CTRL_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_set_level(SYS_POW_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
            } else if (XiaozhiStatus_ == kDevice_join_Sleep) {
                GetBacklight()->RestoreBrightness();
                XiaozhiStatus_ = kDevice_null;
            }
        });

        middle_button_.OnLongPress([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }

            if (app.GetDeviceState() != kDeviceStateStarting || app.GetDeviceState() == kDeviceStateWifiConfiguring) {
                if (app.GetDeviceState() == kDeviceStateWifiConfiguring && power_status_ != kDeviceTypecSupply) {
                    GetBacklight()->SetBrightness(0);
                    XiaozhiStatus_ = kDevice_Distributionnetwork;
                } else if (power_status_ == kDeviceBatterySupply && LcdStatus_ != kDevicelcdbacklightOff) {
                    Application::GetInstance().StartListening();
                    GetBacklight()->SetBrightness(0);   
                    XiaozhiStatus_ = kDevice_Exit_Sleep;
                } else if (power_status_ == kDeviceTypecSupply && LcdStatus_ == kDevicelcdbacklightOn && Application::GetInstance().GetDeviceState() != kDeviceStateStarting) {
                    Application::GetInstance().StartListening();
                    GetBacklight()->SetBrightness(0);
                    LcdStatus_ = kDevicelcdbacklightOff;
                } else if (LcdStatus_ == kDevicelcdbacklightOff && (power_status_ == kDeviceTypecSupply || power_status_ == kDeviceBatterySupply)) {
                    GetDisplay()->SetChatMessage("system", "");
                    GetBacklight()->RestoreBrightness();
                    wake_status_ = kDeviceAwakened;
                    LcdStatus_ = kDevicelcdbacklightOn;
                }
            }
        });

        left_button_.OnClick([this]() {
            if (power_sleep_ == kDeviceNeutralSleep && LcdStatus_ != kDevicelcdbacklightOff) {
                power_save_timer_->WakeUp();
                power_sleep_ = kDeviceNoSleep;
            }

            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        left_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });

        right_button_.OnClick([this]() {
            if (power_sleep_ == kDeviceNeutralSleep && LcdStatus_ != kDevicelcdbacklightOff) {
                power_save_timer_->WakeUp();
                power_sleep_ = kDeviceNoSleep;
            }
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        right_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });


    }

    void InitializeSt7789Display() {
        ESP_LOGI(TAG, "Install panel IO");

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = LCD_CS_PIN;
        io_config.dc_gpio_num = LCD_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 7;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io);

        ESP_LOGI(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = LCD_RST_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel);
        
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY); 
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,
                                        .icon_font = &font_awesome_20_4,
                                        .emoji_font = DISPLAY_HEIGHT >= 240 ? font_emoji_64_init() : font_emoji_32_init(),
                                    });
    }

public:
    atk_dnesp32s3_box0() :
        right_button_(R_BUTTON_GPIO, false),
        left_button_(L_BUTTON_GPIO, false),
        middle_button_(M_BUTTON_GPIO, true) {
        InitializeBoardPowerManager();
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeI2c();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            i2c_bus_, 
            I2C_NUM_0, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            GPIO_NUM_NC, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            GPIO_NUM_NC, 
            AUDIO_CODEC_ES8311_ADDR, 
            false);
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

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(atk_dnesp32s3_box0);
