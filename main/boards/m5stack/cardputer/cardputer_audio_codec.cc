#include "cardputer_audio_codec.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#define TAG "CardputerAudio"

// Keep the speaker on the shared pin this long after the last write, so the
// queued DMA buffers (6 x 240 frames, ~60 ms at 24 kHz) finish playing and
// back-to-back TTS frames don't thrash between modes.
static constexpr int64_t kSpeakerHoldUs = 300 * 1000;
static constexpr uint32_t kReadTimeoutMs = 200;

CardputerAudioCodec::CardputerAudioCodec(int input_sample_rate, int output_sample_rate,
    gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout,
    gpio_num_t mic_clk, gpio_num_t mic_din, float mic_gain)
    : spk_bclk_(spk_bclk), spk_ws_(spk_ws), spk_dout_(spk_dout),
      mic_clk_(mic_clk), mic_din_(mic_din) {
    duplex_ = false;
    input_reference_ = false;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    input_gain_ = mic_gain;
    ESP_LOGI(TAG, "Half-duplex codec ready (mic %d Hz, speaker %d Hz)",
             input_sample_rate_, output_sample_rate_);
}

CardputerAudioCodec::~CardputerAudioCodec() {
    std::lock_guard<std::mutex> lock(mutex_);
    ReleaseChannel();
}

void CardputerAudioCodec::ReleaseChannel() {
    if (chan_ != nullptr) {
        i2s_channel_disable(chan_);
        i2s_del_channel(chan_);
        chan_ = nullptr;
    }
    mode_ = Mode::kIdle;
}

void CardputerAudioCodec::StartSpeaker() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    chan_cfg.auto_clear_after_cb = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &chan_, nullptr));

    // Mono data with both slots enabled: the S3 duplicates each sample into
    // left and right, so the NS4168 plays it whichever channel it selects.
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)output_sample_rate_),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = spk_bclk_,
            .ws = spk_ws_,
            .dout = spk_dout_,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(chan_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(chan_));
    mode_ = Mode::kSpeaker;
}

void CardputerAudioCodec::StartMic() {
    // PDM RX only exists on I2S0 on the ESP32-S3.
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &chan_));

    i2s_pdm_rx_config_t pdm_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG((uint32_t)input_sample_rate_),
        // The data bit-width of PDM mode is fixed to 16
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = mic_clk_,
            .din = mic_din_,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(chan_, &pdm_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(chan_));
    mode_ = Mode::kMic;
}

void CardputerAudioCodec::SwitchMode(Mode mode) {
    if (mode == mode_) {
        return;
    }
    ReleaseChannel();
    if (mode == Mode::kSpeaker) {
        StartSpeaker();
    } else if (mode == Mode::kMic) {
        StartMic();
    }
    ESP_LOGD(TAG, "GPIO43 now owned by %s",
             mode_ == Mode::kSpeaker ? "speaker" : mode_ == Mode::kMic ? "mic" : "nobody");
}

bool CardputerAudioCodec::SpeakerBusy() const {
    if (writers_waiting_.load() > 0) {
        return true;
    }
    return esp_timer_get_time() - last_write_us_.load() < kSpeakerHoldUs;
}

int CardputerAudioCodec::Read(int16_t* dest, int samples) {
    if (SpeakerBusy()) {
        // Feed silence at real-time pace so the wake word / VAD pipeline keeps
        // running without spinning while the speaker owns the shared pin.
        memset(dest, 0, samples * sizeof(int16_t));
        vTaskDelay(pdMS_TO_TICKS(std::max(1, samples * 1000 / input_sample_rate_)));
        return samples;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!input_enabled_) {
        return 0;
    }
    SwitchMode(Mode::kMic);

    size_t bytes_read = 0;
    if (i2s_channel_read(chan_, dest, samples * sizeof(int16_t), &bytes_read,
                         kReadTimeoutMs) != ESP_OK) {
        return 0;
    }

    samples = bytes_read / sizeof(int16_t);
    if (input_gain_ > 0) {
        for (int i = 0; i < samples; i++) {
            int32_t amplified = (int32_t)(dest[i] * input_gain_);
            dest[i] = (int16_t)std::clamp<int32_t>(amplified, -INT16_MAX, INT16_MAX);
        }
    }
    return samples;
}

int CardputerAudioCodec::Write(const int16_t* data, int samples) {
    writers_waiting_++;
    std::lock_guard<std::mutex> lock(mutex_);
    writers_waiting_--;
    if (!output_enabled_) {
        return 0;
    }
    SwitchMode(Mode::kSpeaker);

    // output_volume_: 0-100, applied on a square curve like NoAudioCodec
    int32_t volume_factor = pow(double(output_volume_) / 100.0, 2) * 65536;
    std::vector<int16_t> buffer(samples);
    for (int i = 0; i < samples; i++) {
        int32_t scaled = (int32_t)(((int64_t)data[i] * volume_factor) >> 16);
        buffer[i] = (int16_t)std::clamp<int32_t>(scaled, INT16_MIN, INT16_MAX);
    }

    size_t bytes_written = 0;
    ESP_ERROR_CHECK(i2s_channel_write(chan_, buffer.data(), samples * sizeof(int16_t),
                                      &bytes_written, portMAX_DELAY));
    last_write_us_ = esp_timer_get_time();
    return bytes_written / sizeof(int16_t);
}

void CardputerAudioCodec::EnableInput(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (enable == input_enabled_) {
        return;
    }
    // The mic channel is opened lazily on the first Read(); only release it here.
    if (!enable && mode_ == Mode::kMic) {
        ReleaseChannel();
    }
    AudioCodec::EnableInput(enable);
}

void CardputerAudioCodec::EnableOutput(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (enable == output_enabled_) {
        return;
    }
    // The speaker channel is opened lazily on the first Write(); only release it here.
    if (!enable && mode_ == Mode::kSpeaker) {
        ReleaseChannel();
    }
    AudioCodec::EnableOutput(enable);
}
