#include "BoxAudioDevice.h"
#include "Board.h"

#include <esp_log.h>
#include <cassert>

static const char* TAG = "BoxAudioDevice";

BoxAudioDevice::BoxAudioDevice() {
}

BoxAudioDevice::~BoxAudioDevice() {
    ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    esp_codec_dev_delete(output_dev_);
    ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    esp_codec_dev_delete(input_dev_);

    audio_codec_delete_codec_if(in_codec_if_);
    audio_codec_delete_ctrl_if(in_ctrl_if_);
    audio_codec_delete_codec_if(out_codec_if_);
    audio_codec_delete_ctrl_if(out_ctrl_if_);
    audio_codec_delete_gpio_if(gpio_if_);
    audio_codec_delete_data_if(data_if_);

    ESP_ERROR_CHECK(i2c_del_master_bus(i2c_master_handle_));
}

void BoxAudioDevice::Initialize() {
    duplex_ = true; // 是否双工
    input_reference_ = AUDIO_INPUT_REFERENCE; // 是否使用参考输入，实现回声消除
    input_channels_ = input_reference_ ? 2 : 1; // 输入通道数

    // Initialize I2C peripheral
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = (gpio_num_t)AUDIO_CODEC_I2C_SDA_PIN,
        .scl_io_num = (gpio_num_t)AUDIO_CODEC_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 0,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_master_handle_));

    CreateDuplexChannels();

#ifdef AUDIO_CODEC_USE_PCA9557
    // Initialize PCA9557
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
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_master_handle_, &pca9557_cfg, &pca9557_handle));
    assert(pca9557_handle != NULL);
    auto pca9557_set_register = [](i2c_master_dev_handle_t pca9557_handle, uint8_t data_addr, uint8_t data) {
        uint8_t data_[2] = {data_addr, data};
        ESP_ERROR_CHECK(i2c_master_transmit(pca9557_handle, data_, 2, 50));
    };
    pca9557_set_register(pca9557_handle, 0x03, 0xfd);
    pca9557_set_register(pca9557_handle, 0x01, 0x02);
#endif

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
        .port = I2C_NUM_1,
        .addr = AUDIO_CODEC_ES8311_ADDR,
        .bus_handle = i2c_master_handle_,
    };
    out_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(out_ctrl_if_ != NULL);

    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);

    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = out_ctrl_if_;
    es8311_cfg.gpio_if = gpio_if_;
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
    es8311_cfg.pa_pin = AUDIO_CODEC_PA_PIN;
    es8311_cfg.use_mclk = true;
    es8311_cfg.hw_gain.pa_voltage = 5.0;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
    out_codec_if_ = es8311_codec_new(&es8311_cfg);
    assert(out_codec_if_ != NULL);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = out_codec_if_,
        .data_if = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(output_dev_ != NULL);

    // Input
    i2c_cfg.addr = AUDIO_CODEC_ES7210_ADDR;
    in_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(in_ctrl_if_ != NULL);

    es7210_codec_cfg_t es7210_cfg = {};
    es7210_cfg.ctrl_if = in_ctrl_if_;
    es7210_cfg.mic_selected = ES7120_SEL_MIC1 | ES7120_SEL_MIC2 | ES7120_SEL_MIC3 | ES7120_SEL_MIC4;
    in_codec_if_ = es7210_codec_new(&es7210_cfg);
    assert(in_codec_if_ != NULL);

    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    dev_cfg.codec_if = in_codec_if_;
    input_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(input_dev_ != NULL);

    ESP_LOGI(TAG, "BoxAudioDevice initialized");
}

void BoxAudioDevice::CreateDuplexChannels() {
    assert(input_sample_rate_ == output_sample_rate_);

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false
        },
        .gpio_cfg = {
            .mclk = (gpio_num_t)AUDIO_I2S_GPIO_MCLK,
            .bclk = (gpio_num_t)AUDIO_I2S_GPIO_BCLK,
            .ws = (gpio_num_t)AUDIO_I2S_GPIO_LRCK,
            .dout = (gpio_num_t)AUDIO_I2S_GPIO_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)input_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bclk_div = 8,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = i2s_tdm_slot_mask_t(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3),
            .ws_width = I2S_TDM_AUTO_WS_WIDTH,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
            .skip_mask = false,
            .total_slot = I2S_TDM_AUTO_SLOT_NUM
        },
        .gpio_cfg = {
            .mclk = (gpio_num_t)AUDIO_I2S_GPIO_MCLK,
            .bclk = (gpio_num_t)AUDIO_I2S_GPIO_BCLK,
            .ws = (gpio_num_t)AUDIO_I2S_GPIO_LRCK,
            .dout = I2S_GPIO_UNUSED,
            .din = (gpio_num_t)AUDIO_I2S_GPIO_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(rx_handle_, &tdm_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    ESP_LOGI(TAG, "Duplex channels created");
}

int BoxAudioDevice::Read(int16_t *buffer, int samples) {
    if (input_enabled_) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(input_dev_, (void*)buffer, samples * sizeof(int16_t)));
    }
    return samples;
}

int BoxAudioDevice::Write(const int16_t *buffer, int samples) {
    if (output_enabled_) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(output_dev_, (void*)buffer, samples * sizeof(int16_t)));
    }
    return samples;
}

void BoxAudioDevice::SetOutputVolume(int volume) {
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
    AudioDevice::SetOutputVolume(volume);
}

void BoxAudioDevice::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    if (enable) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 4,
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = 0,
        };
        if (input_reference_) {
            fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);
        }
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_in_channel_gain(input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), 30.0));
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    }
    AudioDevice::EnableInput(enable);
}

void BoxAudioDevice::EnableOutput(bool enable) {
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
            .mclk_multiple = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_));
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    }
    AudioDevice::EnableOutput(enable);
}
