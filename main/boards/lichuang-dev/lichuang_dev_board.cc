#include "boards/wifi_board.h"
#include "audio_codecs/box_audio_codec.h"
#include "display/no_display.h"
#include "application.h"
#include "button.h"
#include "led.h"
#include "config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>

#define TAG "LichuangDevBoard"

class LichuangDevBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializePca9557() {
        i2c_device_config_t pca9557_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x19,
            .scl_speed_hz = 400000,
            .scl_wait_us = 0,
            .flags = {
                .disable_ack_check = 0,
            },
        };
        i2c_master_dev_handle_t pca9557_handle;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_, &pca9557_cfg, &pca9557_handle));
        assert(pca9557_handle != NULL);
        auto pca9557_set_register = [](i2c_master_dev_handle_t pca9557_handle, uint8_t data_addr, uint8_t data) {
            uint8_t data_[2] = {data_addr, data};
            ESP_ERROR_CHECK(i2c_master_transmit(pca9557_handle, data_, 2, 50));
        };
        pca9557_set_register(pca9557_handle, 0x03, 0xfd);
        pca9557_set_register(pca9557_handle, 0x01, 0x02);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            Application::GetInstance().ToggleChatState();
        });
    }

public:
    LichuangDevBoard() : boot_button_(BOOT_BUTTON_GPIO) {
    }

    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing LichuangDevBoard");
        InitializeI2c();
        InitializePca9557();
        InitializeButtons();
        WifiBoard::Initialize();
    }

    virtual Led* GetBuiltinLed() override {
        static Led led(GPIO_NUM_NC);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec* audio_codec = nullptr;
        if (audio_codec == nullptr) {
            audio_codec = new BoxAudioCodec(i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
                AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
            audio_codec->SetOutputVolume(AUDIO_DEFAULT_OUTPUT_VOLUME);
        }
        return audio_codec;
    }

    virtual Display* GetDisplay() override {
        static NoDisplay display;
        return &display;
    }
};

DECLARE_BOARD(LichuangDevBoard);
