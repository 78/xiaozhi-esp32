#include "a7682e_audio_codec.h"

#include "config.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#include <driver/i2s_pdm.h>
#include <esp_log.h>

static const char* TAG = "A7682eAudioCodec";

A7682eAudioCodec::A7682eAudioCodec(A7682eModem& modem) : modem_(modem) {
    duplex_ = false;
    input_reference_ = false;
    input_channels_ = 1;
    input_sample_rate_ = AUDIO_INPUT_SAMPLE_RATE;
    output_sample_rate_ = AUDIO_OUTPUT_SAMPLE_RATE;

#if SOC_I2S_SUPPORTS_PDM_RX
    i2s_chan_config_t rx_chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(XIAOZHI_I2S_PORT(0), I2S_ROLE_MASTER);
    rx_chan_cfg.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    rx_chan_cfg.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;

    esp_err_t ret = i2s_new_channel(&rx_chan_cfg, nullptr, &rx_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PDM RX channel: %s", esp_err_to_name(ret));
        return;
    }

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(input_sample_rate_)),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg =
            {
                .clk = T_DECK_PRO_A7682E_PDM_CLOCK,
                .din = T_DECK_PRO_A7682E_PDM_DATA,
                .invert_flags =
                    {
                        .clk_inv = false,
                    },
            },
    };
    ret = i2s_channel_init_pdm_rx_mode(rx_handle_, &pdm_rx_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize PDM RX channel: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "PDM RX initialized at %d Hz on CLK GPIO%d / DATA GPIO%d", input_sample_rate_,
             T_DECK_PRO_A7682E_PDM_CLOCK, T_DECK_PRO_A7682E_PDM_DATA);
#else
    ESP_LOGE(TAG, "ESP32 target does not support PDM RX");
#endif
}

A7682eAudioCodec::~A7682eAudioCodec() {
    if (rx_handle_ != nullptr && input_enabled_) {
        i2s_channel_disable(rx_handle_);
    }
}

void A7682eAudioCodec::SetOutputVolume(int volume) {
    volume = std::clamp(volume, 0, 100);
    AudioCodec::SetOutputVolume(volume);
    if (!modem_.SetOutputGain(output_volume_)) {
        ESP_LOGW(TAG, "Failed to queue A7682E output gain update");
    }
}

void A7682eAudioCodec::EnableInput(bool enable) {
    std::lock_guard<std::mutex> lock(data_if_mutex_);
    if (enable == input_enabled_) {
        return;
    }
    if (rx_handle_ == nullptr) {
        ESP_LOGE(TAG, "Cannot change PDM input state without an RX channel");
        return;
    }

    const esp_err_t ret = enable ? i2s_channel_enable(rx_handle_) : i2s_channel_disable(rx_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to %s PDM input: %s", enable ? "enable" : "disable",
                 esp_err_to_name(ret));
        return;
    }
    AudioCodec::EnableInput(enable);
}

void A7682eAudioCodec::EnableOutput(bool enable) {
    // A7682E drives the speaker through its own AT-controlled audio path.
    AudioCodec::EnableOutput(enable);
}

void A7682eAudioCodec::OutputData(std::vector<int16_t>& data) {
    // The modem cannot accept the server's Opus-decoded PCM stream.
    data.clear();
}

void A7682eAudioCodec::Start() {
    AudioCodec::Start();
    if (!modem_.Begin()) {
        ESP_LOGE(TAG, "A7682E modem initialization failed");
        return;
    }
    if (!modem_.SetOutputGain(output_volume_)) {
        ESP_LOGW(TAG, "Failed to queue initial A7682E output gain");
    }
    ESP_LOGI(TAG, "Queue startup diagnostic TTS");
    if (!modem_.SpeakText("你好我是小智")) {
        ESP_LOGW(TAG, "Failed to queue startup diagnostic TTS");
    }
    ESP_LOGI(TAG, "A7682E audio codec started with local text TTS");
}

int A7682eAudioCodec::Read(int16_t* dest, int samples) {
    if (rx_handle_ == nullptr || !input_enabled_ || dest == nullptr || samples <= 0) {
        return 0;
    }

    size_t bytes_read = 0;
    if (i2s_channel_read(rx_handle_, dest, samples * sizeof(int16_t), &bytes_read, portMAX_DELAY) !=
        ESP_OK) {
        ESP_LOGE(TAG, "PDM input read failed");
        return 0;
    }

    const int actual_samples = static_cast<int>(bytes_read / sizeof(int16_t));
    if (input_gain_ > 0.0f) {
        for (int i = 0; i < actual_samples; ++i) {
            const int32_t amplified = static_cast<int32_t>(dest[i] * input_gain_);
            dest[i] = static_cast<int16_t>(
                std::clamp(amplified, static_cast<int32_t>(std::numeric_limits<int16_t>::min()),
                           static_cast<int32_t>(std::numeric_limits<int16_t>::max())));
        }
    }
    return actual_samples;
}

int A7682eAudioCodec::Write(const int16_t* data, int samples) {
    (void)data;
    return samples;
}
