#include "boards/wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "display/ssd1306_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "led.h"
#include "config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>

#define TAG "CompactWifiBoard"

class CompactWifiBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            Application::GetInstance().ToggleChatState();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("Volume\n" + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            auto codec = GetAudioCodec();
            codec->SetOutputVolume(100);
            GetDisplay()->ShowNotification("Volume\n100");
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("Volume\n" + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            auto codec = GetAudioCodec();
            codec->SetOutputVolume(0);
            GetDisplay()->ShowNotification("Volume\n0");
        });
    }

public:
    CompactWifiBoard() :
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
    }

    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing CompactWifiBoard");
        // Check if the reset button is pressed
        SystemReset::GetInstance().CheckButtons();

        InitializeDisplayI2c();
        InitializeButtons();

        WifiBoard::Initialize();
    }

    virtual Led* GetBuiltinLed() override {
        static Led led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        static Ssd1306Display display(display_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        return &display;
    }
};

DECLARE_BOARD(CompactWifiBoard);
