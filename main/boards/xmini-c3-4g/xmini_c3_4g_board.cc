#include "ml307_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/oled_display.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "settings.h"
#include "config.h"
#include "sleep_timer.h"
#include "adc_battery_monitor.h"
#include "press_to_talk_mcp_tool.h"
#include "assets/lang_config.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#define TAG "XminiC3Board"

class XminiC3Board : public Ml307Board {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;
    SleepTimer* sleep_timer_ = nullptr;
    AdcBatteryMonitor* adc_battery_monitor_ = nullptr;
    PressToTalkMcpTool* press_to_talk_tool_ = nullptr;

    void InitializeBatteryMonitor() {
        adc_battery_monitor_ = new AdcBatteryMonitor(ADC_UNIT_1, ADC_CHANNEL_4, 100000, 100000, CHARGING_PIN);
        adc_battery_monitor_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                sleep_timer_->SetEnabled(false);
                // 插入充电器时播放charging.ogg音效
                Application::GetInstance().PlaySound(Lang::Sounds::OGG_CHARGING);
            } else {
                sleep_timer_->SetEnabled(true);
            }
        });
    }

    void InitializePowerSaveTimer() {
        // Wake word detection will be disabled in light sleep mode
        sleep_timer_ = new SleepTimer(30);
        sleep_timer_->OnEnterLightSleepMode([this]() {
            ESP_LOGI(TAG, "Enabling sleep mode");
            // Show the standby screen
            GetDisplay()->SetPowerSaveMode(true);
            // Enable sleep mode, and sleep in 1 second after DTR is set to high
            modem_->SetSleepMode(true, 1);
            // Set the DTR pin to high to make the modem enter sleep mode
            modem_->GetAtUart()->SetDtrPin(true);
        });
        sleep_timer_->OnExitLightSleepMode([this]() {
            // Set the DTR pin to low to make the modem wake up
            modem_->GetAtUart()->SetDtrPin(false);
            // Hide the standby screen
            GetDisplay()->SetPowerSaveMode(false);
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

        if (i2c_master_probe(codec_i2c_bus_, 0x18, 1000) != ESP_OK) {
            while (true) {
                ESP_LOGE(TAG, "Failed to probe I2C bus, please check if you have installed the correct firmware");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        }
    }

    void InitializeSsd1306Display() {
        display_ = new NoDisplay();
        return;
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

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    void InitializeButtons() {
        // 添加时间戳变量来精确区分单击和长按
        static int64_t press_down_time = 0;
        static const int64_t LONG_PRESS_THRESHOLD_MS = 500; // 长按阈值300ms
        
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            
            if (!press_to_talk_tool_ || !press_to_talk_tool_->IsPressToTalkEnabled()) {
                // 连续对话模式下的新交互逻辑
                DeviceState current_state = app.GetDeviceState();
                
                if (current_state == kDeviceStateIdle) {
                    // 设备处于非监听状态，先播放wake.ogg再进入监听
                    app.PlaySound(Lang::Sounds::OGG_WAKE);
                    // 延迟一小段时间让音效播放完毕
                    vTaskDelay(pdMS_TO_TICKS(500));
                    app.ToggleChatState();
                } else if (current_state == kDeviceStateListening) {
                    // 设备处于监听状态，播放bye.ogg再退出监听
                    app.PlaySound(Lang::Sounds::OGG_BYE);
                    // 延迟一小段时间让音效播放完毕
                    vTaskDelay(pdMS_TO_TICKS(500));
                    app.ToggleChatState();
                } else {
                    // 其他状态保持原有逻辑
                    app.ToggleChatState();
                }
            } else {
                // 长按对话模式下，只有在按下时间小于长按阈值时才播放音效
                int64_t current_time = esp_timer_get_time() / 1000; // 转换为毫秒
                int64_t press_duration = current_time - press_down_time;
                
                if (press_duration < LONG_PRESS_THRESHOLD_MS) {
                    // 真正的单击操作，播放音效
                    app.PlaySound(Lang::Sounds::OGG_MODE_PTT_BTN_SOUND);
                }
                // 否则是长按操作，不播放音效
            }
        });
        boot_button_.OnPressDown([this]() {
            // 记录按下时间
            press_down_time = esp_timer_get_time() / 1000; // 转换为毫秒
            
            if (press_to_talk_tool_ && press_to_talk_tool_->IsPressToTalkEnabled()) {
                // 长按对话模式下，长按BOOT按钮时不播放音效，直接开始监听
                Application::GetInstance().StartListening();
            }
        });
        boot_button_.OnPressUp([this]() {
            if (press_to_talk_tool_ && press_to_talk_tool_->IsPressToTalkEnabled()) {
                // 长按对话模式下，释放BOOT按钮时停止监听，不播放音效
                Application::GetInstance().StopListening();
            }
        });
    
        // 添加双击事件处理
        boot_button_.OnDoubleClick([this]() {
            if (press_to_talk_tool_) {
                bool current_mode = press_to_talk_tool_->IsPressToTalkEnabled();
                bool new_mode = !current_mode; // 切换模式
                
                // 使用Settings类直接保存设置，确保立即生效
                Settings settings("vendor", true);
                settings.SetInt("press_to_talk", new_mode ? 1 : 0);
                
                // 重新初始化工具
                delete press_to_talk_tool_;
                press_to_talk_tool_ = new PressToTalkMcpTool();
                press_to_talk_tool_->Initialize();
                
                auto& app = Application::GetInstance();
                auto display = Board::GetInstance().GetDisplay();
                
                if (new_mode) {
                    display->ShowNotification("已切换到长按说话模式");
                    app.PlaySound(Lang::Sounds::OGG_MODE_PTT);
                } else {
                    display->ShowNotification("已切换到单击说话模式");
                    app.PlaySound(Lang::Sounds::OGG_MODE_CONTINUOUS);
                }
                
                ESP_LOGI(TAG, "Press to talk mode switched to: %s", new_mode ? "press_to_talk" : "click_to_talk");
            }
        });
    }

    void InitializeTools() {
        press_to_talk_tool_ = new PressToTalkMcpTool();
        press_to_talk_tool_->Initialize();
    }

public:
    XminiC3Board() : Ml307Board(ML307_TX_PIN, ML307_RX_PIN, ML307_DTR_PIN),
        boot_button_(BOOT_BUTTON_GPIO, false, 0, 0, true) {

        InitializeBatteryMonitor();
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

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            sleep_timer_->WakeUp();
        }
        Ml307Board::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(XminiC3Board);