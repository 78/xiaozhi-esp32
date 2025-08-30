#ifndef _AI_VOX_AUDIO_CODEC_H
#define _AI_VOX_AUDIO_CODEC_H

#include "audio/audio_codec.h"

#include "esp_log.h"
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

class AIVoxAudioCodec : public AudioCodec {
  private:
    const audio_codec_data_if_t *data_if_ = nullptr;
    const audio_codec_ctrl_if_t *ctrl_if_ = nullptr;
    const audio_codec_if_t *codec_if_ = nullptr;
    const audio_codec_gpio_if_t *gpio_if_ = nullptr;

    esp_codec_dev_handle_t output_dev_ = nullptr;
    esp_codec_dev_handle_t input_dev_ = nullptr;

    // ref buffer used for aec
    std::vector<int16_t> ref_buffer_;
    int read_pos_ = 0;
    int write_pos_ = 0;

    void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
        assert(input_sample_rate_ == output_sample_rate_);

        i2s_chan_config_t chan_cfg = {
            .id = I2S_NUM_0,
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
            .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
            .auto_clear = true,
            .intr_priority = 0,
        };
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

        i2s_std_config_t std_cfg = {
            .clk_cfg = {.sample_rate_hz = (uint32_t)output_sample_rate_,
                        .clk_src = I2S_CLK_SRC_DEFAULT,
                        .mclk_multiple = I2S_MCLK_MULTIPLE_128},
            .slot_cfg = {.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
                         .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                         .slot_mode = I2S_SLOT_MODE_STEREO,
                         .slot_mask = I2S_STD_SLOT_BOTH,
                         .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
                         .ws_pol = false,
                         .bit_shift = true},
            .gpio_cfg = {.mclk = mclk,
                         .bclk = bclk,
                         .ws = ws,
                         .dout = dout,
                         .din = din,
                         .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}},
        };

        ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
        ESP_LOGI("AIVoxAudioCodec", "Duplex channels created");
    }

    virtual int Read(int16_t *dest, int samples) override {
        if (input_enabled_) {
            if (!input_reference_) {
                ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(input_dev_, (void *)dest, samples * sizeof(int16_t)));
            } else {
                int size = samples / input_channels_;
                std::vector<int16_t> data(size);
                // read mic data
                ESP_ERROR_CHECK_WITHOUT_ABORT(
                    esp_codec_dev_read(input_dev_, (void *)data.data(), data.size() * sizeof(int16_t)));
                int j = 0;
                int i = 0;
                while (i < samples) {
                    // mic data
                    dest[i++] = data[j++];
                    // ref data
                    dest[i++] = read_pos_ < write_pos_ ? ref_buffer_[read_pos_++] : 0;
                }

                if (read_pos_ == write_pos_) {
                    read_pos_ = write_pos_ = 0;
                }
            }
        }
        return samples;
    }

    virtual int Write(const int16_t *data, int samples) override {
        if (output_enabled_) {
            ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(output_dev_, (void *)data, samples * sizeof(int16_t)));
            if (input_reference_) { // 板子不支持硬件回采，采用缓存播放缓存来实现回声消除
                if (write_pos_ - read_pos_ + samples > ref_buffer_.size()) {
                    assert(ref_buffer_.size() >= samples);
                    // 写溢出，只保留最近的数据
                    read_pos_ = write_pos_ + samples - ref_buffer_.size();
                }
                if (read_pos_) {
                    if (write_pos_ != read_pos_) {
                        memmove(ref_buffer_.data(), ref_buffer_.data() + read_pos_,
                                (write_pos_ - read_pos_) * sizeof(int16_t));
                    }
                    write_pos_ -= read_pos_;
                    read_pos_ = 0;
                }
                memcpy(&ref_buffer_[write_pos_], data, samples * sizeof(int16_t));
                write_pos_ += samples;
            }
        }
        return samples;
    }

  public:
    AIVoxAudioCodec(void *i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
                    gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
                    uint8_t es8311_addr, bool input_reference = false) {
        duplex_ = true;                     // 是否双工
        input_reference_ = input_reference; // 是否使用参考输入，实现回声消除
        if (input_reference) {
            ref_buffer_.resize(960 * 2);
        }
        input_channels_ = 1 + input_reference_;
        input_sample_rate_ = input_sample_rate;
        output_sample_rate_ = output_sample_rate;
        CreateDuplexChannels(mclk, bclk, ws, dout, din);

        // Do initialize of related interface: data_if, ctrl_if and gpio_if
        audio_codec_i2s_cfg_t i2s_cfg = {
            .port = I2S_NUM_0,
            .rx_handle = rx_handle_,
            .tx_handle = tx_handle_,
        };
        data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
        assert(data_if_ != NULL);

        // Output
        audio_codec_i2c_cfg_t i2c_cfg = {
            .port = i2c_port,
            .addr = es8311_addr,
            .bus_handle = i2c_master_handle,
        };
        ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
        assert(ctrl_if_ != NULL);

        gpio_if_ = audio_codec_new_gpio();
        assert(gpio_if_ != NULL);

        es8311_codec_cfg_t es8311_cfg = {};
        es8311_cfg.ctrl_if = ctrl_if_;
        es8311_cfg.gpio_if = gpio_if_;
        es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
        es8311_cfg.pa_pin = GPIO_NUM_NC;
        es8311_cfg.use_mclk = true;
        es8311_cfg.hw_gain.pa_voltage = 5.0;
        es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
        es8311_cfg.pa_reverted = false;
        es8311_cfg.mclk_div = I2S_MCLK_MULTIPLE_128;
        codec_if_ = es8311_codec_new(&es8311_cfg);
        assert(codec_if_ != NULL);

        esp_codec_dev_cfg_t dev_cfg = {
            .dev_type = ESP_CODEC_DEV_TYPE_OUT,
            .codec_if = codec_if_,
            .data_if = data_if_,
        };
        output_dev_ = esp_codec_dev_new(&dev_cfg);
        assert(output_dev_ != NULL);
        dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
        input_dev_ = esp_codec_dev_new(&dev_cfg);
        assert(input_dev_ != NULL);
        esp_codec_set_disable_when_closed(output_dev_, false);
        esp_codec_set_disable_when_closed(input_dev_, false);
        ESP_LOGI("AIVoxAudioCodec", "AIVoxAudioCodec initialized");
    }

    virtual ~AIVoxAudioCodec() {
        ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
        esp_codec_dev_delete(output_dev_);
        ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
        esp_codec_dev_delete(input_dev_);

        audio_codec_delete_codec_if(codec_if_);
        audio_codec_delete_ctrl_if(ctrl_if_);
        audio_codec_delete_gpio_if(gpio_if_);
        audio_codec_delete_data_if(data_if_);
    }

    virtual void SetOutputVolume(int volume) override {
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
        AudioCodec::SetOutputVolume(volume);
    }

    virtual void EnableInput(bool enable) override {
        if (enable == input_enabled_) {
            return;
        }
        if (enable) {
            esp_codec_dev_sample_info_t fs = {
                .bits_per_sample = 16,
                .channel = 1,
                .channel_mask = 0,
                .sample_rate = (uint32_t)input_sample_rate_,
                .mclk_multiple = I2S_MCLK_MULTIPLE_128,
            };
            ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
            ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(input_dev_, AUDIO_CODEC_DEFAULT_MIC_GAIN));
        } else {
            ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
        }
        AudioCodec::EnableInput(enable);
    }

    virtual void EnableOutput(bool enable) override {
        if (enable == output_enabled_) {
            return;
        }
        if (enable) {
            // Play 16bit 1 channel
            esp_codec_dev_sample_info_t fs = {
                .bits_per_sample = 16,
                .channel = 1,
                .channel_mask = 0,
                .sample_rate = (uint32_t)output_sample_rate_,
                .mclk_multiple = I2S_MCLK_MULTIPLE_128,
            };
            ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs));
            ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_));
        } else {
            ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
        }
        AudioCodec::EnableOutput(enable);
    }
};

#endif // _AI_VOX_AUDIO_CODEC_H
