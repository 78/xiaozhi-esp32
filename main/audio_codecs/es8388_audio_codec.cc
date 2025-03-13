#include "es8388_audio_codec.h"

#include <esp_log.h>

// 定义日志标签，用于在日志输出中标识该音频编解码器相关信息
static const char TAG[] = "Es8388AudioCodec";

// 构造函数，用于初始化 Es8388AudioCodec 对象
// 参数说明：
// i2c_master_handle: I2C 主设备句柄，用于与 ES8388 芯片进行通信
// i2c_port: I2C 端口号
// input_sample_rate: 音频输入的采样率
// output_sample_rate: 音频输出的采样率
// mclk: 主时钟引脚
// bclk: 位时钟引脚
// ws: 字选择引脚
// dout: 数据输出引脚
// din: 数据输入引脚
// pa_pin: 功率放大器引脚
// es8388_addr: ES8388 芯片的 I2C 地址
Es8388AudioCodec::Es8388AudioCodec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate, int output_sample_rate,
    gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
    gpio_num_t pa_pin, uint8_t es8388_addr) {
    // 设置是否支持双工模式（同时进行音频输入和输出）
    duplex_ = true; 
    // 设置是否使用参考输入以实现回声消除，这里不使用
    input_reference_ = false; 
    // 设置音频输入的通道数
    input_channels_ = 1; 
    // 记录音频输入的采样率
    input_sample_rate_ = input_sample_rate;
    // 记录音频输出的采样率
    output_sample_rate_ = output_sample_rate;
    // 记录功率放大器引脚
    pa_pin_ = pa_pin;
    // 创建双工 I2S 通道，用于音频数据的输入和输出
    CreateDuplexChannels(mclk, bclk, ws, dout, din);

    // 初始化相关接口，包括数据接口、控制接口和 GPIO 接口
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    // 创建 I2S 数据接口对象
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    // 确保数据接口对象创建成功
    assert(data_if_ != NULL);

    // 配置音频输出相关的 I2C 控制接口
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = i2c_port,
        .addr = es8388_addr,
        .bus_handle = i2c_master_handle,
    };
    // 创建 I2C 控制接口对象
    ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    // 确保控制接口对象创建成功
    assert(ctrl_if_ != NULL);

    // 创建 GPIO 接口对象
    gpio_if_ = audio_codec_new_gpio();
    // 确保 GPIO 接口对象创建成功
    assert(gpio_if_ != NULL);

    // 配置 ES8388 编解码芯片
    es8388_codec_cfg_t es8388_cfg = {};
    es8388_cfg.ctrl_if = ctrl_if_;
    es8388_cfg.gpio_if = gpio_if_;
    // 设置编解码芯片的工作模式为同时支持输入和输出
    es8388_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH;
    // 设置 ES8388 芯片为主模式
    es8388_cfg.master_mode = true;
    es8388_cfg.pa_pin = pa_pin;
    // 设置功率放大器极性是否反转
    es8388_cfg.pa_reverted = false;
    es8388_cfg.hw_gain.pa_voltage = 5.0;
    es8388_cfg.hw_gain.codec_dac_voltage = 3.3;
    // 创建 ES8388 编解码接口对象
    codec_if_ = es8388_codec_new(&es8388_cfg);
    // 确保编解码接口对象创建成功
    assert(codec_if_ != NULL);

    // 配置输出设备
    esp_codec_dev_cfg_t outdev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if_,
        .data_if = data_if_,
    };
    // 创建输出设备对象
    output_dev_ = esp_codec_dev_new(&outdev_cfg);
    // 确保输出设备对象创建成功
    assert(output_dev_ != NULL);

    // 配置输入设备
    esp_codec_dev_cfg_t indev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = codec_if_,
        .data_if = data_if_,
    };
    // 创建输入设备对象
    input_dev_ = esp_codec_dev_new(&indev_cfg);
    // 确保输入设备对象创建成功
    assert(input_dev_ != NULL);

    // 设置输出设备和输入设备在关闭时不禁用
    esp_codec_set_disable_when_closed(output_dev_, false);
    esp_codec_set_disable_when_closed(input_dev_, false);

    // 记录初始化完成的日志信息
    ESP_LOGI(TAG, "Es8388AudioCodec initialized");
}

// 析构函数，用于释放 Es8388AudioCodec 对象占用的资源
Es8388AudioCodec::~Es8388AudioCodec() {
    // 关闭输出设备
    ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    // 删除输出设备对象
    esp_codec_dev_delete(output_dev_);
    // 关闭输入设备
    ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    // 删除输入设备对象
    esp_codec_dev_delete(input_dev_);

    // 删除编解码接口对象
    audio_codec_delete_codec_if(codec_if_);
    // 删除控制接口对象
    audio_codec_delete_ctrl_if(ctrl_if_);
    // 删除 GPIO 接口对象
    audio_codec_delete_gpio_if(gpio_if_);
    // 删除数据接口对象
    audio_codec_delete_data_if(data_if_);
}

// 创建双工 I2S 通道，用于音频数据的输入和输出
// 参数说明：
// mclk: 主时钟引脚
// bclk: 位时钟引脚
// ws: 字选择引脚
// dout: 数据输出引脚
// din: 数据输入引脚
void Es8388AudioCodec::CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din){
    // 确保输入和输出的采样率相同
    assert(input_sample_rate_ == output_sample_rate_);

    // 配置 I2S 通道
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    // 创建 I2S 通道
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    // 配置标准 I2S 模式
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
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = dout,
            .din = din,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    // 初始化发送通道为标准 I2S 模式
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    // 初始化接收通道为标准 I2S 模式
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
    // 记录双工通道创建完成的日志信息
    ESP_LOGI(TAG, "Duplex channels created");
}

// 设置音频输出音量
// 参数 volume: 要设置的音量值
void Es8388AudioCodec::SetOutputVolume(int volume) {
    // 设置输出设备的音量
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
    // 调用基类的 SetOutputVolume 函数
    AudioCodec::SetOutputVolume(volume);
}

// 启用或禁用音频输入功能
// 参数 enable: true 表示启用输入，false 表示禁用输入
void Es8388AudioCodec::EnableInput(bool enable) {
    // 如果当前输入状态与要设置的状态相同，则直接返回
    if (enable == input_enabled_) {
        return;
    }
    if (enable) {
        // 配置输入采样信息
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)input_sample_rate_,
            .mclk_multiple = 0,
        };
        // 打开输入设备
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
        // 设置输入增益
        ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(input_dev_, 24.0));
    } else {
        // 关闭输入设备
        ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    }
    // 调用基类的 EnableInput 函数
    AudioCodec::EnableInput(enable);
}

// 启用或禁用音频输出功能
// 参数 enable: true 表示启用输出，false 表示禁用输出
void Es8388AudioCodec::EnableOutput(bool enable) {
    // 如果当前输出状态与要设置的状态相同，则直接返回
    if (enable == output_enabled_) {
        return;
    }
    if (enable) {
        // 配置输出采样信息
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = 0,
        };
        // 打开输出设备
        ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs));
        // 设置输出设备的音量
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_));

        // 设置模拟输出音量为 0dB，默认是 -45dB
        uint8_t reg_val = 30; // 0dB
        // 定义要设置的寄存器数组，包括左右声道的耳机音量和扬声器音量寄存器
        uint8_t regs[] = { 46, 47, 48, 49 }; // HP_LVOL, HP_RVOL, SPK_LVOL, SPK_RVOL
        for (uint8_t reg : regs) {
            // 向指定寄存器写入音量值
            ctrl_if_->write_reg(ctrl_if_, reg, 1, &reg_val, 1);
        }

        // 如果功率放大器引脚有效，则将其电平设置为高
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 1);
        }
    } else {
        // 关闭输出设备
        ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
        // 如果功率放大器引脚有效，则将其电平设置为低
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 0);
        }
    }
    // 调用基类的 EnableOutput 函数
    AudioCodec::EnableOutput(enable);
}

// 从输入设备读取音频数据
// 参数 dest: 存储读取数据的缓冲区
// 参数 samples: 要读取的样本数
// 返回值: 实际读取的样本数
int Es8388AudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_) {
        // 如果输入功能已启用，则从输入设备读取数据
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(input_dev_, (void*)dest, samples * sizeof(int16_t)));
    }
    return samples;
}

// 向输出设备写入音频数据
// 参数 data: 要写入的数据缓冲区
// 参数 samples: 要写入的样本数
// 返回值: 实际写入的样本数
int Es8388AudioCodec::Write(const int16_t* data, int samples) {
    if (output_enabled_) {
        // 如果输出功能已启用，则向输出设备写入数据
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(output_dev_, (void*)data, samples * sizeof(int16_t)));
    }
    return samples;
}