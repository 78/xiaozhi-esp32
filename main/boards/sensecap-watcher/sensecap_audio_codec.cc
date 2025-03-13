#include "sensecap_audio_codec.h"

#include <esp_log.h>
#include <driver/i2c.h>
#include <driver/i2s_tdm.h>

static const char TAG[] = "SensecapAudioCodec";  // 定义日志标签

// SensecapAudioCodec 构造函数，初始化音频编解码器
SensecapAudioCodec::SensecapAudioCodec(void* i2c_master_handle, int input_sample_rate, int output_sample_rate,
    gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
    gpio_num_t pa_pin, uint8_t es8311_addr, uint8_t es7243e_addr, bool input_reference) {
    duplex_ = true;  // 设置为双工模式，支持同时输入和输出
    input_reference_ = input_reference;  // 是否使用参考输入，用于回声消除
    input_channels_ = input_reference_ ? 2 : 1;  // 输入通道数，如果使用参考输入则为 2，否则为 1
    input_sample_rate_ = input_sample_rate;  // 输入采样率
    output_sample_rate_ = output_sample_rate;  // 输出采样率

    // 创建双工通道（输入和输出）
    CreateDuplexChannels(mclk, bclk, ws, dout, din);

    // 初始化相关接口：数据接口、控制接口和 GPIO 接口
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,  // 使用 I2S 端口 0
        .rx_handle = rx_handle_,  // 接收通道句柄
        .tx_handle = tx_handle_,  // 发送通道句柄
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);  // 创建 I2S 数据接口
    assert(data_if_ != NULL);  // 确保数据接口创建成功

    // 输出部分
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = (i2c_port_t)0,  // 使用 I2C 端口 0
        .addr = es8311_addr,  // ES8311 编解码器地址
        .bus_handle = i2c_master_handle,  // I2C 总线句柄
    };
    out_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);  // 创建 I2C 控制接口
    assert(out_ctrl_if_ != NULL);  // 确保控制接口创建成功

    gpio_if_ = audio_codec_new_gpio();  // 创建 GPIO 接口
    assert(gpio_if_ != NULL);  // 确保 GPIO 接口创建成功

    // 配置 ES8311 编解码器
    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if = out_ctrl_if_;  // 控制接口
    es8311_cfg.gpio_if = gpio_if_;  // GPIO 接口
    es8311_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;  // 编解码器模式为 DAC（数字到模拟转换）
    es8311_cfg.pa_pin = pa_pin;  // 功放控制引脚
    es8311_cfg.use_mclk = true;  // 使用主时钟
    es8311_cfg.hw_gain.pa_voltage = 5.0;  // 功放电压
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3;  // DAC 电压
    out_codec_if_ = es8311_codec_new(&es8311_cfg);  // 创建 ES8311 编解码器接口
    assert(out_codec_if_ != NULL);  // 确保编解码器接口创建成功

    // 配置输出设备
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,  // 设备类型为输出
        .codec_if = out_codec_if_,  // 编解码器接口
        .data_if = data_if_,  // 数据接口
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);  // 创建输出设备
    assert(output_dev_ != NULL);  // 确保输出设备创建成功

    // 输入部分
    i2c_cfg.addr = es7243e_addr << 1;  // ES7243E 编解码器地址（左移 1 位）
    in_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);  // 创建 I2C 控制接口
    assert(in_ctrl_if_ != NULL);  // 确保控制接口创建成功

    // 配置 ES7243E 编解码器
    es7243e_codec_cfg_t es7243e_cfg = {};
    es7243e_cfg.ctrl_if = in_ctrl_if_;  // 控制接口
    in_codec_if_ = es7243e_codec_new(&es7243e_cfg);  // 创建 ES7243E 编解码器接口
    assert(in_codec_if_ != NULL);  // 确保编解码器接口创建成功

    // 配置输入设备
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;  // 设备类型为输入
    dev_cfg.codec_if = in_codec_if_;  // 编解码器接口
    input_dev_ = esp_codec_dev_new(&dev_cfg);  // 创建输入设备
    assert(input_dev_ != NULL);  // 确保输入设备创建成功

    // 设置设备在关闭时不自动禁用
    esp_codec_set_disable_when_closed(output_dev_, false);
    esp_codec_set_disable_when_closed(input_dev_, false);

    ESP_LOGI(TAG, "SensecapAudioDevice initialized");  // 日志输出，初始化完成
}

// SensecapAudioCodec 析构函数，释放资源
SensecapAudioCodec::~SensecapAudioCodec() {
    ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));  // 关闭输出设备
    esp_codec_dev_delete(output_dev_);  // 删除输出设备
    ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));  // 关闭输入设备
    esp_codec_dev_delete(input_dev_);  // 删除输入设备

    audio_codec_delete_codec_if(in_codec_if_);  // 删除输入编解码器接口
    audio_codec_delete_ctrl_if(in_ctrl_if_);  // 删除输入控制接口
    audio_codec_delete_codec_if(out_codec_if_);  // 删除输出编解码器接口
    audio_codec_delete_ctrl_if(out_ctrl_if_);  // 删除输出控制接口
    audio_codec_delete_gpio_if(gpio_if_);  // 删除 GPIO 接口
    audio_codec_delete_data_if(data_if_);  // 删除数据接口
}

// 创建双工通道（输入和输出）
void SensecapAudioCodec::CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
    assert(input_sample_rate_ == output_sample_rate_);  // 确保输入和输出采样率相同

    // 配置 I2S 通道
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,  // 使用 I2S 端口 0
        .role = I2S_ROLE_MASTER,  // 主模式
        .dma_desc_num = 6,  // DMA 描述符数量
        .dma_frame_num = 240,  // DMA 帧数量
        .auto_clear_after_cb = true,  // 回调后自动清除
        .auto_clear_before_cb = false,  // 回调前不自动清除
        .intr_priority = 0,  // 中断优先级
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));  // 创建 I2S 通道

    // 配置标准 I2S 模式
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,  // 采样率
            .clk_src = I2S_CLK_SRC_DEFAULT,  // 时钟源
            .ext_clk_freq_hz = 0,  // 外部时钟频率
            .mclk_multiple = I2S_MCLK_MULTIPLE_256  // 主时钟倍数
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,  // 数据位宽
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,  // 槽位宽自动
            .slot_mode = I2S_SLOT_MODE_MONO,  // 单声道模式
            .slot_mask = I2S_STD_SLOT_BOTH,  // 使用左右声道
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,  // WS 信号宽度
            .ws_pol = false,  // WS 信号极性
            .bit_shift = true,  // 位偏移
            .left_align = true,  // 左对齐
            .big_endian = false,  // 小端模式
            .bit_order_lsb = false  // 位顺序为 MSB
        },
        .gpio_cfg = {
            .mclk = mclk,  // 主时钟引脚
            .bclk = bclk,  // 位时钟引脚
            .ws = ws,  // WS 引脚
            .dout = dout,  // 数据输出引脚
            .din = din,  // 数据输入引脚
            .invert_flags = {
                .mclk_inv = false,  // 不反转主时钟
                .bclk_inv = false,  // 不反转位时钟
                .ws_inv = false  // 不反转 WS 信号
            }
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));  // 初始化发送通道

    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;  // 使用右声道
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));  // 初始化接收通道
    ESP_LOGI(TAG, "Duplex channels created");  // 日志输出，双工通道创建完成
}

// 设置输出音量
void SensecapAudioCodec::SetOutputVolume(int volume) {
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));  // 设置输出音量
    AudioCodec::SetOutputVolume(volume);  // 调用父类方法
}

// 启用或禁用输入
void SensecapAudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;  // 如果状态未改变，直接返回
    }
    if (enable) {
        // 配置输入采样信息
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,  // 16 位采样
            .channel = 2,  // 2 通道
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1),  // 通道掩码
            .sample_rate = (uint32_t)output_sample_rate_,  // 采样率
            .mclk_multiple = 0,  // 主时钟倍数
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));  // 打开输入设备
        ESP_ERROR_CHECK(esp_codec_dev_set_in_gain(input_dev_, 27.0));  // 设置输入增益
    } else {
        ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));  // 关闭输入设备
    }
    AudioCodec::EnableInput(enable);  // 调用父类方法
}

// 启用或禁用输出
void SensecapAudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;  // 如果状态未改变，直接返回
    }
    if (enable) {
        // 配置输出采样信息
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,  // 16 位采样
            .channel = 1,  // 1 通道
            .channel_mask = 0,  // 通道掩码
            .sample_rate = (uint32_t)output_sample_rate_,  // 采样率
            .mclk_multiple = 0,  // 主时钟倍数
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs));  // 打开输出设备
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_));  // 设置输出音量
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 1);  // 启用功放
        }
    } 
    else {
        ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));  // 关闭输出设备
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 0);  // 禁用功放
        }
    }
    AudioCodec::EnableOutput(enable);  // 调用父类方法
}

// 从输入设备读取音频数据
int SensecapAudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(input_dev_, (void*)dest, samples * sizeof(int16_t)));  // 读取数据
    }
    return samples;  // 返回读取的样本数
}

// 向输出设备写入音频数据
int SensecapAudioCodec::Write(const int16_t* data, int samples) {
    if (output_enabled_) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(output_dev_, (void*)data, samples * sizeof(int16_t)));  // 写入数据
    }
    return samples;  // 返回写入的样本数
}