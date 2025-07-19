#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/oled_display.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "settings.h"
#include "config.h"
#include "sleep_timer.h"
#include "font_awesome_symbols.h"
#include "adc_battery_monitor.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define TAG "XminiC3Board"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);


class XminiC3Board : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;
    bool press_to_talk_enabled_ = false;
    SleepTimer* sleep_timer_ = nullptr;
    AdcBatteryMonitor* adc_battery_monitor_ = nullptr;

    void InitializePowerManager() {
        adc_battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_1, ADC_CHANNEL_3, 100000, 100000, GPIO_NUM_12);
        adc_battery_monitor_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                sleep_timer_->SetEnabled(false);
            } else {
                sleep_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
#if CONFIG_USE_ESP_WAKE_WORD
        sleep_timer_ = new SleepTimer(600);
#else
        sleep_timer_ = new SleepTimer(30);
#endif
        sleep_timer_->OnEnterLightSleepMode([this]() {
            ESP_LOGI(TAG, "Enabling sleep mode");
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("sleepy");
            
            auto codec = GetAudioCodec();
            codec->EnableInput(false);
        });
        sleep_timer_->OnExitLightSleepMode([this]() {
            auto codec = GetAudioCodec();
            codec->EnableInput(true);
            
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("neutral");
        });
        sleep_timer_->SetEnabled(true);
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

        // Print I2C bus info
        if (i2c_master_probe(codec_i2c_bus_, 0x18, 1000) != ESP_OK) {
            while (true) {
                ESP_LOGE(TAG, "Failed to probe I2C bus, please check if you have installed the correct firmware");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        }
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(codec_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
            {&font_puhui_14_1, &font_awesome_14_1});
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            if (!press_to_talk_enabled_) {
                app.ToggleChatState();
            }
        });
        boot_button_.OnPressDown([this]() {
            if (sleep_timer_) {
                sleep_timer_->WakeUp();
            }
            if (press_to_talk_enabled_) {
                Application::GetInstance().StartListening();
            }
        });
        boot_button_.OnPressUp([this]() {
            if (press_to_talk_enabled_) {
                Application::GetInstance().StopListening();
            }
        });
    }

    void InitializeTools() {
        Settings settings("vendor");
        press_to_talk_enabled_ = settings.GetInt("press_to_talk", 0) != 0;

        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.set_press_to_talk",
            "Switch between press to talk mode (长按说话) and click to talk mode (单击说话).\n"
            "The mode can be `press_to_talk` or `click_to_talk`.",
            PropertyList({
                Property("mode", kPropertyTypeString)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                auto mode = properties["mode"].value<std::string>();
                if (mode == "press_to_talk") {
                    SetPressToTalkEnabled(true);
                    return true;
                } else if (mode == "click_to_talk") {
                    SetPressToTalkEnabled(false);
                    return true;
                }
                throw std::runtime_error("Invalid mode: " + mode);
            });
    }

public:
    XminiC3Board() : boot_button_(BOOT_BUTTON_GPIO, false, 0, 0, true) {  
        InitializePowerManager();
        InitializePowerSaveTimer();
        InitializeCodecI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeTools();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        charging = adc_battery_monitor_->IsCharging();
        discharging = adc_battery_monitor_->IsDischarging();
        level = adc_battery_monitor_->GetBatteryLevel();
        return true;
    }

    void SetPressToTalkEnabled(bool enabled) {
        press_to_talk_enabled_ = enabled;

        Settings settings("vendor", true);
        settings.SetInt("press_to_talk", enabled ? 1 : 0);
        ESP_LOGI(TAG, "Press to talk enabled: %d", enabled);
    }

    bool IsPressToTalkEnabled() {
        return press_to_talk_enabled_;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            sleep_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(XminiC3Board);
