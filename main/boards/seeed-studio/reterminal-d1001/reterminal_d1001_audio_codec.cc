#include "reterminal_d1001_audio_codec.h"

#include "config.h"

#include <driver/gpio.h>
#include <driver/i2s_tdm.h>
#include <esp_log.h>

#define TAG "D1001AudioCodec"

ReTerminalD1001AudioCodec::ReTerminalD1001AudioCodec(i2c_master_bus_handle_t i2c_bus,
                                                     int input_sample_rate,
                                                     int output_sample_rate) {
    duplex_ = true;
    input_gain_ = AUDIO_MIC_GAIN;
    // TDM carries 4 slots: 0/2 = mics, 1 = DAC playback loopback (electrical,
    // pre-PA), 3 = empty. Slot 0 + slot 1 give the AFE an "MR" pair, enabling
    // on-device AEC and full-duplex (realtime) listening.
    input_reference_ = true;
    input_channels_ = 2;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;

    CreateChannels();

    // Playback data interface (I2S0 TX only).
    audio_codec_i2s_cfg_t tx_i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = nullptr,
        .tx_handle = tx_handle_,
    };
    tx_data_if_ = audio_codec_new_i2s_data(&tx_i2s_cfg);
    assert(tx_data_if_ != nullptr);

    // Capture data interface (I2S1 RX only).
    audio_codec_i2s_cfg_t rx_i2s_cfg = {
        .port = I2S_NUM_1,
        .rx_handle = rx_handle_,
        .tx_handle = nullptr,
    };
    rx_data_if_ = audio_codec_new_i2s_data(&rx_i2s_cfg);
    assert(rx_data_if_ != nullptr);

    // ES8311 playback codec (DAC mode only).
    // esp_codec_dev expects the 8-bit I2C address form (addr << 1) and
    // shifts it back internally; config.h keeps the 7-bit hardware facts.
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = (i2c_port_t)AUDIO_I2C_PORT,
        .addr = (uint16_t)(ES8311_I2C_ADDRESS << 1),
        .bus_handle = i2c_bus,
    };
    out_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(out_ctrl_if_ != nullptr);

    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != nullptr);

    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = out_ctrl_if_;
    es8311_cfg.gpio_if = gpio_if_;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    // The PA is an NS4150B gated by PCA9535 bit 11, not a GPIO on the P4.
    es8311_cfg.pa_pin = GPIO_NUM_NC;
    es8311_cfg.use_mclk = true;
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
    out_codec_if_ = es8311_codec_new(&es8311_cfg);
    assert(out_codec_if_ != nullptr);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = out_codec_if_,
        .data_if = tx_data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(output_dev_ != nullptr);

    // The stock volume curve tops out at 0 dB, which is tuned for full-scale
    // content (the factory UI sounds). Cloud TTS masters 10-15 dB lower, so
    // at the same percentage the replies are barely audible on this speaker.
    // Shift the whole curve up; the ES8311 digital volume reaches +32 dB, so
    // +6 dB at 100% leaves headroom and normal speech cannot clip.
    esp_codec_dev_vol_map_t vol_map[2];
    vol_map[0].vol = 0;
    vol_map[0].db_value = -45.0;
    vol_map[1].vol = 100;
    vol_map[1].db_value = 6.0;
    esp_codec_dev_vol_curve_t vol_curve;
    vol_curve.count = 2;
    vol_curve.vol_map = vol_map;
    esp_codec_dev_set_vol_curve(output_dev_, &vol_curve);

    // ES7210 capture codec (same 8-bit address convention).
    i2c_cfg.addr = (uint16_t)(ES7210_I2C_ADDRESS << 1);
    in_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(in_ctrl_if_ != nullptr);

    es7210_codec_cfg_t es7210_cfg = {};
    es7210_cfg.ctrl_if = in_ctrl_if_;
    es7210_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4;
    in_codec_if_ = es7210_codec_new(&es7210_cfg);
    assert(in_codec_if_ != nullptr);

    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    dev_cfg.codec_if = in_codec_if_;
    dev_cfg.data_if = rx_data_if_;
    input_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(input_dev_ != nullptr);

    ESP_LOGI(TAG, "D1001 audio codec initialized (TX I2S0 std, RX I2S1 TDM4)");
}

ReTerminalD1001AudioCodec::~ReTerminalD1001AudioCodec() {
    if (output_dev_ != nullptr) {
        esp_codec_dev_close(output_dev_);
        esp_codec_dev_delete(output_dev_);
    }
    if (input_dev_ != nullptr) {
        esp_codec_dev_close(input_dev_);
        esp_codec_dev_delete(input_dev_);
    }

    audio_codec_delete_codec_if(in_codec_if_);
    audio_codec_delete_ctrl_if(in_ctrl_if_);
    audio_codec_delete_codec_if(out_codec_if_);
    audio_codec_delete_ctrl_if(out_ctrl_if_);
    audio_codec_delete_gpio_if(gpio_if_);
    audio_codec_delete_data_if(rx_data_if_);
    audio_codec_delete_data_if(tx_data_if_);
}

void ReTerminalD1001AudioCodec::CreateChannels() {
    // Playback: I2S0 TX, standard mode, 16-bit mono.
    i2s_chan_config_t tx_chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_handle_, nullptr));

    // Reduce signal overshoot, as done by the Seeed BSP.
    gpio_set_drive_capability(AUDIO_TX_MCLK_PIN, GPIO_DRIVE_CAP_1);
    gpio_set_drive_capability(AUDIO_TX_BCLK_PIN, GPIO_DRIVE_CAP_1);
    gpio_set_drive_capability(AUDIO_TX_WS_PIN, GPIO_DRIVE_CAP_1);
    gpio_set_drive_capability(AUDIO_TX_DOUT_PIN, GPIO_DRIVE_CAP_1);

    i2s_std_config_t std_cfg = {
        .clk_cfg =
            {
                .sample_rate_hz = (uint32_t)output_sample_rate_,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .ext_clk_freq_hz = 0,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
        .slot_cfg =
            {
                .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = I2S_STD_SLOT_BOTH,
                .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
                .ws_pol = false,
                .bit_shift = true,
                .left_align = true,
                .big_endian = false,
                .bit_order_lsb = false,
            },
        .gpio_cfg =
            {
                .mclk = AUDIO_TX_MCLK_PIN,
                .bclk = AUDIO_TX_BCLK_PIN,
                .ws = AUDIO_TX_WS_PIN,
                .dout = AUDIO_TX_DOUT_PIN,
                .din = I2S_GPIO_UNUSED,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));

    // Capture: I2S1 RX, TDM 4-slot, as configured by the Seeed BSP.
    i2s_chan_config_t rx_chan_cfg = {
        .id = I2S_NUM_1,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, nullptr, &rx_handle_));

    gpio_set_drive_capability(AUDIO_RX_MCLK_PIN, GPIO_DRIVE_CAP_1);
    gpio_set_drive_capability(AUDIO_RX_BCLK_PIN, GPIO_DRIVE_CAP_1);
    gpio_set_drive_capability(AUDIO_RX_LRCK_PIN, GPIO_DRIVE_CAP_1);
    gpio_set_drive_capability(AUDIO_RX_DIN_PIN, GPIO_DRIVE_CAP_1);

    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg =
            {
                .sample_rate_hz = (uint32_t)input_sample_rate_,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .ext_clk_freq_hz = 0,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
                .bclk_div = 8,
            },
        .slot_cfg =
            {
                .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = i2s_tdm_slot_mask_t(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 |
                                                 I2S_TDM_SLOT3),
                .ws_width = I2S_TDM_AUTO_WS_WIDTH,
                .ws_pol = false,
                .bit_shift = true,
                .left_align = false,
                .big_endian = false,
                .bit_order_lsb = false,
                .skip_mask = false,
                .total_slot = I2S_TDM_AUTO_SLOT_NUM,
            },
        .gpio_cfg =
            {
                .mclk = AUDIO_RX_MCLK_PIN,
                .bclk = AUDIO_RX_BCLK_PIN,
                .ws = AUDIO_RX_LRCK_PIN,
                .dout = I2S_GPIO_UNUSED,
                .din = AUDIO_RX_DIN_PIN,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(rx_handle_, &tdm_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    ESP_LOGI(TAG, "TX/RX channels created");
}

void ReTerminalD1001AudioCodec::PrefillSilence() {
    // Queue silent frames before enabling the power amplifier so the DAC
    // output is stable zeros at PA power-on (avoids the startup pop). Two
    // DMA frames are enough to mask the transient without stalling here.
    constexpr int kSilenceSamples = AUDIO_CODEC_DMA_FRAME_NUM * 2;
    int16_t silence[kSilenceSamples] = {0};
    ESP_ERROR_CHECK(esp_codec_dev_write(output_dev_, silence, sizeof(silence)));
}

void ReTerminalD1001AudioCodec::SetOutputVolume(int volume) {
    std::lock_guard<std::mutex> lock(data_if_mutex_);
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
    AudioCodec::SetOutputVolume(volume);
}

void ReTerminalD1001AudioCodec::EnableInput(bool enable) {
    std::lock_guard<std::mutex> lock(data_if_mutex_);
    if (enable == input_enabled_) {
        return;
    }
    if (enable) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 4,
            .channel_mask = 0,
            .sample_rate = (uint32_t)input_sample_rate_,
            .mclk_multiple = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_in_channel_gain(
            input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), input_gain_));
        ESP_ERROR_CHECK(esp_codec_dev_set_in_channel_gain(
            input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1), input_gain_));
        ESP_ERROR_CHECK(esp_codec_dev_set_in_channel_gain(
            input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(2), 0));
        ESP_ERROR_CHECK(esp_codec_dev_set_in_channel_gain(
            input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(3), 0));
        rx_buf_.resize(AUDIO_CODEC_DMA_FRAME_NUM * 4);
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
        rx_buf_.clear();
        rx_buf_.shrink_to_fit();
    }
    AudioCodec::EnableInput(enable);
}

void ReTerminalD1001AudioCodec::EnableOutput(bool enable) {
    std::lock_guard<std::mutex> lock(data_if_mutex_);
    if (enable == output_enabled_) {
        return;
    }
    if (enable) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_));
        PrefillSilence();
        if (pa_callback_) {
            pa_callback_(true);
        }
    } else {
        // Power the amplifier down first to avoid the pop on close.
        if (pa_callback_) {
            pa_callback_(false);
        }
        ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    }
    AudioCodec::EnableOutput(enable);
}

int ReTerminalD1001AudioCodec::Read(int16_t* dest, int samples) {
    if (!input_enabled_ || input_channels_ <= 0) {
        return 0;
    }
    int frames = samples / input_channels_;
    if (frames <= 0) {
        return 0;
    }

    size_t tdm_samples = (size_t)frames * 4;
    if (rx_buf_.size() < tdm_samples) {
        rx_buf_.resize(tdm_samples);
    }

    int ret = esp_codec_dev_read(input_dev_, rx_buf_.data(), tdm_samples * sizeof(int16_t));
    if (ret != ESP_CODEC_DEV_OK) {
        return 0;
    }

    if (input_reference_) {
        // TDM: slot 0 is Mic1, slot 1 is DAC Reference loopback
        for (int i = 0; i < frames; ++i) {
            dest[2 * i + 0] = rx_buf_[4 * i + 0];
            dest[2 * i + 1] = rx_buf_[4 * i + 1];
        }
    } else {
        for (int i = 0; i < frames; ++i) {
            dest[i] = rx_buf_[4 * i + 0];
        }
    }
    return samples;
}

int ReTerminalD1001AudioCodec::Write(const int16_t* data, int samples) {
    if (output_enabled_) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(
            esp_codec_dev_write(output_dev_, (void*)data, samples * sizeof(int16_t)));
    }
    return samples;
}
