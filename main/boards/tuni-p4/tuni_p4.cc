// main/boards/tuni-p4/tuni_p4.cc
#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "application.h"
#include "display/display.h"   // NoDisplay
#include "button.h"
#include "config.h"
#include "ssid_manager.h"

#include <esp_log.h>
#include <driver/i2c_master.h>

#define TAG "TuniP4"

class TuniP4 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    Display* display_ = nullptr;

    void InitializeCodecI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = { .enable_internal_pullup = 1 },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeButtons() {
        // No press-to-talk. Short click only enters Wi-Fi config while still starting.
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
            }
        });

        // Long-press (any state) forgets the stored network(s) and re-enters BLE
        // pairing - a full re-provision, not just a temporary config-mode visit.
        // Scoped to the "wifi" NVS namespace only (SsidManager::Clear()), so the
        // device identity keypair / JWT state survive - this is not a factory
        // reset. EnterWifiConfigMode() already handles the graceful teardown
        // when already connected (closes the audio channel, stops station,
        // then starts BLE advertising).
        boot_button_.OnLongPress([this]() {
            ESP_LOGW(TAG, "BOOT long-press: forgetting WiFi credentials");
            SsidManager::GetInstance().Clear();
            EnterWifiConfigMode();
        });
    }

public:
    TuniP4() : boot_button_(BOOT_BUTTON_GPIO, false, 5000) {
        InitializeCodecI2c();
        InitializeButtons();
        display_ = new NoDisplay();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, I2C_NUM_1, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        // Far-field open mic: bump ES8311 ADC PGA from the 30 dB default to 36 dB
        // (+6 dB ≈ 2x) for louder capture (helps STT + pronunciation scoring).
        // PGA steps are 0/6/.../42 dB; 36 keeps headroom so loud speech doesn't clip.
        // Applied on the first EnableInput (UpdateDeviceState), set here before that.
        audio_codec.SetInputGain(36.0f);
        // Speaker too quiet to hear the TTS clearly: force output volume to max
        // (default is 70/100). Overrides any stored NVS value on boot so it's
        // reliably loud; applied on the first EnableOutput.
        audio_codec.SetOutputVolume(100);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }
};

DECLARE_BOARD(TuniP4);
