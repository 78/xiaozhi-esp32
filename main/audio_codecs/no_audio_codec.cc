#include "no_audio_codec.h"

#include <esp_log.h>
#include <cmath>

#define TAG "NoAudioCodec"

// 析构函数，用于释放资源
NoAudioCodec::~NoAudioCodec() {
    if (rx_handle_ != nullptr) {
        ESP_ERROR_CHECK(i2s_channel_disable(rx_handle_)); // 禁用接收通道
    }
    if (tx_handle_ != nullptr) {
        ESP_ERROR_CHECK(i2s_channel_disable(tx_handle_)); // 禁用发送通道
    }
}

// 双工模式构造函数，初始化I2S通道
NoAudioCodecDuplex::NoAudioCodecDuplex(int input_sample_rate, int output_sample_rate, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
    duplex_ = true; // 设置为双工模式
    input_sample_rate_ = input_sample_rate; // 输入采样率
    output_sample_rate_ = output_sample_rate; // 输出采样率

    // I2S通道配置
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0, // I2S端口号
        .role = I2S_ROLE_MASTER, // 主模式
        .dma_desc_num = 6, // DMA描述符数量
        .dma_frame_num = 240, // DMA帧数
        .auto_clear_after_cb = true, // 回调后自动清除
        .auto_clear_before_cb = false, // 回调前不自动清除
        .intr_priority = 0, // 中断优先级
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_)); // 创建I2S通道

    // I2S标准配置
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_, // 采样率
            .clk_src = I2S_CLK_SRC_DEFAULT, // 时钟源
            .mclk_multiple = I2S_MCLK_MULTIPLE_256, // MCLK倍数
			#ifdef   I2S_HW_VERSION_2    
				.ext_clk_freq_hz = 0, // 外部时钟频率
			#endif
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT, // 数据位宽
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, // 槽位宽
            .slot_mode = I2S_SLOT_MODE_MONO, // 单声道模式
            .slot_mask = I2S_STD_SLOT_LEFT, // 左声道
            .ws_width = I2S_DATA_BIT_WIDTH_32BIT, // WS信号宽度
            .ws_pol = false, // WS信号极性
            .bit_shift = true, // 位偏移
            #ifdef   I2S_HW_VERSION_2   
                .left_align = true, // 左对齐
                .big_endian = false, // 大端模式
                .bit_order_lsb = false // LSB位序
            #endif
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, // MCLK引脚未使用
            .bclk = bclk, // BCLK引脚
            .ws = ws, // WS引脚
            .dout = dout, // 数据输出引脚
            .din = din, // 数据输入引脚
            .invert_flags = {
                .mclk_inv = false, // MCLK不反转
                .bclk_inv = false, // BCLK不反转
                .ws_inv = false // WS不反转
            }
        }
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg)); // 初始化发送通道
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg)); // 初始化接收通道
    ESP_LOGI(TAG, "Duplex channels created"); // 日志：双工通道创建成功
}

// ATK双工模式构造函数，初始化I2S通道
ATK_NoAudioCodecDuplex::ATK_NoAudioCodecDuplex(int input_sample_rate, int output_sample_rate, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
    duplex_ = true; // 设置为双工模式
    input_sample_rate_ = input_sample_rate; // 输入采样率
    output_sample_rate_ = output_sample_rate; // 输出采样率

    // I2S通道配置
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0, // I2S端口号
        .role = I2S_ROLE_MASTER, // 主模式
        .dma_desc_num = 6, // DMA描述符数量
        .dma_frame_num = 240, // DMA帧数
        .auto_clear_after_cb = true, // 回调后自动清除
        .auto_clear_before_cb = false, // 回调前不自动清除
        .intr_priority = 0, // 中断优先级
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_)); // 创建I2S通道

    // I2S标准配置
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_, // 采样率
            .clk_src = I2S_CLK_SRC_DEFAULT, // 时钟源
            .mclk_multiple = I2S_MCLK_MULTIPLE_256, // MCLK倍数
			#ifdef   I2S_HW_VERSION_2    
				.ext_clk_freq_hz = 0, // 外部时钟频率
			#endif
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT, // 数据位宽
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, // 槽位宽
            .slot_mode = I2S_SLOT_MODE_STEREO, // 立体声模式
            .slot_mask = I2S_STD_SLOT_BOTH, // 左右声道
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT, // WS信号宽度
            .ws_pol = false, // WS信号极性
            .bit_shift = true, // 位偏移
            #ifdef   I2S_HW_VERSION_2   
                .left_align = true, // 左对齐
                .big_endian = false, // 大端模式
                .bit_order_lsb = false // LSB位序
            #endif
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, // MCLK引脚未使用
            .bclk = bclk, // BCLK引脚
            .ws = ws, // WS引脚
            .dout = dout, // 数据输出引脚
            .din = din, // 数据输入引脚
            .invert_flags = {
                .mclk_inv = false, // MCLK不反转
                .bclk_inv = false, // BCLK不反转
                .ws_inv = false // WS不反转
            }
        }
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg)); // 初始化发送通道
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg)); // 初始化接收通道
    ESP_LOGI(TAG, "Duplex channels created"); // 日志：双工通道创建成功
}

// 单工模式构造函数，初始化I2S通道
NoAudioCodecSimplex::NoAudioCodecSimplex(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din) {
    duplex_ = false; // 设置为单工模式
    input_sample_rate_ = input_sample_rate; // 输入采样率
    output_sample_rate_ = output_sample_rate; // 输出采样率

    // 创建扬声器通道
    i2s_chan_config_t chan_cfg = {
        .id = (i2s_port_t)0, // I2S端口号
        .role = I2S_ROLE_MASTER, // 主模式
        .dma_desc_num = 6, // DMA描述符数量
        .dma_frame_num = 240, // DMA帧数
        .auto_clear_after_cb = true, // 回调后自动清除
        .auto_clear_before_cb = false, // 回调前不自动清除
        .intr_priority = 0, // 中断优先级
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, nullptr)); // 创建I2S通道

    // I2S标准配置
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_, // 采样率
            .clk_src = I2S_CLK_SRC_DEFAULT, // 时钟源
            .mclk_multiple = I2S_MCLK_MULTIPLE_256, // MCLK倍数
			#ifdef   I2S_HW_VERSION_2    
				.ext_clk_freq_hz = 0, // 外部时钟频率
			#endif
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT, // 数据位宽
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, // 槽位宽
            .slot_mode = I2S_SLOT_MODE_MONO, // 单声道模式
            .slot_mask = I2S_STD_SLOT_LEFT, // 左声道
            .ws_width = I2S_DATA_BIT_WIDTH_32BIT, // WS信号宽度
            .ws_pol = false, // WS信号极性
            .bit_shift = true, // 位偏移
            #ifdef   I2S_HW_VERSION_2   
                .left_align = true, // 左对齐
                .big_endian = false, // 大端模式
                .bit_order_lsb = false // LSB位序
            #endif
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, // MCLK引脚未使用
            .bclk = spk_bclk, // BCLK引脚
            .ws = spk_ws, // WS引脚
            .dout = spk_dout, // 数据输出引脚
            .din = I2S_GPIO_UNUSED, // 数据输入引脚未使用
            .invert_flags = {
                .mclk_inv = false, // MCLK不反转
                .bclk_inv = false, // BCLK不反转
                .ws_inv = false // WS不反转
            }
        }
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg)); // 初始化发送通道

    // 创建麦克风通道
    chan_cfg.id = (i2s_port_t)1; // I2S端口号
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &rx_handle_)); // 创建I2S通道
    std_cfg.clk_cfg.sample_rate_hz = (uint32_t)input_sample_rate_; // 采样率
    std_cfg.gpio_cfg.bclk = mic_sck; // BCLK引脚
    std_cfg.gpio_cfg.ws = mic_ws; // WS引脚
    std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED; // 数据输出引脚未使用
    std_cfg.gpio_cfg.din = mic_din; // 数据输入引脚
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg)); // 初始化接收通道
    ESP_LOGI(TAG, "Simplex channels created"); // 日志：单工通道创建成功
}

// 单工模式构造函数，初始化I2S通道，支持自定义槽位掩码
NoAudioCodecSimplex::NoAudioCodecSimplex(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, i2s_std_slot_mask_t spk_slot_mask, gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din, i2s_std_slot_mask_t mic_slot_mask){
    duplex_ = false; // 设置为单工模式
    input_sample_rate_ = input_sample_rate; // 输入采样率
    output_sample_rate_ = output_sample_rate; // 输出采样率

    // 创建扬声器通道
    i2s_chan_config_t chan_cfg = {
        .id = (i2s_port_t)0, // I2S端口号
        .role = I2S_ROLE_MASTER, // 主模式
        .dma_desc_num = 6, // DMA描述符数量
        .dma_frame_num = 240, // DMA帧数
        .auto_clear_after_cb = true, // 回调后自动清除
        .auto_clear_before_cb = false, // 回调前不自动清除
        .intr_priority = 0, // 中断优先级
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, nullptr)); // 创建I2S通道

    // I2S标准配置
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_, // 采样率
            .clk_src = I2S_CLK_SRC_DEFAULT, // 时钟源
            .mclk_multiple = I2S_MCLK_MULTIPLE_256, // MCLK倍数
			#ifdef   I2S_HW_VERSION_2    
				.ext_clk_freq_hz = 0, // 外部时钟频率
			#endif
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT, // 数据位宽
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, // 槽位宽
            .slot_mode = I2S_SLOT_MODE_MONO, // 单声道模式
            .slot_mask = spk_slot_mask, // 自定义槽位掩码
            .ws_width = I2S_DATA_BIT_WIDTH_32BIT, // WS信号宽度
            .ws_pol = false, // WS信号极性
            .bit_shift = true, // 位偏移
            #ifdef   I2S_HW_VERSION_2   
                .left_align = true, // 左对齐
                .big_endian = false, // 大端模式
                .bit_order_lsb = false // LSB位序
            #endif
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, // MCLK引脚未使用
            .bclk = spk_bclk, // BCLK引脚
            .ws = spk_ws, // WS引脚
            .dout = spk_dout, // 数据输出引脚
            .din = I2S_GPIO_UNUSED, // 数据输入引脚未使用
            .invert_flags = {
                .mclk_inv = false, // MCLK不反转
                .bclk_inv = false, // BCLK不反转
                .ws_inv = false // WS不反转
            }
        }
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg)); // 初始化发送通道

    // 创建麦克风通道
    chan_cfg.id = (i2s_port_t)1; // I2S端口号
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &rx_handle_)); // 创建I2S通道
    std_cfg.clk_cfg.sample_rate_hz = (uint32_t)input_sample_rate_; // 采样率
    std_cfg.slot_cfg.slot_mask = mic_slot_mask; // 自定义槽位掩码
    std_cfg.gpio_cfg.bclk = mic_sck; // BCLK引脚
    std_cfg.gpio_cfg.ws = mic_ws; // WS引脚
    std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED; // 数据输出引脚未使用
    std_cfg.gpio_cfg.din = mic_din; // 数据输入引脚
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg)); // 初始化接收通道
    ESP_LOGI(TAG, "Simplex channels created"); // 日志：单工通道创建成功
}

// PDM单工模式构造函数，初始化I2S通道
NoAudioCodecSimplexPdm::NoAudioCodecSimplexPdm(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, gpio_num_t mic_sck, gpio_num_t mic_din) {
    duplex_ = false; // 设置为单工模式
    input_sample_rate_ = input_sample_rate; // 输入采样率
    output_sample_rate_ = output_sample_rate; // 输出采样率

    // 创建扬声器通道
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG((i2s_port_t)1, I2S_ROLE_MASTER); // 默认配置
    tx_chan_cfg.dma_desc_num = 6; // DMA描述符数量
    tx_chan_cfg.dma_frame_num = 240; // DMA帧数
    tx_chan_cfg.auto_clear_after_cb = true; // 回调后自动清除
    tx_chan_cfg.auto_clear_before_cb = false; // 回调前不自动清除
    tx_chan_cfg.intr_priority = 0; // 中断优先级
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_handle_, NULL)); // 创建I2S通道

    // I2S标准配置
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_, // 采样率
            .clk_src = I2S_CLK_SRC_DEFAULT, // 时钟源
            .mclk_multiple = I2S_MCLK_MULTIPLE_256, // MCLK倍数
			#ifdef   I2S_HW_VERSION_2    
				.ext_clk_freq_hz = 0, // 外部时钟频率
			#endif
        },
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO), // 默认槽位配置
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, // MCLK引脚未使用
            .bclk = spk_bclk, // BCLK引脚
            .ws = spk_ws, // WS引脚
            .dout = spk_dout, // 数据输出引脚
            .din = I2S_GPIO_UNUSED, // 数据输入引脚未使用
            .invert_flags = {
                .mclk_inv = false, // MCLK不反转
                .bclk_inv = false, // BCLK不反转
                .ws_inv   = false, // WS不反转
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &tx_std_cfg)); // 初始化发送通道

#if SOC_I2S_SUPPORTS_PDM_RX
    // 创建麦克风通道，PDM模式
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG((i2s_port_t)0, I2S_ROLE_MASTER); // 默认配置
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, NULL, &rx_handle_)); // 创建I2S通道
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG((uint32_t)input_sample_rate_), // 默认时钟配置
        /* PDM模式的数据位宽固定为16位 */
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), // 默认槽位配置
        .gpio_cfg = {
            .clk = mic_sck, // 时钟引脚
            .din = mic_din, // 数据输入引脚
            .invert_flags = {
                .clk_inv = false, // 时钟不反转
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle_, &pdm_rx_cfg)); // 初始化接收通道
#else
    ESP_LOGE(TAG, "PDM is not supported"); // 日志：不支持PDM模式
#endif
    ESP_LOGI(TAG, "Simplex channels created"); // 日志：单工通道创建成功
}

// 写入音频数据
int NoAudioCodec::Write(const int16_t* data, int samples) {
    std::vector<int32_t> buffer(samples); // 创建缓冲区

    // output_volume_: 0-100
    // volume_factor_: 0-65536
    int32_t volume_factor = pow(double(output_volume_) / 100.0, 2) * 65536; // 计算音量因子
    for (int i = 0; i < samples; i++) {
        int64_t temp = int64_t(data[i]) * volume_factor; // 使用 int64_t 进行乘法运算
        if (temp > INT32_MAX) {
            buffer[i] = INT32_MAX; // 限制最大值
        } else if (temp < INT32_MIN) {
            buffer[i] = INT32_MIN; // 限制最小值
        } else {
            buffer[i] = static_cast<int32_t>(temp); // 转换为int32_t
        }
    }

    size_t bytes_written;
    ESP_ERROR_CHECK(i2s_channel_write(tx_handle_, buffer.data(), samples * sizeof(int32_t), &bytes_written, portMAX_DELAY)); // 写入数据
    return bytes_written / sizeof(int32_t); // 返回写入的样本数
}

// 读取音频数据
int NoAudioCodec::Read(int16_t* dest, int samples) {
    size_t bytes_read;

    std::vector<int32_t> bit32_buffer(samples); // 创建缓冲区
    if (i2s_channel_read(rx_handle_, bit32_buffer.data(), samples * sizeof(int32_t), &bytes_read, portMAX_DELAY) != ESP_OK) {
        ESP_LOGE(TAG, "Read Failed!"); // 日志：读取失败
        return 0;
    }

    samples = bytes_read / sizeof(int32_t); // 计算样本数
    for (int i = 0; i < samples; i++) {
        int32_t value = bit32_buffer[i] >> 12; // 右移12位
        dest[i] = (value > INT16_MAX) ? INT16_MAX : (value < -INT16_MAX) ? -INT16_MAX : (int16_t)value; // 限制值范围
    }
    return samples; // 返回读取的样本数
}