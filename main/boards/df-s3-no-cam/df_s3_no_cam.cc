#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "display/oled_display.h"
#include "wifi_manager.h"

#include "led/gpio_led.h"
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_sh1106.h>
#include <esp_log.h>

#define TAG "DfrobotEsp32S3NoCam"

class DfrobotEsp32S3NoCam : public WifiBoard {
private:
    Button boot_button_;
    Display* display_ = nullptr;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;

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
        if (i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_) != ESP_OK) {
            i2c_bus_ = nullptr;
        }
    }

    void InitializeDisplay() {
        if (!i2c_bus_) {
            display_ = new NoDisplay();
            return;
        }

        const uint8_t addrs[] = {0x3C, 0x3D};
        for (auto addr : addrs) {
            esp_lcd_panel_io_handle_t panel_io = nullptr;
            esp_lcd_panel_handle_t panel = nullptr;

            esp_lcd_panel_io_i2c_config_t io_cfg = {
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

            if (esp_lcd_new_panel_io_i2c_v2(i2c_bus_, &io_cfg, &panel_io) != ESP_OK) {
                continue;
            }

#if DISPLAY_USE_SH1106
            esp_lcd_panel_sh1106_config_t sh1106_cfg = {};
            esp_lcd_panel_dev_config_t panel_cfg = {
                .reset_gpio_num = -1,
                .bits_per_pixel = 1,
                .vendor_config = &sh1106_cfg,
            };
            if (esp_lcd_new_panel_sh1106(panel_io, &panel_cfg, &panel) != ESP_OK) {
                continue;
            }
#else
            esp_lcd_panel_ssd1306_config_t ssd1306_cfg = {
                .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
            };
            esp_lcd_panel_dev_config_t panel_cfg = {
                .reset_gpio_num = -1,
                .bits_per_pixel = 1,
                .vendor_config = &ssd1306_cfg,
            };
            if (esp_lcd_new_panel_ssd1306(panel_io, &panel_cfg, &panel) != ESP_OK) {
                continue;
            }
#endif

            if (esp_lcd_panel_reset(panel) != ESP_OK) continue;
            if (esp_lcd_panel_init(panel) != ESP_OK) continue;
            // Force normal polarity for modules that boot in reverse mode
            esp_lcd_panel_invert_color(panel, false);
            esp_lcd_panel_disp_on_off(panel, true);

            display_ = new OledDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
            display_->SetEmotion("happy");
            display_->SetStatus("DF-S3 NoCam");
            ESP_LOGI(TAG, "OLED initialized at 0x%02X", addr);
            return;
        }

        display_ = new NoDisplay();
    }

public:
    DfrobotEsp32S3NoCam() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeButtons();
        InitializeI2c();
        InitializeDisplay();


    }

    virtual Led* GetLed() override {
        static GpioLed led(BUILTIN_LED_GPIO, 0);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplexPdm audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(DfrobotEsp32S3NoCam);
