#include "es8388_audio_codec.h"

#include <esp_log.h>

static const char TAG[] = "Es8388AudioCodec";

Es8388AudioCodec::Es8388AudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
    gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
    gpio_num_t pa_pin, uint8_t es8388_addr)
{
    duplex_             = true;                 /* 是否全双工 */
    input_reference_    = false;                /* 是否使用参考输入，实现回声消除 */
    input_channels_     = 1;                    /* 输入通道数 */
    input_sample_rate_  = input_sample_rate;    /* 输入采样率 */
    output_sample_rate_ = output_sample_rate;   /* 输出采样率 */

    CreateDuplexChannels(mclk, bclk, ws, dout, din);    /* 创建双工通道 */

    /* 初始化I2S的接口 */
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);

    /* 初始化音频芯片的I2C通信接口 */
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = i2c_port,
        .addr = es8388_addr,
        .bus_handle = i2c_master_handle,
    };
    ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(ctrl_if_ != NULL);

    /* 初始化音频设备的IO口 */
    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);

    /* 配置ES8388音频编解码芯片 */
    es8388_codec_cfg_t es8388_cfg = {};
    es8388_cfg.ctrl_if                      = ctrl_if_;                         /* 音频控制接口 */
    es8388_cfg.gpio_if                      = gpio_if_;                         /* 音频GPIO接口 */
    es8388_cfg.codec_mode                   = ESP_CODEC_DEV_WORK_MODE_BOTH;
    es8388_cfg.master_mode                  = true;
    es8388_cfg.pa_pin                       = pa_pin;
    es8388_cfg.pa_reverted                  = false;
    es8388_cfg.hw_gain.pa_voltage           = 5.0;
    es8388_cfg.hw_gain.codec_dac_voltage    = 3.3;
    codec_if_ = es8388_codec_new(&es8388_cfg);
    assert(codec_if_ != NULL);

    esp_codec_dev_cfg_t outdev_cfg = {              /* 音频设备配置 */
        .dev_type   = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if   = codec_if_,
        .data_if    = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&outdev_cfg);   /* ES8388作出输出设备 */
    assert(output_dev_ != NULL);

    esp_codec_dev_cfg_t indev_cfg = {               /* 音频设备配置 */
        .dev_type   = ESP_CODEC_DEV_TYPE_IN,
        .codec_if   = codec_if_,
        .data_if    = data_if_,
    };
    input_dev_ = esp_codec_dev_new(&indev_cfg);     /* ES8388作为输入设备 */
    assert(input_dev_ != NULL);  

    ESP_LOGI(TAG, "Es8388AudioCodec initialized");
}

Es8388AudioCodec::~Es8388AudioCodec()
{
    ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    esp_codec_dev_delete(output_dev_);
    ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    esp_codec_dev_delete(input_dev_);

    audio_codec_delete_codec_if(codec_if_);
    audio_codec_delete_ctrl_if(ctrl_if_);
    audio_codec_delete_gpio_if(gpio_if_);
    audio_codec_delete_data_if(data_if_);
}

void Es8388AudioCodec::CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din)
{
    assert(input_sample_rate_ == output_sample_rate_);

    i2s_chan_config_t chan_cfg = {                      /* I2S通道配置  */
        .id                     = I2S_NUM_0,            /* I2S0 */  
        .role                   = I2S_ROLE_MASTER,      /* I2S主机 */
        .dma_desc_num           = 6,
        .dma_frame_num          = 240 * 3,
        .auto_clear_after_cb    = true,
        .auto_clear_before_cb   = false,
        .intr_priority          = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));      /* 分配新的I2S通道 */

    i2s_std_config_t std_cfg = {                                                /* 标准通信模式配置 */
        .clk_cfg = {                /* 时钟配置 可用I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE)宏函数辅助配置 */
            .sample_rate_hz     = (uint32_t)output_sample_rate_,                /* I2S采样率 */
            .clk_src            = I2S_CLK_SRC_DEFAULT,                          /* I2S时钟源 */
            .ext_clk_freq_hz    = 0,
            .mclk_multiple      = I2S_MCLK_MULTIPLE_256                         /* I2S主时钟MCLK相对于采样率的倍数(默认256) */
        },
        .slot_cfg = {               /* 声道配置,可用I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)宏函数辅助配置(支持16位宽采样数据) */
            .data_bit_width     = I2S_DATA_BIT_WIDTH_16BIT,                     /* 声道支持16位宽的采样数据 */
            .slot_bit_width     = I2S_SLOT_BIT_WIDTH_AUTO,                      /* 通道位宽 */
            .slot_mode          = I2S_SLOT_MODE_STEREO,                         /* 立体声模式 */
            .slot_mask          = I2S_STD_SLOT_BOTH,                            /* 立体声 */
            .ws_width           = I2S_DATA_BIT_WIDTH_16BIT,                     /* WS信号位宽 */
            .ws_pol             = false,                                        /* WS信号极性 */
            .bit_shift          = true,                                         /* 位移位(Philips模式下配置) */
            .left_align         = true,                                         /* 左对齐 */
            .big_endian         = false,                                        /* 小端模式 */
            .bit_order_lsb      = false                                         /* MSB */
        },
        .gpio_cfg = {               /* 引脚配置 */
            .mclk       = mclk,     /* 主时钟线 */
            .bclk       = bclk,     /* 位时钟线 */
            .ws         = ws,       /* 字(声道)选择线 */
            .dout       = dout,     /* 串行数据输出线 */
            .din        = din,      /* 串行数据输入线 */
            .invert_flags = {       /* 引脚翻转(不反相) */
                .mclk_inv   = false,
                .bclk_inv   = false,
                .ws_inv     = false
            }
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));   /* 初始化RX通道 */
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));   /* 初始化TX通道 */

    ESP_LOGI(TAG, "Duplex channels created");
}

void Es8388AudioCodec::SetOutputVolume(int volume)
{
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
    AudioCodec::SetOutputVolume(volume);
}

void Es8388AudioCodec::EnableInput(bool enable)
{
    if (enable == input_enabled_)
    {
        return;
    }

    if (enable)
    {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample    = 16,
            .channel            = 1,
            .channel_mask       = 0,
            .sample_rate        = (uint32_t)input_sample_rate_,
            .mclk_multiple      = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(input_dev_, 40.0));
    }
    else    /* 输入输出共用的i2s，只关闭了输出后会把输入也关没了，所以得注释关闭 */
    {
        // ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    }

    AudioCodec::EnableInput(enable);
}

void Es8388AudioCodec::EnableOutput(bool enable)
{
    if (enable == output_enabled_)
    {
        return;
    }

    if (enable)
    {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample    = 16,
            .channel            = 1,
            .channel_mask       = 0,
            .sample_rate        = (uint32_t)output_sample_rate_,
            .mclk_multiple      = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_));
    }
    else    /* 输入输出共用的i2s，只关闭了输出后会把输入也关没了，所以得注释关闭 */
    {
        // ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    }

    AudioCodec::EnableOutput(enable);
}

int Es8388AudioCodec::Read(int16_t* dest, int samples)
{
    if (input_enabled_)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(input_dev_, (void*)dest, samples * sizeof(int16_t)));
    }

    return samples;
}

int Es8388AudioCodec::Write(const int16_t* data, int samples)
{
    if (output_enabled_)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(output_dev_, (void*)data, samples * sizeof(int16_t)));
    }

    return samples;
}