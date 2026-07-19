#include <driver/i2c_master.h>
#include "codecs/box_audio_codec.h"
#include "config.h"
#include "rgb_matrix_display.h"
#include "settings.h"
#include "wifi_board.h"

class CustomBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    CustomMatrixDisplay* display_;

    void init_I2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void init_HUB75() {
        display_ = new CustomMatrixDisplay(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    }

    void init_Audio() {
        if (AUDIO_CODEC_PA_PIN != GPIO_NUM_NC) {
            gpio_config_t io_conf = {};
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = (1ULL << AUDIO_CODEC_PA_PIN);
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            ESP_ERROR_CHECK(gpio_config(&io_conf));
            gpio_set_level(AUDIO_CODEC_PA_PIN, 1);
        }

        Settings settings("audio", false);
        const int stored_volume = settings.GetInt("output_volume", 70);
        if (stored_volume > 0) {
            return;
        }

        Settings persist("audio", true);
        persist.SetInt("output_volume", 70);
    }

public:
    CustomBoard() {
        init_I2c();
        init_Audio();
        init_HUB75();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        class MatrixBacklight : public Backlight {
        public:
            explicit MatrixBacklight(CustomMatrixDisplay* display) : display_(display) {}

        protected:
            CustomMatrixDisplay* display_;

            void SetBrightnessImpl(uint8_t brightness) override {
                if (display_ == nullptr) {
                    return;
                }
                display_->SetBrightness(brightness);
            }
        };

        static MatrixBacklight backlight(display_);
        return &backlight;
    }
};

DECLARE_BOARD(CustomBoard);
