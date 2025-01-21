#include "t_circle_s3_audio_codec.h"

#include <esp_log.h>
#include <driver/i2c.h>
#include <driver/i2c_master.h>
#include <driver/i2s_tdm.h>
#include <driver/i2s.h>

static const char TAG[] = "T_Circle_S3_Audio_Codec";

T_Circle_S3_Audio_Codec::T_Circle_S3_Audio_Codec(int input_sample_rate, int output_sample_rate,
                                                 gpio_num_t mic_bclk, gpio_num_t mic_ws, gpio_num_t mic_data,
                                                 gpio_num_t spkr_bclk, gpio_num_t spkr_lrclk, gpio_num_t spkr_data,
                                                 bool input_reference)
{
    duplex_ = true;                             // 是否双工
    input_reference_ = input_reference;         // 是否使用参考输入，实现回声消除
    input_channels_ = input_reference_ ? 2 : 1; // 输入通道数
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;

    create_voice_hardware(mic_bclk, mic_ws, mic_data, spkr_bclk, spkr_lrclk, spkr_data);

    gpio_config_t config;
    config.pin_bit_mask = BIT64(GPIO_NUM_45);
    config.mode = GPIO_MODE_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    config.intr_type = GPIO_INTR_DISABLE;
#if SOC_GPIO_SUPPORT_PIN_HYS_FILTER
    config.hys_ctrl_mode = GPIO_HYS_SOFT_ENABLE;
#endif
    gpio_config(&config);

    gpio_set_level(GPIO_NUM_45, 0);

    // // Do initialize of related interface: data_if, ctrl_if and gpio_if
    // audio_codec_i2s_cfg_t i2s_cfg = {
    //     .port = I2S_NUM_0,
    //     .rx_handle = rx_handle_,
    //     .tx_handle = tx_handle_,
    // };
    // data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    // assert(data_if_ != NULL);

    // // Output
    // audio_codec_i2c_cfg_t i2c_cfg = {
    //     .port = i2c_port,
    //     .addr = es8311_addr,
    //     .bus_handle = i2c_master_handle,
    // };
    // ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    // assert(ctrl_if_ != NULL);

    // gpio_if_ = audio_codec_new_gpio();
    // assert(gpio_if_ != NULL);

    // es8311_codec_cfg_t es8311_cfg = {};
    // es8311_cfg.ctrl_if = ctrl_if_;
    // es8311_cfg.gpio_if = gpio_if_;
    // es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    // es8311_cfg.pa_pin = pa_pin;
    // es8311_cfg.use_mclk = use_mclk;
    // es8311_cfg.hw_gain.pa_voltage = 5.0;
    // es8311_cfg.hw_gain.codec_dac_voltage = 3.3;
    // codec_if_ = es8311_codec_new(&es8311_cfg);
    // assert(codec_if_ != NULL);

    // esp_codec_dev_cfg_t dev_cfg = {
    //     .dev_type = ESP_CODEC_DEV_TYPE_OUT,
    //     .codec_if = codec_if_,
    //     .data_if = data_if_,
    // };
    // output_dev_ = esp_codec_dev_new(&dev_cfg);
    // assert(output_dev_ != NULL);
    // dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    // input_dev_ = esp_codec_dev_new(&dev_cfg);
    // assert(input_dev_ != NULL);

    ESP_LOGI(TAG, "T_Circle_S3_Audio_Codec initialized");
}

T_Circle_S3_Audio_Codec::~T_Circle_S3_Audio_Codec()
{
    // ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    // esp_codec_dev_delete(output_dev_);
    // ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    // esp_codec_dev_delete(input_dev_);

    audio_codec_delete_codec_if(in_codec_if_);
    audio_codec_delete_ctrl_if(in_ctrl_if_);
    audio_codec_delete_codec_if(out_codec_if_);
    audio_codec_delete_ctrl_if(out_ctrl_if_);
    audio_codec_delete_gpio_if(gpio_if_);
    audio_codec_delete_data_if(data_if_);
}

void T_Circle_S3_Audio_Codec::create_voice_hardware(gpio_num_t mic_bclk, gpio_num_t mic_ws, gpio_num_t mic_data,
                                                    gpio_num_t spkr_bclk, gpio_num_t spkr_lrclk, gpio_num_t spkr_data)
{
    // assert(input_sample_rate_ == output_sample_rate_);

    // ESP_LOGI(TAG, "Audio IOs: mic_bclk: %d, mic_ws: %d, mic_data: %d, spkr_bclk: %d, spkr_lrclk: %d, spkr_data: %d",
    //          mic_bclk, mic_ws, mic_data, spkr_bclk, spkr_lrclk, spkr_data);

    // i2s_chan_config_t mic_chan_config =
    //     {
    //         .id = I2S_NUM_0,
    //         .role = I2S_ROLE_MASTER,
    //         .dma_desc_num = 6,
    //         .dma_frame_num = 240,
    //         .auto_clear_after_cb = true,
    //         .auto_clear_before_cb = false,
    //         .intr_priority = 0,
    //     };
    // i2s_chan_config_t spkr_chan_config =
    //     {
    //         .id = I2S_NUM_1,
    //         .role = I2S_ROLE_MASTER,
    //         .dma_desc_num = 6,
    //         .dma_frame_num = 240,
    //         .auto_clear_after_cb = true,
    //         .auto_clear_before_cb = false,
    //         .intr_priority = 0,
    //     };
    i2s_chan_config_t mic_chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    mic_chan_config.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    i2s_chan_config_t spkr_chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    spkr_chan_config.auto_clear = true; // Auto clear the legacy data in the DMA buffer

    ESP_ERROR_CHECK(i2s_new_channel(&mic_chan_config, NULL, &rx_handle_));
    ESP_ERROR_CHECK(i2s_new_channel(&spkr_chan_config, &tx_handle_, NULL));

    i2s_std_config_t mic_config = {
        // .clk_cfg =
        //     {
        //         .sample_rate_hz = static_cast<uint32_t>(input_sample_rate_),
        //         .clk_src = I2S_CLK_SRC_DEFAULT,
        //         .ext_clk_freq_hz = 0,
        //         .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        //     },
        // .slot_cfg =
        //     {
        //         .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
        //         .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
        //         .slot_mode = I2S_SLOT_MODE_STEREO,
        //         .slot_mask = I2S_STD_SLOT_BOTH,
        //         .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
        //         .ws_pol = false,
        //         .bit_shift = true,
        //         .left_align = true,
        //         .big_endian = false,
        //         .bit_order_lsb = false,
        //     },
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(input_sample_rate_)),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = mic_bclk,
                .ws = mic_ws,
                .dout = I2S_GPIO_UNUSED,
                .din = mic_data,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            }};

    i2s_std_config_t spkr_config = {
        // .clk_cfg =
        //     {
        //         .sample_rate_hz = static_cast<uint32_t>(output_sample_rate_),
        //         .clk_src = I2S_CLK_SRC_DEFAULT,
        //         .ext_clk_freq_hz = 0,
        //         .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        //     },
        // .slot_cfg =
        //     {
        //         .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
        //         .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
        //         .slot_mode = I2S_SLOT_MODE_STEREO,
        //         .slot_mask = I2S_STD_SLOT_BOTH,
        //         .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
        //         .ws_pol = false,
        //         .bit_shift = true,
        //         .left_align = true,
        //         .big_endian = false,
        //         .bit_order_lsb = false,
        //     },
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(11025)),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = spkr_bclk,
                .ws = spkr_lrclk,
                .dout = spkr_data,
                .din = I2S_GPIO_UNUSED,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            }};

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &mic_config));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &spkr_config));
    ESP_LOGI(TAG, "voice hardware created");
}

void T_Circle_S3_Audio_Codec::SetOutputVolume(int volume)
{
    // ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
    _volume = volume;
    AudioCodec::SetOutputVolume(volume);
}

void T_Circle_S3_Audio_Codec::EnableInput(bool enable)
{
    // if (enable == input_enabled_)
    // {
    //     return;
    // }
    if (enable)
    {
        // esp_codec_dev_sample_info_t fs = {
        //     .bits_per_sample = 16,
        //     .channel = 1,
        //     .channel_mask = 0,
        //     .sample_rate = (uint32_t)input_sample_rate_,
        //     .mclk_multiple = 0,
        // };
        // ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
        // ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(input_dev_, 40.0));
    }
    else
    {
        // ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    }
    AudioCodec::EnableInput(enable);
}

void T_Circle_S3_Audio_Codec::EnableOutput(bool enable)
{
    // if (enable == output_enabled_)
    // {
    //     return;
    // }
    if (enable)
    {
        // // Play 16bit 1 channel
        // esp_codec_dev_sample_info_t fs = {
        //     .bits_per_sample = 16,
        //     .channel = 1,
        //     .channel_mask = 0,
        //     .sample_rate = (uint32_t)output_sample_rate_,
        //     .mclk_multiple = 0,
        // };
        // ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs));
        // ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_));
        gpio_set_level(GPIO_NUM_45, 1);
    }
    else
    {
        // ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
        gpio_set_level(GPIO_NUM_45, 0);
    }
    AudioCodec::EnableOutput(enable);
}

int T_Circle_S3_Audio_Codec::Read(int16_t *dest, int samples)
{
    if (input_enabled_)
    {
        size_t bytes_read;

        // ESP_ERROR_CHECK_W8ITHOUT_ABORT(esp_codec_dev_read(input_dev_, (void *)dest, samples * sizeof(int16_t)));
        // i2s_read(I2S_NUM_0, dest, samples * sizeof(int16_t), &bytes_read, portMAX_DELAY);
        i2s_channel_read(rx_handle_, dest, samples * sizeof(int16_t), &bytes_read, portMAX_DELAY);
    }
    return samples;
}

void Adjust_Volume(const int16_t *input_data, int16_t *output_data, size_t samples, float volume)
{
    for (size_t i = 0; i < samples; i++)
    {
        output_data[i] = (float)input_data[i] * volume;
    }
}

int T_Circle_S3_Audio_Codec::Write(const int16_t *data, int samples)
{
    if (output_enabled_)
    {
        size_t bytes_read;

        // ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(output_dev_, (void *)data, samples * sizeof(int16_t)));
        // i2s_write(I2S_NUM_1, data, samples * sizeof(int16_t), &bytes_read, portMAX_DELAY);

        auto output_data = (int16_t *)malloc(samples * sizeof(int16_t));

        Adjust_Volume(data, output_data, samples, (float)(_volume / 100.0));

        i2s_channel_write(tx_handle_, output_data, samples * sizeof(int16_t), &bytes_read, portMAX_DELAY);  

        free(output_data);
    }
    return samples;
}
