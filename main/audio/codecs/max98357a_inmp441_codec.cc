#include "max98357a_inmp441_codec.h"
#include <esp_log.h>
#include <driver/i2s_std.h>

#define TAG "Max98357aInmp441Codec"

Max98357aInmp441Codec::Max98357aInmp441Codec(int input_sample_rate, int output_sample_rate,
    gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
    gpio_num_t sd_mode_pin) : sd_mode_pin_(sd_mode_pin) {
    
    duplex_ = true;
    input_reference_ = false;
    input_channels_ = 1;
    output_channels_ = 1;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    input_gain_ = 0.0;

    // Configure SD_MODE pin for MAX98357A if provided
    if (sd_mode_pin_ != GPIO_NUM_NC) {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << sd_mode_pin_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
        gpio_set_level(sd_mode_pin_, 1); // Enable MAX98357A
    }

    CreateDuplexChannels(bclk, ws, dout, din);
    ESP_LOGI(TAG, "MAX98357A + INMP441 codec initialized");
}

Max98357aInmp441Codec::~Max98357aInmp441Codec() {
    if (tx_handle_) {
        i2s_channel_disable(tx_handle_);
        i2s_del_channel(tx_handle_);
    }
    if (rx_handle_) {
        i2s_channel_disable(rx_handle_);
        i2s_del_channel(rx_handle_);
    }
    
    // Shutdown MAX98357A
    if (sd_mode_pin_ != GPIO_NUM_NC) {
        gpio_set_level(sd_mode_pin_, 0);
    }
}

void Max98357aInmp441Codec::CreateDuplexChannels(gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
    // Create I2S channels for duplex operation
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    // Configure TX (output to MAX98357A)
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = bclk,
            .ws = ws,
            .dout = dout,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &tx_std_cfg));

    // Configure RX (input from INMP441)
    i2s_std_config_t rx_std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)input_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = bclk,
            .ws = ws,
            .dout = I2S_GPIO_UNUSED,
            .din = din,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &rx_std_cfg));

    ESP_LOGI(TAG, "Duplex I2S channels created");
}

void Max98357aInmp441Codec::SetOutputVolume(int volume) {
    // MAX98357A doesn't support software volume control
    // Volume is controlled by SD_MODE pin (3 levels) or external circuit
    ESP_LOGW(TAG, "MAX98357A doesn't support software volume control");
    AudioCodec::SetOutputVolume(volume);
}

void Max98357aInmp441Codec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    if (enable) {
        ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
        ESP_LOGI(TAG, "INMP441 input enabled");
    } else {
        ESP_ERROR_CHECK(i2s_channel_disable(rx_handle_));
        ESP_LOGI(TAG, "INMP441 input disabled");
    }
    AudioCodec::EnableInput(enable);
}

void Max98357aInmp441Codec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    if (enable) {
        ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
        if (sd_mode_pin_ != GPIO_NUM_NC) {
            gpio_set_level(sd_mode_pin_, 1);
        }
        ESP_LOGI(TAG, "MAX98357A output enabled");
    } else {
        ESP_ERROR_CHECK(i2s_channel_disable(tx_handle_));
        if (sd_mode_pin_ != GPIO_NUM_NC) {
            gpio_set_level(sd_mode_pin_, 0);
        }
        ESP_LOGI(TAG, "MAX98357A output disabled");
    }
    AudioCodec::EnableOutput(enable);
}

int Max98357aInmp441Codec::Read(int16_t* dest, int samples) {
    if (!input_enabled_) {
        return 0;
    }

    size_t bytes_read = 0;
    // INMP441 outputs 32-bit data, we need to convert to 16-bit
    int32_t temp_buffer[AUDIO_CODEC_DMA_FRAME_NUM];
    size_t bytes_to_read = samples * sizeof(int32_t);
    
    esp_err_t ret = i2s_channel_read(rx_handle_, temp_buffer, bytes_to_read, &bytes_read, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S read failed: %d", ret);
        return 0;
    }

    // Convert 32-bit to 16-bit (take upper 16 bits)
    int samples_read = bytes_read / sizeof(int32_t);
    for (int i = 0; i < samples_read; i++) {
        dest[i] = (int16_t)(temp_buffer[i] >> 16);
    }

    return samples_read;
}

int Max98357aInmp441Codec::Write(const int16_t* data, int samples) {
    if (!output_enabled_) {
        return 0;
    }

    size_t bytes_written = 0;
    size_t bytes_to_write = samples * sizeof(int16_t);
    
    esp_err_t ret = i2s_channel_write(tx_handle_, data, bytes_to_write, &bytes_written, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S write failed: %d", ret);
        return 0;
    }

    return bytes_written / sizeof(int16_t);
}
