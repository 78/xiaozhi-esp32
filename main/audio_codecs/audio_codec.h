#ifndef _AUDIO_CODEC_H
#define _AUDIO_CODEC_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <driver/i2s_std.h>

#include <vector>
#include <string>
#include <functional>

#include "board.h"

// 定义音频编解码器的DMA描述符数量
#define AUDIO_CODEC_DMA_DESC_NUM 6
// 定义音频编解码器的DMA帧数
#define AUDIO_CODEC_DMA_FRAME_NUM 240

/**
 * 音频编解码器类，用于管理音频输入输出的配置和数据处理
 */
class AudioCodec {
public:
    // 构造函数
    AudioCodec();
    // 虚析构函数，确保派生类对象可以正确释放资源
    virtual ~AudioCodec();
    
    // 设置输出音量
    virtual void SetOutputVolume(int volume);
    // 控制输入使能
    virtual void EnableInput(bool enable);
    // 控制输出使能
    virtual void EnableOutput(bool enable);

    // 输出音频数据
    virtual void OutputData(std::vector<int16_t>& data);
    // 输入音频数据
    virtual bool InputData(std::vector<int16_t>& data);
    // 启动音频编解码器
    virtual void Start();

    // 以下为一组内联函数，用于获取音频编解码器的状态和配置信息
        // 返回当前设备是否处于全双工模式
    inline bool duplex() const { return duplex_; }
    
    // 返回输入参考信号的状态
    inline bool input_reference() const { return input_reference_; }
    
    // 返回输入采样率
    inline int input_sample_rate() const { return input_sample_rate_; }
    
    // 返回输出采样率
    inline int output_sample_rate() const { return output_sample_rate_; }
    
    // 返回输入声道数
    inline int input_channels() const { return input_channels_; }
    
    // 返回输出声道数
    inline int output_channels() const { return output_channels_; }
    
    // 返回输出音量
    inline int output_volume() const { return output_volume_; }
    
    // 返回输入启用状态
    inline bool input_enabled() const { return input_enabled_; }
    
    // 返回输出启用状态
    inline bool output_enabled() const { return output_enabled_; }

protected:
    // I2S传输通道句柄
    i2s_chan_handle_t tx_handle_ = nullptr;
    // I2S接收通道句柄
    i2s_chan_handle_t rx_handle_ = nullptr;

    // 全双工模式标志
    bool duplex_ = false;
    // 输入参考标志
    bool input_reference_ = false;
    // 输入使能状态
    bool input_enabled_ = false;
    // 输出使能状态
    bool output_enabled_ = false;
    // 输入采样率
    int input_sample_rate_ = 0;
    // 输出采样率
    int output_sample_rate_ = 0;
    // 输入声道数
    int input_channels_ = 1;
    // 输出声道数
    int output_channels_ = 1;
    // 输出音量
    int output_volume_ = 70;

    // 读取数据的纯虚函数，由派生类实现具体逻辑
    virtual int Read(int16_t* dest, int samples) = 0;
    // 写入数据的纯虚函数，由派生类实现具体逻辑
    virtual int Write(const int16_t* data, int samples) = 0;
};

#endif // _AUDIO_CODEC_H
