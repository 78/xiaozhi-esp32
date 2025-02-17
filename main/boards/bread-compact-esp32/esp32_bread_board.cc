#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "display/ssd1306_display.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

#define TAG "ESP32-MarsbearSupport"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);


class CompactWifiBoard : public WifiBoard {
private:
    Button boot_button_;
    Button touch_button_;
    Button asr_button_;

    i2c_master_bus_handle_t display_i2c_bus_;

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

    void InitializeButtons() {
        
        // 配置 GPIO
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << BUILTIN_LED_GPIO,  // 设置需要配置的 GPIO 引脚
            .mode = GPIO_MODE_OUTPUT,           // 设置为输出模式
            .pull_up_en = GPIO_PULLUP_DISABLE,  // 禁用上拉
            .pull_down_en = GPIO_PULLDOWN_DISABLE,  // 禁用下拉
            .intr_type = GPIO_INTR_DISABLE      // 禁用中断
        };
        gpio_config(&io_conf);  // 应用配置

        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            gpio_set_level(BUILTIN_LED_GPIO, 1);
            app.ToggleChatState();
        });

        asr_button_.OnClick([this]() {
            std::string wake_word="你好小智";
            Application::GetInstance().WakeWordInvoke(wake_word);
        });

        touch_button_.OnPressDown([this]() {
            gpio_set_level(BUILTIN_LED_GPIO, 1);
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            gpio_set_level(BUILTIN_LED_GPIO, 0);
            Application::GetInstance().StopListening();
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Lamp"));
    }

public:
    CompactWifiBoard() : boot_button_(BOOT_BUTTON_GPIO), touch_button_(TOUCH_BUTTON_GPIO), asr_button_(ASR_BUTTON_GPIO)
    {
        InitializeDisplayI2c();
        InitializeButtons();
        InitializeIot();
    }

    virtual AudioCodec* GetAudioCodec() override 
    {
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
        static Ssd1306Display display(display_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                    &font_puhui_14_1, &font_awesome_14_1);
        return &display;
    }

};

DECLARE_BOARD(CompactWifiBoard);
