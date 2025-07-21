#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/gpio_led.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>

#define TAG "DoitS3AiBox"

class DoitS3AiBox : public WifiBoard {
private:
    Button boot_button_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    uint8_t click_times;
    uint32_t check_time;

    void InitializeButtons() {
        click_times = 0;
        check_time = 0;
        boot_button_.OnClick([this]() {
            if(click_times==0) {
                check_time = esp_timer_get_time()/1000;
            }
            if(esp_timer_get_time()/1000-check_time<1000) {
                click_times++;
                check_time = esp_timer_get_time()/1000;
            } else {
                click_times = 0;
                check_time = 0;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
        boot_button_.OnDoubleClick([this]() {
            click_times++;
            ESP_LOGI(TAG, "DoubleClick times %d", click_times);
            if(click_times==3) {
                click_times = 0;
                ResetWifiConfiguration();
            }
        });

        boot_button_.OnLongPress([this]() {
            if(click_times>=3) {
                ResetWifiConfiguration();
            } else {
                click_times = 0;
                check_time = 0;
            }
        });

        touch_button_.OnPressDown([this]() {
            click_times = 0;
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            click_times = 0;
            Application::GetInstance().StopListening();
        });

        volume_up_button_.OnClick([this]() {
            click_times = 0;
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
        });

        volume_up_button_.OnLongPress([this]() {
            click_times = 0;
            GetAudioCodec()->SetOutputVolume(100);
        });

        volume_down_button_.OnClick([this]() {
            click_times = 0;
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
        });

        volume_down_button_.OnLongPress([this]() {
            click_times = 0;
            GetAudioCodec()->SetOutputVolume(0);
        });
    }


    void InitializeGpio(gpio_num_t gpio_num_) {
        gpio_config_t config = {
            .pin_bit_mask = (1ULL << gpio_num_),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&config));
        gpio_set_level(gpio_num_, 1);
    }

public:
    DoitS3AiBox() :
        boot_button_(BOOT_BUTTON_GPIO),
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO){
        // 上拉io48 置高电平
        InitializeGpio(GPIO_NUM_48);
        InitializeButtons();
    }

    virtual Led* GetLed() override {
        static GpioLed led(BUILTIN_LED_GPIO, 1);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplexPdm audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }
};

DECLARE_BOARD(DoitS3AiBox);
