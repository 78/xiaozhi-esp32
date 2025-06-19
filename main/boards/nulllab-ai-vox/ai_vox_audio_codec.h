#pragma once
#include <driver/i2s_std.h>
#include <esp_log.h>

#include <cmath>
#include <cstring>

#include "audio_codecs/no_audio_codec.h"

class AIVoxAudioCodec : public NoAudioCodec {
  private:
    // ref buffer used for aec
    std::vector<int16_t> ref_buffer_;
    int read_pos_ = 0;
    int write_pos_ = 0;

    int Write(const int16_t *data, int samples) {
        if (output_enabled_) {
            std::vector<int32_t> buffer(samples);

            // output_volume_: 0-100
            // volume_factor_: 0-65536
            int32_t volume_factor = pow(double(output_volume_) / 100.0, 2) * 65536;
            for (int i = 0; i < samples; i++) {
                int64_t temp = int64_t(data[i]) * volume_factor; // 使用 int64_t 进行乘法运算
                if (temp > INT32_MAX) {
                    buffer[i] = INT32_MAX;
                } else if (temp < INT32_MIN) {
                    buffer[i] = INT32_MIN;
                } else {
                    buffer[i] = static_cast<int32_t>(temp);
                }
            }

            size_t bytes_written;
            ESP_ERROR_CHECK_WITHOUT_ABORT(
                i2s_channel_write(tx_handle_, buffer.data(), samples * sizeof(int32_t), &bytes_written, portMAX_DELAY));

            // 板子不支持硬件回采，采用缓存播放缓冲来实现回声消除
            if (input_reference_) {
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

    int Read(int16_t *dest, int samples) {
        if (input_enabled_) {
            int size = samples / input_channels_;
            std::vector<int32_t> buffer(size);

            size_t bytes_read;
            ESP_ERROR_CHECK_WITHOUT_ABORT(
                i2s_channel_read(rx_handle_, buffer.data(), size * sizeof(int32_t), &bytes_read, portMAX_DELAY));

            if (!input_reference_) {
                int i = 0;
                while (i < samples) {
                    // mic data
                    int32_t value = buffer[i] >> 14;
                    dest[i++] = (value > INT16_MAX) ? INT16_MAX : (value < -INT16_MAX) ? -INT16_MAX : (int16_t)value;
                }
            } else {
                int i = 0;
                int j = 0;
                while (i < samples) {
                    // mic data
                    int32_t value = buffer[j++] >> 14;
                    dest[i++] = (value > INT16_MAX) ? INT16_MAX : (value < -INT16_MAX) ? -INT16_MAX : (int16_t)value;

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

  public:
    AIVoxAudioCodec(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws,
                    gpio_num_t spk_dout, gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din,
                    bool input_reference = false) {
        duplex_ = false;                    // 是否双工
        input_reference_ = input_reference; // 是否使用参考输入，实现回声消除
        if (input_reference_) {
            ref_buffer_.resize(960 * 2);
        }
        input_channels_ = 1 + input_reference_; // 输入通道数："M" / "MR"
        input_sample_rate_ = input_sample_rate;
        output_sample_rate_ = output_sample_rate;

        // Create a new channel for speaker
        i2s_chan_config_t chan_cfg = {
            .id = (i2s_port_t)0,
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = 6,
            .dma_frame_num = 240,
            .auto_clear_after_cb = true,
            .auto_clear_before_cb = false,
            .intr_priority = 0,
        };
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, nullptr));

        i2s_std_config_t std_cfg = {
            .clk_cfg =
                {
                    .sample_rate_hz = (uint32_t)output_sample_rate_,
                    .clk_src = I2S_CLK_SRC_DEFAULT,
                    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
#ifdef I2S_HW_VERSION_2
                    .ext_clk_freq_hz = 0,
#endif

                },
            .slot_cfg = {.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
                         .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                         .slot_mode = I2S_SLOT_MODE_MONO,
                         .slot_mask = I2S_STD_SLOT_LEFT,
                         .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
                         .ws_pol = false,
                         .bit_shift = true,
#ifdef I2S_HW_VERSION_2
                         .left_align = true,
                         .big_endian = false,
                         .bit_order_lsb = false
#endif

            },
            .gpio_cfg = {.mclk = I2S_GPIO_UNUSED,
                         .bclk = spk_bclk,
                         .ws = spk_ws,
                         .dout = spk_dout,
                         .din = I2S_GPIO_UNUSED,
                         .invert_flags = {.mclk_inv = false, .bclk_inv = false, .ws_inv = false}}};
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));

        // Create a new channel for MIC
        chan_cfg.id = (i2s_port_t)1;
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &rx_handle_));
        std_cfg.clk_cfg.sample_rate_hz = (uint32_t)input_sample_rate_;
        std_cfg.gpio_cfg.bclk = mic_sck;
        std_cfg.gpio_cfg.ws = mic_ws;
        std_cfg.gpio_cfg.din = mic_din;
        std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
        ESP_LOGI("AIVoxAudioCodec", "Simplex channels created");
    }

    void SetOutputVolume(int volume) {
        output_volume_ = volume;
        AudioCodec::SetOutputVolume(volume);
    }
};
