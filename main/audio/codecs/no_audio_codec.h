/*
 * @LastEditors: T sir
 * @LastEditTime: 2026-05-14 09:31:19
 */
#ifndef _NO_AUDIO_CODEC_H
#define _NO_AUDIO_CODEC_H

#include "audio_codec.h"

#include <driver/gpio.h>
#include <driver/i2s_pdm.h>
#include <mutex>

class NoAudioCodec : public AudioCodec {
protected:
    std::mutex data_if_mutex_;

    virtual int Write(const int16_t* data, int samples) override;
    virtual int Read(int16_t* dest, int samples) override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;

public:
    virtual ~NoAudioCodec();
};

class NoAudioCodecDuplex : public NoAudioCodec {
public:
    NoAudioCodecDuplex(int input_sample_rate, int output_sample_rate, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din);
};

class NoAudioCodecSimplex : public NoAudioCodec {
public:
    NoAudioCodecSimplex(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din);
    NoAudioCodecSimplex(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, i2s_std_slot_mask_t spk_slot_mask, gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din, i2s_std_slot_mask_t mic_slot_mask);
};

/**
 * @brief 简单单工 PDM 音频编解码器类
 * @note 用于处理通过 PDM (脉冲密度调制) 接口输入的麦克风数据，以及通过 I2S 标准接口输出的扬声器数据。
 *       "Simplex" 表示输入和输出使用不同的物理通道或时序，通常用于麦克风是 PDM 接口而扬声器是 I2S 接口的场景。
 */
class NoAudioCodecSimplexPdm : public NoAudioCodec {
public:
    /**
     * @brief 构造函数 (默认槽位掩码)
     * @param input_sample_rate 麦克风输入采样率 (Hz)
     * @param output_sample_rate 扬声器输出采样率 (Hz)
     * @param spk_bclk 扬声器 I2S 位时钟 (BCLK) GPIO 引脚
     * @param spk_ws 扬声器 I2S 字选择 (WS/LRCLK) GPIO 引脚
     * @param spk_dout 扬声器 I2S 数据输出 (DOUT) GPIO 引脚
     * @param mic_sck 麦克风 PDM 时钟 (SCK) GPIO 引脚
     * @param mic_din 麦克风 PDM 数据输入 (DIN) GPIO 引脚
     */
    NoAudioCodecSimplexPdm(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, gpio_num_t mic_sck,  gpio_num_t mic_din);

    /**
     * @brief 构造函数 (自定义扬声器槽位掩码)
     * @param input_sample_rate 麦克风输入采样率 (Hz)
     * @param output_sample_rate 扬声器输出采样率 (Hz)
     * @param spk_bclk 扬声器 I2S 位时钟 (BCLK) GPIO 引脚
     * @param spk_ws 扬声器 I2S 字选择 (WS/LRCLK) GPIO 引脚
     * @param spk_dout 扬声器 I2S 数据输出 (DOUT) GPIO 引脚
     * @param spk_slot_mask 扬声器 I2S 槽位掩码 (例如: I2S_STD_SLOT_LEFT, I2S_STD_SLOT_RIGHT, I2S_STD_SLOT_BOTH)
     * @param mic_sck 麦克风 PDM 时钟 (SCK) GPIO 引脚
     * @param mic_din 麦克风 PDM 数据输入 (DIN) GPIO 引脚
     */
    NoAudioCodecSimplexPdm(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, i2s_std_slot_mask_t spk_slot_mask, gpio_num_t mic_sck,  gpio_num_t mic_din);

    /**
     * @brief 读取麦克风音频数据
     * @param dest 目标缓冲区指针，用于存储读取到的 PCM 数据
     * @param samples 需要读取的采样点数量
     * @return 实际读取的采样点数量，失败返回 -1 或错误码
     * @note 此函数重写了基类的 Read 方法，专门处理 PDM 麦克风的數據读取。
     */
    int Read(int16_t* dest, int samples);
};

#endif // _NO_AUDIO_CODEC_H
