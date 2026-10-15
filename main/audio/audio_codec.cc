#include "audio_codec.h"
#include "board.h"
#include "settings.h"

#include <esp_log.h>
#include <cstring>
#include <driver/adc.h>
#include <driver/dac.h>

#define TAG "AudioCodec"

// Pin Assignments
#define ADC_MIC_CHANNEL ADC1_CHANNEL_6 // GPIO34 (Change if using a different ADC pin)
#define DAC_SPEAKER_CHANNEL DAC_CHANNEL_1 // GPIO25 (DAC1)

AudioCodec::AudioCodec() {
}

AudioCodec::~AudioCodec() {
    if (output_enabled_) {
        dac_output_disable(DAC_SPEAKER_CHANNEL);
    }
}

void AudioCodec::Start() {
    Settings settings("audio", false);
    output_volume_ = settings.GetInt("output_volume", output_volume_);
    if (output_volume_ <= 0) {
        ESP_LOGW(TAG, "Output volume value (%d) is too small, setting to default (10)", output_volume_);
        output_volume_ = 10;
    }

    // Initialize ADC for MAX9814 Microphone
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_MIC_CHANNEL, ADC_ATTEN_DB_11); // Configured for ~0V - 3.3V range

    // Initialize DAC for LM386 Amplifier
    dac_output_enable(DAC_SPEAKER_CHANNEL);

    ESP_LOGI(TAG, "Audio codec started (ADC/DAC mode for MAX9814 and LM386)");
}

int AudioCodec::Read(int16_t* dest, int samples) {
    if (!input_enabled_) {
        memset(dest, 0, samples * sizeof(int16_t));
        return samples;
    }

    for (int i = 0; i < samples; i++) {
        // Read 12-bit raw value (0 to 4095) from ADC
        int raw_adc = adc1_get_raw(ADC_MIC_CHANNEL);

        // Remove 1.65V DC Offset (~2048 at 12-bit) and scale to 16-bit PCM (-32768 to 32767)
        int32_t pcm_sample = (raw_adc - 2048) * 16;

        // Apply software gain multiplier
        pcm_sample = (int32_t)(pcm_sample * input_gain_);

        // Clamp values to prevent 16-bit clipping
        if (pcm_sample > 32767) pcm_sample = 32767;
        if (pcm_sample < -32768) pcm_sample = -32768;

        dest[i] = (int16_t)pcm_sample;
    }

    return samples;
}

int AudioCodec::Write(const int16_t* src, int samples) {
    if (!output_enabled_) {
        return samples;
    }

    for (int i = 0; i < samples; i++) {
        // Scale sample according to software volume (0-100%)
        int32_t sample = (src[i] * output_volume_) / 100;

        // Convert 16-bit signed PCM (-32768 to 32767) to 8-bit unsigned DAC (0 to 255)
        uint8_t dac_val = (uint8_t)(((sample + 32768) >> 8) & 0xFF);

        // Output sample to internal hardware DAC (GPIO25)
        dac_output_voltage(DAC_SPEAKER_CHANNEL, dac_val);
    }

    return samples;
}

void AudioCodec::OutputData(std::vector<int16_t>& data) {
    Write(data.data(), data.size());
}

bool AudioCodec::InputData(std::vector<int16_t>& data) {
    int samples = Read(data.data(), data.size());
    return samples > 0;
}

void AudioCodec::SetOutputVolume(int volume) {
    output_volume_ = volume;
    ESP_LOGI(TAG, "Set output volume to %d", output_volume_);
    
    Settings settings("audio", true);
    settings.SetInt("output_volume", output_volume_);
}

void AudioCodec::SetInputGain(float gain) {
    input_gain_ = gain;
    ESP_LOGI(TAG, "Set input gain to %.1f", input_gain_);
}

void AudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    input_enabled_ = enable;
    ESP_LOGI(TAG, "Set input enable to %s", enable ? "true" : "false");
}

void AudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    output_enabled_ = enable;
    if (enable) {
        dac_output_enable(DAC_SPEAKER_CHANNEL);
    } else {
        dac_output_disable(DAC_SPEAKER_CHANNEL);
    }
    ESP_LOGI(TAG, "Set output enable to %s", enable ? "true" : "false");
}
