#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/gpio_led.h"
#include "mcp_server.h"

#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_sh1106.h>
#include <esp_log.h>

#define TAG "MarkOneBoard"

class MarkOneSilentCodec : public NoAudioCodec {
public:
    MarkOneSilentCodec() {
        duplex_ = false;
        input_sample_rate_ = AUDIO_INPUT_SAMPLE_RATE;
        output_sample_rate_ = AUDIO_OUTPUT_SAMPLE_RATE;
    }
};

class MarkOneBoard : public WifiBoard {
private:
    Button boot_button_;
    Display* display_ = nullptr;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    int servo_angles_[2] = {90, 90};

    static constexpr ledc_mode_t kServoSpeedMode = LEDC_LOW_SPEED_MODE;
    static constexpr ledc_timer_t kServoTimer = LEDC_TIMER_0;
    static constexpr ledc_timer_bit_t kServoDutyRes = LEDC_TIMER_14_BIT;
    static constexpr uint32_t kServoFreqHz = 50;
    static constexpr uint32_t kServoMinUs = 500;
    static constexpr uint32_t kServoMaxUs = 2500;
    static constexpr uint32_t kServoPeriodUs = 20000;

    uint32_t AngleToDuty(int angle) {
        if (angle < 0) angle = 0;
        if (angle > 180) angle = 180;
        uint32_t pulse_us = kServoMinUs + (uint32_t)((kServoMaxUs - kServoMinUs) * angle / 180);
        uint32_t max_duty = (1u << kServoDutyRes) - 1u;
        return (pulse_us * max_duty) / kServoPeriodUs;
    }

    void SetServoAngleInternal(int idx, int angle) {
        if (idx < 0 || idx > 1) return;
        uint32_t duty = AngleToDuty(angle);
        ledc_channel_t ch = (idx == 0) ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1;
        if (ledc_set_duty(kServoSpeedMode, ch, duty) != ESP_OK) return;
        if (ledc_update_duty(kServoSpeedMode, ch) != ESP_OK) return;
        servo_angles_[idx] = angle;
    }

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
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
        esp_err_t err = i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
            i2c_bus_ = nullptr;
        }
    }

    void InitializeDisplay() {
        if (i2c_bus_ == nullptr) {
            ESP_LOGE(TAG, "I2C not ready, using NoDisplay");
            display_ = new NoDisplay();
            return;
        }
        const uint8_t addrs[] = {0x3C, 0x3D};

        for (auto addr : addrs) {
            esp_lcd_panel_io_i2c_config_t io_config = {
                .dev_addr = addr,
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
                .scl_speed_hz = 100 * 1000,
            };

            panel_io_ = nullptr;
            panel_ = nullptr;
            if (esp_lcd_new_panel_io_i2c_v2(i2c_bus_, &io_config, &panel_io_) != ESP_OK) {
                continue;
            }

#if DISPLAY_USE_SH1106
            esp_lcd_panel_sh1106_config_t sh1106_config = {};
            esp_lcd_panel_dev_config_t panel_config = {
                .reset_gpio_num = -1,
                .bits_per_pixel = 1,
                .vendor_config = &sh1106_config,
            };
            if (esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_) != ESP_OK) {
                continue;
            }
#else
            esp_lcd_panel_ssd1306_config_t ssd1306_config = {
                .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
            };
            esp_lcd_panel_dev_config_t panel_config = {
                .reset_gpio_num = -1,
                .bits_per_pixel = 1,
                .vendor_config = &ssd1306_config,
            };
            if (esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_) != ESP_OK) {
                continue;
            }
#endif
            if (esp_lcd_panel_reset(panel_) != ESP_OK) {
                continue;
            }
            if (esp_lcd_panel_init(panel_) != ESP_OK) {
                continue;
            }
            esp_lcd_panel_disp_on_off(panel_, true);

#if DISPLAY_USE_SH1106
            ESP_LOGI(TAG, "SH1106 initialized at I2C address 0x%02X", addr);
#else
            ESP_LOGI(TAG, "SSD1306 initialized at I2C address 0x%02X", addr);
#endif
            display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
            display_->SetEmotion("happy");
            display_->SetStatus("Mark One");
            return;
        }

        ESP_LOGE(TAG, "Failed to initialize OLED at 0x3C/0x3D, using NoDisplay");
        display_ = new NoDisplay();
    }

    void InitializeServos() {
        ledc_timer_config_t timer_cfg = {
            .speed_mode = kServoSpeedMode,
            .duty_resolution = kServoDutyRes,
            .timer_num = kServoTimer,
            .freq_hz = kServoFreqHz,
            .clk_cfg = LEDC_AUTO_CLK,
            .deconfigure = false,
        };
        esp_err_t err = ledc_timer_config(&timer_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(err));
            return;
        }

        ledc_channel_config_t ch0 = {
            .gpio_num = SERVO1_GPIO,
            .speed_mode = kServoSpeedMode,
            .channel = LEDC_CHANNEL_0,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = kServoTimer,
            .duty = 0,
            .hpoint = 0,
            .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
            .flags = { .output_invert = 0 },
        };
        err = ledc_channel_config(&ch0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_channel_config ch0 failed: %s", esp_err_to_name(err));
            return;
        }

        ledc_channel_config_t ch1 = {
            .gpio_num = SERVO2_GPIO,
            .speed_mode = kServoSpeedMode,
            .channel = LEDC_CHANNEL_1,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = kServoTimer,
            .duty = 0,
            .hpoint = 0,
            .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
            .flags = { .output_invert = 0 },
        };
        err = ledc_channel_config(&ch1);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ledc_channel_config ch1 failed: %s", esp_err_to_name(err));
            return;
        }

        SetServoAngleInternal(0, servo_angles_[0]);
        SetServoAngleInternal(1, servo_angles_[1]);
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
    }

    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();

        mcp_server.AddTool("robot.servo.set",
            "Set one servo angle (degrees). channel: 1 or 2, angle: 0~180.",
            PropertyList({
                Property("channel", kPropertyTypeInteger, 1, 1, 2),
                Property("angle", kPropertyTypeInteger, 90, 0, 180)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int channel = properties["channel"].value<int>();
                int angle = properties["angle"].value<int>();
                int idx = channel - 1;
                SetServoAngleInternal(idx, angle);
                return std::string("{\"status\":\"ok\",\"channel\":") + std::to_string(channel) +
                       ",\"angle\":" + std::to_string(servo_angles_[idx]) + "}";
            });

        mcp_server.AddTool("robot.servo.home",
            "Move both servos to 90 degrees.",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                (void)properties;
                SetServoAngleInternal(0, 90);
                SetServoAngleInternal(1, 90);
                return "{\"status\":\"ok\",\"servo1\":90,\"servo2\":90}";
            });

        mcp_server.AddTool("robot.servo.get",
            "Get current cached servo angles.",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                (void)properties;
                return std::string("{\"servo1\":") + std::to_string(servo_angles_[0]) +
                       ",\"servo2\":" + std::to_string(servo_angles_[1]) + "}";
            });
    }

public:
    MarkOneBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeDisplay();
        InitializeServos();
        InitializeButtons();
        InitializeTools();
    }

    virtual Led* GetLed() override {
        static GpioLed led(BUILTIN_LED_GPIO, 0);
        return &led;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static MarkOneSilentCodec audio_codec;
        return &audio_codec;
    }
};

DECLARE_BOARD(MarkOneBoard);
