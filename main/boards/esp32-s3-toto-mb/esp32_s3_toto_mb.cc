#include "wifi_board.h"
#include "audio_codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "led/gpio_led.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include "driver/uart.h"

#define TAG "Esp32S3TotoMbBoard"

class Esp32S3TotoMbBoard : public WifiBoard {
private:

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("MatrixScreen"));
    }


    void InitializeGpio(gpio_num_t gpio_num_) {
        gpio_reset_pin(gpio_num_);
        gpio_set_direction(gpio_num_, GPIO_MODE_INPUT);
        // 配置下拉，默认低电平
        gpio_pulldown_en(gpio_num_);
    }

public:
    Esp32S3TotoMbBoard() {
        // 下拉io8 置高电平
        InitializeGpio(GPIO_NUM_8);
        InitializeIot();
    }

    virtual AudioCodec* GetAudioCodec() override {
        #ifdef AUDIO_I2S_METHOD_SIMPLEX
            static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        #else
            static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        #endif
        return &audio_codec;
    }
};

DECLARE_BOARD(Esp32S3TotoMbBoard);
