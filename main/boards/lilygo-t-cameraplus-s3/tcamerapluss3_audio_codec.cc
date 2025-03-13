#include "tcamerapluss3_audio_codec.h"

#include <esp_log.h>
#include <driver/i2c.h>
#include <driver/i2c_master.h>
#include <driver/i2s_tdm.h>

static const char TAG[] = "Tcamerapluss3AudioCodec"; // 日志标签

// Tcamerapluss3AudioCodec 构造函数
Tcamerapluss3AudioCodec::Tcamerapluss3AudioCodec(int input_sample_rate, int output_sample_rate,
    gpio_num_t mic_bclk, gpio_num_t mic_ws, gpio_num_t mic_data,
    gpio_num_t spkr_bclk, gpio_num_t spkr_lrclk, gpio_num_t spkr_data,
    bool input_reference) {
    duplex_ = true;                             // 是否双工（同时支持输入和输出）
    input_reference_ = input_reference;         // 是否使用参考输入，实现回声消除
    input_channels_ = input_reference_ ? 2 : 1; // 输入通道数（如果使用参考输入，则为 2 通道）
    input_sample_rate_ = input_sample_rate;     // 输入采样率
    output_sample_rate_ = output_sample_rate;   // 输出采样率

    // 创建音频硬件（麦克风和扬声器的 I2S 配置）
    CreateVoiceHardware(mic_bclk, mic_ws, mic_data, spkr_bclk, spkr_lrclk, spkr_data);

    ESP_LOGI(TAG, "Tcamerapluss3AudioCodec initialized"); // 打印初始化日志
}

// Tcamerapluss3AudioCodec 析构函数
Tcamerapluss3AudioCodec::~Tcamerapluss3AudioCodec() {
    // 释放音频编解码器相关资源
    audio_codec_delete_codec_if(in_codec_if_);
    audio_codec_delete_ctrl_if(in_ctrl_if_);
    audio_codec_delete_codec_if(out_codec_if_);
    audio_codec_delete_ctrl_if(out_ctrl_if_);
    audio_codec_delete_gpio_if(gpio_if_);
    audio_codec_delete_data_if(data_if_);
}

// 创建音频硬件（麦克风和扬声器的 I2S 配置）
void Tcamerapluss3AudioCodec::CreateVoiceHardware(gpio_num_t mic_bclk, gpio_num_t mic_ws, gpio_num_t mic_data,
    gpio_num_t spkr_bclk, gpio_num_t spkr_lrclk, gpio_num_t spkr_data) {
    
    // 配置麦克风 I2S 通道
    i2s_chan_config_t mic_chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    mic_chan_config.auto_clear = true; // 自动清除 DMA 缓冲区中的旧数据
    // 配置扬声器 I2S 通道
    i2s_chan_config_t spkr_chan_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    spkr_chan_config.auto_clear = true; // 自动清除 DMA 缓冲区中的旧数据

    // 创建麦克风和扬声器 I2S 通道
    ESP_ERROR_CHECK(i2s_new_channel(&mic_chan_config, NULL, &rx_handle_)); // 麦克风接收通道
    ESP_ERROR_CHECK(i2s_new_channel(&spkr_chan_config, &tx_handle_, NULL)); // 扬声器发送通道

    // 配置麦克风 I2S 标准模式
    i2s_std_config_t mic_config = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_, // 采样率
            .clk_src = I2S_CLK_SRC_DEFAULT, // 时钟源
            .mclk_multiple = I2S_MCLK_MULTIPLE_256, // MCLK 倍数
            #ifdef   I2S_HW_VERSION_2    
                .ext_clk_freq_hz = 0, // 外部时钟频率
            #endif
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), // 单声道配置
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, // MCLK 引脚未使用
            .bclk = mic_bclk, // 位时钟引脚
            .ws = mic_ws, // 字选择引脚
            .dout = I2S_GPIO_UNUSED, // 数据输出引脚未使用
            .din = mic_data, // 数据输入引脚
            .invert_flags = {
                .mclk_inv = false, // MCLK 不反转
                .bclk_inv = false, // 位时钟不反转
                .ws_inv = true // 字选择反转（默认右通道）
            }
        }
    };

    // 配置扬声器 I2S 标准模式
    i2s_std_config_t spkr_config = {
        .clk_cfg ={
            .sample_rate_hz = static_cast<uint32_t>(11025), // 采样率
            .clk_src = I2S_CLK_SRC_DEFAULT, // 时钟源
            .ext_clk_freq_hz = 0, // 外部时钟频率
            .mclk_multiple = I2S_MCLK_MULTIPLE_256, // MCLK 倍数
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO), // 立体声配置
        .gpio_cfg ={
            .mclk = I2S_GPIO_UNUSED, // MCLK 引脚未使用
            .bclk = spkr_bclk, // 位时钟引脚
            .ws = spkr_lrclk, // 字选择引脚
            .dout = spkr_data, // 数据输出引脚
            .din = I2S_GPIO_UNUSED, // 数据输入引脚未使用
            .invert_flags = {
                .mclk_inv = false, // MCLK 不反转
                .bclk_inv = false, // 位时钟不反转
                .ws_inv = false // 字选择不反转
            }
        }
    };

    // 初始化麦克风和扬声器 I2S 通道
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &mic_config)); // 初始化麦克风通道
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &spkr_config)); // 初始化扬声器通道
    ESP_LOGI(TAG, "Voice hardware created"); // 打印硬件创建日志
}

// 设置输出音量
void Tcamerapluss3AudioCodec::SetOutputVolume(int volume) {
    volume_ = volume; // 保存音量值
    AudioCodec::SetOutputVolume(volume); // 调用基类方法设置音量
}

// 启用或禁用输入
void Tcamerapluss3AudioCodec::EnableInput(bool enable) {
    AudioCodec::EnableInput(enable); // 调用基类方法启用或禁用输入
}

// 启用或禁用输出
void Tcamerapluss3AudioCodec::EnableOutput(bool enable) {
    AudioCodec::EnableOutput(enable); // 调用基类方法启用或禁用输出
}

// 读取音频数据
int Tcamerapluss3AudioCodec::Read(int16_t *dest, int samples) {
    if (input_enabled_) { // 如果输入已启用
        size_t bytes_read;
        // 从麦克风通道读取音频数据
        i2s_channel_read(rx_handle_, dest, samples * sizeof(int16_t), &bytes_read, portMAX_DELAY);
    }
    return samples; // 返回读取的样本数
}

// 调整音频数据音量
void AdjustTcamerapluss3Volume(const int16_t *input_data, int16_t *output_data, size_t samples, float volume) {
    for (size_t i = 0; i < samples; i++) {
        output_data[i] = (float)input_data[i] * volume; // 根据音量调整音频数据
    }
}

// 写入音频数据
int Tcamerapluss3AudioCodec::Write(const int16_t *data, int samples) {
    if (output_enabled_) { // 如果输出已启用
        size_t bytes_read;
        auto output_data = (int16_t *)malloc(samples * sizeof(int16_t)); // 分配输出缓冲区
        AdjustTcamerapluss3Volume(data, output_data, samples, (float)(volume_ / 100.0)); // 调整音量
        // 向扬声器通道写入音频数据
        i2s_channel_write(tx_handle_, output_data, samples * sizeof(int16_t), &bytes_read, portMAX_DELAY);
        free(output_data); // 释放输出缓冲区
    }
    return samples; // 返回写入的样本数
}