#include "audio_codec.h"
#include "board.h"
#include "settings.h"

#include <esp_log.h>
#include <cstring>
#include <driver/i2s_common.h>

#define TAG "AudioCodec"

AudioCodec::AudioCodec() {
}

AudioCodec::~AudioCodec() {
}

void AudioCodec::OutputData(std::vector<int16_t>& data) {
    Write(data.data(), data.size());
}

bool AudioCodec::InputData(std::vector<int16_t>& data) {
    int samples = Read(data.data(), data.size());
    if (samples > 0) {
        return true;
    }
    return false;
}

void AudioCodec::Start() {
    Settings settings("audio", false);
    output_volume_ = settings.GetInt("output_volume", output_volume_);
    if (output_volume_ <= 0) {
        ESP_LOGW(TAG, "Output volume value (%d) is too small, setting to default (10)", output_volume_);
        output_volume_ = 10;
    }

    // 保存原始输出采样率
    if (original_output_sample_rate_ == 0) {
        original_output_sample_rate_ = output_sample_rate_;
        ESP_LOGI(TAG, "Saved original output sample rate: %d Hz", original_output_sample_rate_);
    }

    if (tx_handle_ != nullptr) {
        ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    }

    if (rx_handle_ != nullptr) {
        ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    }

    EnableInput(true);
    EnableOutput(true);
    ESP_LOGI(TAG, "Audio codec started");
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
    ESP_LOGI(TAG, "Set output enable to %s", enable ? "true" : "false");
}

bool AudioCodec::SetOutputSampleRate(int sample_rate) {
    // 特殊处理：如果传入 -1，表示重置到原始采样率
    if (sample_rate == -1) {
        if (original_output_sample_rate_ > 0) {
            sample_rate = original_output_sample_rate_;
            ESP_LOGI(TAG, "Resetting to original output sample rate: %d Hz", sample_rate);
        } else {
            ESP_LOGW(TAG, "Original sample rate not available, cannot reset");
            return false;
        }
    }
    
    if (sample_rate <= 0 || sample_rate > 192000) {
        ESP_LOGE(TAG, "Invalid sample rate: %d", sample_rate);
        return false;
    }
    
    if (output_sample_rate_ == sample_rate) {
        ESP_LOGI(TAG, "Sample rate already set to %d Hz", sample_rate);
        return true;
    }
    
    if (tx_handle_ == nullptr) {
        ESP_LOGW(TAG, "TX handle is null, only updating sample rate variable");
        output_sample_rate_ = sample_rate;
        return true;
    }
    
    ESP_LOGI(TAG, "Changing output sample rate from %d to %d Hz", output_sample_rate_, sample_rate);
    
    // 先尝试禁用 I2S 通道（如果已启用的话）
    //bool was_enabled = false;
    esp_err_t disable_ret = i2s_channel_disable(tx_handle_);
    if (disable_ret == ESP_OK) {
        //was_enabled = true;
        ESP_LOGI(TAG, "Disabled I2S TX channel for reconfiguration");
    } else if (disable_ret == ESP_ERR_INVALID_STATE) {
        // 通道可能已经是禁用状态，这是正常的
        ESP_LOGI(TAG, "I2S TX channel was already disabled");
    } else {
        ESP_LOGW(TAG, "Failed to disable I2S TX channel: %s", esp_err_to_name(disable_ret));
    }
    
    // 重新配置 I2S 时钟
    i2s_std_clk_config_t clk_cfg = {
        .sample_rate_hz = (uint32_t)sample_rate,
        .clk_src = I2S_CLK_SRC_DEFAULT,
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
#ifdef I2S_HW_VERSION_2
        .ext_clk_freq_hz = 0,
#endif
    };
    
    esp_err_t ret = i2s_channel_reconfig_std_clock(tx_handle_, &clk_cfg);
    
    // 重新启用通道（无论之前是什么状态，现在都需要启用以便播放音频）
    esp_err_t enable_ret = i2s_channel_enable(tx_handle_);
    if (enable_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S TX channel: %s", esp_err_to_name(enable_ret));
    } else {
        ESP_LOGI(TAG, "Enabled I2S TX channel");
    }
    
    if (ret == ESP_OK) {
        output_sample_rate_ = sample_rate;
        ESP_LOGI(TAG, "Successfully changed output sample rate to %d Hz", sample_rate);
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to change sample rate to %d Hz: %s", sample_rate, esp_err_to_name(ret));
        return false;
    }
}