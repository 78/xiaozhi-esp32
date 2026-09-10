#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "display/oled_home_screen.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_wifi.h>
#include <lvgl.h>

LV_FONT_DECLARE(lv_font_montserrat_24);

#ifdef SH1106
#include <esp_lcd_panel_sh1106.h>
#endif

#define TAG "CompactWifiBoard"

class CompactWifiBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    OledHomeScreen* home_screen_ = nullptr;
    esp_timer_handle_t home_init_timer_ = nullptr;
    esp_timer_handle_t state_poll_timer_ = nullptr;
    DeviceState last_state_ = kDeviceStateUnknown;
    Button boot_button_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .scl_speed_hz = 400 * 1000,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

#ifdef SH1106
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_));
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
#endif
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
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
        touch_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            // 更新首页音量图标
            if (home_screen_) {
                home_screen_->SetVolumeLevel(volume);
            }
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            if (home_screen_) {
                home_screen_->SetVolumeLevel(100);
            }
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            // 更新首页音量图标
            if (home_screen_) {
                home_screen_->SetVolumeLevel(volume);
            }
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            if (home_screen_) {
                home_screen_->SetVolumeLevel(0);
            }
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    // 检查WiFi是否已连接
    bool IsWifiConnected() {
        wifi_ap_record_t ap_info;
        return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
    }

    // 根据设备状态切换首页/默认界面
    void UpdateDisplayByState(DeviceState state) {
        if (!home_screen_) return;

        // 这些状态下显示首页（空闲状态）
        bool should_show_home = (state == kDeviceStateIdle);

        if (should_show_home && !home_screen_->IsVisible()) {
            // 切换到首页前，更新状态
            home_screen_->SetWifiStatus(IsWifiConnected());
            home_screen_->SetVolumeLevel(GetAudioCodec()->output_volume());
            home_screen_->Show();
        } else if (!should_show_home && home_screen_->IsVisible()) {
            // 切换回默认界面
            home_screen_->Hide();
        }
    }

    // 状态轮询定时器回调
    void StartStatePolling() {
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                auto* board = static_cast<CompactWifiBoard*>(arg);
                auto& app = Application::GetInstance();
                DeviceState current_state = app.GetDeviceState();
                
                // 状态变化时更新显示
                if (current_state != board->last_state_) {
                    ESP_LOGI(TAG, "Device state changed: %d -> %d", board->last_state_, current_state);
                    board->last_state_ = current_state;
                    board->UpdateDisplayByState(current_state);
                }
                
                // 空闲状态下定期更新WiFi状态
                if (current_state == kDeviceStateIdle && board->home_screen_ && board->home_screen_->IsVisible()) {
                    board->home_screen_->SetWifiStatus(board->IsWifiConnected());
                }
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "state_poll_timer",
            .skip_unhandled_events = true
        };
        esp_timer_create(&timer_args, &state_poll_timer_);
        esp_timer_start_periodic(state_poll_timer_, 500000); // 500ms轮询
    }

    void InitializeHomeScreen() {
        if (!display_) return;
        
        auto theme = static_cast<LvglTheme*>(display_->GetTheme());
        if (!theme) return;
        
        // 创建首页，传入可见性变化回调
        home_screen_ = new OledHomeScreen(
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            theme->text_font()->font(),
            theme->icon_font()->font(),
            &lv_font_montserrat_24,
            [this](bool home_visible) {
                // 首页显示时隐藏默认界面，反之亦然
                auto oled_display = static_cast<OledDisplay*>(display_);
                if (home_visible) {
                    oled_display->HideDefaultUI();
                } else {
                    oled_display->ShowDefaultUI();
                }
            }
        );
        
        // 延迟初始化首页，等待OledDisplay的SetupUI完成
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                auto* board = static_cast<CompactWifiBoard*>(arg);
                if (board->home_screen_) {
                    board->home_screen_->Initialize();
                    // 初始化完成后启动状态轮询
                    board->StartStatePolling();
                }
                esp_timer_delete(board->home_init_timer_);
                board->home_init_timer_ = nullptr;
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "home_init_delay",
            .skip_unhandled_events = true
        };
        esp_timer_create(&timer_args, &home_init_timer_);
        esp_timer_start_once(home_init_timer_, 500000); // 500ms延迟
    }

    // 物联网初始化，逐步迁移到 MCP 协议
    void InitializeTools() {
        static LampController lamp(LAMP_GPIO);
    }

public:
    CompactWifiBoard() :
        boot_button_(BOOT_BUTTON_GPIO),
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeTools();
        InitializeHomeScreen();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(CompactWifiBoard);
