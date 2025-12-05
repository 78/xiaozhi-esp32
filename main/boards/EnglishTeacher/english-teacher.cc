#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>

#ifdef SH1106
#include <esp_lcd_panel_sh1106.h>
#endif

#define TAG "EnglishTeacherBoard"

class EnglishTeacherBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button button_up_;
    Button button_left_;
    Button button_down_;
    Button button_right_;
    Button button_select_;
    Button button_start_;
    Button boot_button_;
    Button touch_button_;
    Button button_c;
    Button button_d;
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

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
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
        auto log_btn = [](const char* name, gpio_num_t gpio) {
            ESP_LOGW(TAG, "%s pressed (GPIO%d)", name, static_cast<int>(gpio));
        };

        button_up_.OnClick([log_btn]() { log_btn("Button UP", BUTTON_UP_GPIO); });
        button_left_.OnClick([log_btn]() { log_btn("Button LEFT", BUTTON_LEFT_GPIO); });
        button_down_.OnClick([log_btn]() { log_btn("Button DOWN", BUTTON_DOWN_GPIO); });
        button_right_.OnClick([log_btn]() { log_btn("Button RIGHT", BUTTON_RIGHT_GPIO); });

        button_select_.OnClick([log_btn]() { log_btn("Button SELECT", BUTTON_SELECT_GPIO); });
        button_start_.OnClick([log_btn]() { log_btn("Button START", BUTTON_START_GPIO); });


        button_c.OnClick([log_btn]() { log_btn("Button C", BUTTON_C_GPIO); });
        button_d.OnClick([log_btn]() { log_btn("Button D", BUTTON_D_GPIO); });

        boot_button_.OnClick([this]() {
            ESP_LOGW(TAG, "Button A (BOOT) pressed (GPIO%d)", static_cast<int>(BUTTON_A_GPIO));
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
        touch_button_.OnPressDown([this]() {
            ESP_LOGW(TAG, "Button B (TOUCH) down (GPIO%d)", static_cast<int>(BUTTON_B_GPIO));
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            ESP_LOGW(TAG, "Button B (TOUCH) up (GPIO%d)", static_cast<int>(BUTTON_B_GPIO));
            Application::GetInstance().StopListening();
        });

        volume_up_button_.OnClick([this]() {
            ESP_LOGW(TAG, "Button C (VOL+) pressed (GPIO%d)", static_cast<int>(BUTTON_C_GPIO));
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            ESP_LOGW(TAG, "Button C (VOL+) long (GPIO%d)", static_cast<int>(BUTTON_C_GPIO));
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            ESP_LOGW(TAG, "Button D (VOL-) pressed (GPIO%d)", static_cast<int>(BUTTON_D_GPIO));
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            ESP_LOGW(TAG, "Button D (VOL-) long (GPIO%d)", static_cast<int>(BUTTON_D_GPIO));
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    // 物联网初始化，逐步迁移到 MCP 协议
    void InitializeTools() {
        static LampController lamp(LAMP_GPIO);
    }

public:
    EnglishTeacherBoard() :
        button_up_(BUTTON_UP_GPIO),
        button_left_(BUTTON_LEFT_GPIO),
        button_down_(BUTTON_DOWN_GPIO),
        button_right_(BUTTON_RIGHT_GPIO),
        button_select_(BUTTON_SELECT_GPIO),
        button_start_(BUTTON_START_GPIO),
        boot_button_(BOOT_BUTTON_GPIO),
        touch_button_(TOUCH_BUTTON_GPIO),
        button_c(BUTTON_C_GPIO),
        button_d(BUTTON_D_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
       // InitializeDisplayI2c();
       // InitializeSsd1306Display();
        display_ = new NoDisplay(); // Disable display for English Teacher Board
        InitializeButtons();
        InitializeTools();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        // static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
        //     AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
            static NoAudioCodecSimplex audio_codec(
        AUDIO_INPUT_SAMPLE_RATE,    // mic 采样率
        AUDIO_OUTPUT_SAMPLE_RATE,   // speaker 采样率
        AUDIO_I2S_SPK_GPIO_BCLK,   // 喇叭 BCLK
        AUDIO_I2S_SPK_GPIO_LRCK,   // 喇叭 WS/LRCK
        AUDIO_I2S_SPK_GPIO_DOUT,   // 喇叭 DATA OUT
        I2S_STD_SLOT_RIGHT,         // 喇叭声道 RIGHT
        AUDIO_I2S_MIC_GPIO_SCK,    // MIC BCLK
        AUDIO_I2S_MIC_GPIO_WS,     // MIC WS/LRCK
        AUDIO_I2S_MIC_GPIO_DIN,    // MIC DATA IN
        I2S_STD_SLOT_LEFT           // MIC 声道 LEFT
    );
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

DECLARE_BOARD(EnglishTeacherBoard);
