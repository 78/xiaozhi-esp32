#include "cores3_audio_codec.h"

#include <esp_log.h>
#include <driver/i2c.h>
#include <driver/i2c_master.h>
#include <driver/i2s_tdm.h>

// 定义日志标签，用于在日志中标识该音频编解码器相关信息
static const char TAG[] = "CoreS3AudioCodec";

// 构造函数，初始化 CoreS3AudioCodec 对象
// 参数说明：
// i2c_master_handle: I2C 主设备句柄，用于与音频编解码芯片通信
// input_sample_rate: 音频输入的采样率
// output_sample_rate: 音频输出的采样率
// mclk: 主时钟引脚
// bclk: 位时钟引脚
// ws: 字选择引脚
// dout: 数据输出引脚
// din: 数据输入引脚
// aw88298_addr: AW88298 音频编解码芯片的 I2C 地址
// es7210_addr: ES7210 音频编解码芯片的 I2C 地址
// input_reference: 是否使用参考输入以实现回声消除
CoreS3AudioCodec::CoreS3AudioCodec(void* i2c_master_handle, int input_sample_rate, int output_sample_rate,
    gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
    uint8_t aw88298_addr, uint8_t es7210_addr, bool input_reference) {
    // 设置为双工模式，即支持同时进行音频输入和输出
    duplex_ = true; 
    // 是否使用参考输入以实现回声消除
    input_reference_ = input_reference; 
    // 根据是否使用参考输入确定输入通道数
    input_channels_ = input_reference_ ? 2 : 1; 
    // 记录音频输入的采样率
    input_sample_rate_ = input_sample_rate;
    // 记录音频输出的采样率
    output_sample_rate_ = output_sample_rate;

    // 创建双工 I2S 通道，用于音频数据的输入和输出
    CreateDuplexChannels(mclk, bclk, ws, dout, din);

    // 初始化相关接口，包括数据接口、控制接口和 GPIO 接口
    // 配置 I2S 数据接口
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    // 创建 I2S 数据接口对象
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    // 确保数据接口对象创建成功
    assert(data_if_ != NULL);

    // 配置音频输出（扬声器）相关接口
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = (i2c_port_t)1,
        .addr = aw88298_addr,
        .bus_handle = i2c_master_handle,
    };
    // 创建 I2C 控制接口对象，用于控制 AW88298 芯片
    out_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    // 确保输出控制接口对象创建成功
    assert(out_ctrl_if_ != NULL);

    // 创建 GPIO 接口对象
    gpio_if_ = audio_codec_new_gpio();
    // 确保 GPIO 接口对象创建成功
    assert(gpio_if_ != NULL);

    // 配置 AW88298 音频编解码芯片
    aw88298_codec_cfg_t aw88298_cfg = {};
    aw88298_cfg.ctrl_if = out_ctrl_if_;
    aw88298_cfg.gpio_if = gpio_if_;
    aw88298_cfg.reset_pin = GPIO_NUM_NC;
    aw88298_cfg.hw_gain.pa_voltage = 5.0;
    aw88298_cfg.hw_gain.codec_dac_voltage = 3.3;
    aw88298_cfg.hw_gain.pa_gain = 1;
    // 创建 AW88298 编解码接口对象
    out_codec_if_ = aw88298_codec_new(&aw88298_cfg);
    // 确保输出编解码接口对象创建成功
    assert(out_codec_if_ != NULL);

    // 配置输出设备
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = out_codec_if_,
        .data_if = data_if_,
    };
    // 创建输出设备对象
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    // 确保输出设备对象创建成功
    assert(output_dev_ != NULL);

    // 配置音频输入（麦克风）相关接口
    i2c_cfg.addr = es7210_addr;
    // 创建 I2C 控制接口对象，用于控制 ES7210 芯片
    in_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    // 确保输入控制接口对象创建成功
    assert(in_ctrl_if_ != NULL);

    // 配置 ES7210 音频编解码芯片
    es7210_codec_cfg_t es7210_cfg = {};
    es7210_cfg.ctrl_if = in_ctrl_if_;
    es7210_cfg.mic_selected = ES7120_SEL_MIC1 | ES7120_SEL_MIC2 | ES7120_SEL_MIC3;
    // 创建 ES7210 编解码接口对象
    in_codec_if_ = es7210_codec_new(&es7210_cfg);
    // 确保输入编解码接口对象创建成功
    assert(in_codec_if_ != NULL);

    // 配置输入设备
    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    dev_cfg.codec_if = in_codec_if_;
    // 创建输入设备对象
    input_dev_ = esp_codec_dev_new(&dev_cfg);
    // 确保输入设备对象创建成功
    assert(input_dev_ != NULL);

    // 记录初始化完成日志
    ESP_LOGI(TAG, "CoreS3AudioCodec initialized");
}

// 析构函数，用于释放 CoreS3AudioCodec 对象占用的资源
CoreS3AudioCodec::~CoreS3AudioCodec() {
    // 关闭输出设备
    ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    // 删除输出设备对象
    esp_codec_dev_delete(output_dev_);
    // 关闭输入设备
    ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    // 删除输入设备对象
    esp_codec_dev_delete(input_dev_);

    // 删除输入编解码接口对象
    audio_codec_delete_codec_if(in_codec_if_);
    // 删除输入控制接口对象
    audio_codec_delete_ctrl_if(in_ctrl_if_);
    // 删除输出编解码接口对象
    audio_codec_delete_codec_if(out_codec_if_);
    // 删除输出控制接口对象
    audio_codec_delete_ctrl_if(out_ctrl_if_);
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
void CoreS3AudioCodec::CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
    // 确保输入和输出采样率相同
    assert(input_sample_rate_ == output_sample_rate_);

    // 记录音频输入输出引脚信息
    ESP_LOGI(TAG, "Audio IOs: mclk: %d, bclk: %d, ws: %d, dout: %d, din: %d", mclk, bclk, ws, dout, din);

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
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    // 配置 TDM I2S 模式
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
            .mclk = mclk,
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

    // 初始化发送通道为标准 I2S 模式
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    // 初始化接收通道为 TDM I2S 模式
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(rx_handle_, &tdm_cfg));
    // 记录双工通道创建完成日志
    ESP_LOGI(TAG, "Duplex channels created");
}

// 设置音频输出音量
// 参数 volume: 要设置的音量值
void CoreS3AudioCodec::SetOutputVolume(int volume) {
    // 设置输出设备的音量
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
    // 调用基类的 SetOutputVolume 函数
    AudioCodec::SetOutputVolume(volume);
}

// 启用或禁用音频输入功能
// 参数 enable: true 表示启用输入，false 表示禁用输入
void CoreS3AudioCodec::EnableInput(bool enable) {
    // 如果当前输入状态与要设置的状态相同，则直接返回
    if (enable == input_enabled_) {
        return;
    }
    if (enable) {
        // 配置输入采样信息
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 2,
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = 0,
        };
        if (input_reference_) {
            // 如果使用参考输入，添加参考通道掩码
            fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);
        }
        // 打开输入设备
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
        // 设置输入通道增益
        ESP_ERROR_CHECK(esp_codec_dev_set_in_channel_gain(input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), 40.0));
    } else {
        // 关闭输入设备
        ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    }
    // 调用基类的 EnableInput 函数
    AudioCodec::EnableInput(enable);
}

// 启用或禁用音频输出功能
// 参数 enable: true 表示启用输出，false 表示禁用输出
void CoreS3AudioCodec::EnableOutput(bool enable) {
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
    } else {
        // 关闭输出设备
        ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    }
    // 调用基类的 EnableOutput 函数
    AudioCodec::EnableOutput(enable);
}

// 从输入设备读取音频数据
// 参数 dest: 存储读取数据的缓冲区
// 参数 samples: 要读取的样本数
// 返回值: 实际读取的样本数
int CoreS3AudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_) {
        // 如果输入已启用，从输入设备读取数据
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(input_dev_, (void*)dest, samples * sizeof(int16_t)));
    }
    return samples;
}

// 向输出设备写入音频数据
// 参数 data: 要写入的数据缓冲区
// 参数 samples: 要写入的样本数
// 返回值: 实际写入的样本数
int CoreS3AudioCodec::Write(const int16_t* data, int samples) {
    if (output_enabled_) {
        // 如果输出已启用，向输出设备写入数据
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(output_dev_, (void*)data, samples * sizeof(int16_t)));
    }
    return samples;
}