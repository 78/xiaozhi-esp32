#include "audio_codec.h"
#include "board.h"
#include "settings.h"

#include <esp_log.h>
#include <cstring>
#include <driver/i2s_common.h>

#define TAG "AudioCodec"

AudioCodec::AudioCodec() {
}

AudioCodec::~AudioCodec() {
}
// 定义一个名为 OnInputReady 的成员函数，用于设置输入准备好时的回调函数
// 当音频输入准备好时，会调用传入的回调函数
void AudioCodec::OnInputReady(std::function<bool()> callback) {
    // 将传入的回调函数赋值给成员变量 on_input_ready_
    on_input_ready_ = callback;
}

// 定义一个名为 OnOutputReady 的成员函数，用于设置输出准备好时的回调函数
// 当音频输出准备好时，会调用传入的回调函数
void AudioCodec::OnOutputReady(std::function<bool()> callback) {
    // 将传入的回调函数赋值给成员变量 on_output_ready_
    on_output_ready_ = callback;
}

// 定义一个名为 OutputData 的成员函数，用于输出音频数据
// 参数 data 是一个存储音频数据的向量
void AudioCodec::OutputData(std::vector<int16_t>& data) {
    // 调用 Write 函数将音频数据写入 I2S 接口
    Write(data.data(), data.size());
}

// 定义一个名为 InputData 的成员函数，用于读取音频输入数据
// 参数 data 是一个存储音频数据的向量，用于接收读取到的数据
bool AudioCodec::InputData(std::vector<int16_t>& data) {
    // 定义音频输入的时长为 30 毫秒
    int duration = 30;
    // 计算输入音频帧的大小
    int input_frame_size = input_sample_rate_ / 1000 * duration * input_channels_;

    // 调整 data 向量的大小以适应输入音频帧的大小
    data.resize(input_frame_size);
    // 调用 Read 函数从 I2S 接口读取音频数据
    int samples = Read(data.data(), data.size());
    // 如果读取到的样本数大于 0，则表示读取成功
    if (samples > 0) {
        return true;
    }
    // 否则，表示读取失败
    return false;
}

// 定义一个名为 on_sent 的静态成员函数，作为 I2S 发送完成事件的回调函数
// IRAM_ATTR 表示该函数应放在内部 RAM 中执行，以提高执行速度
// 参数 handle 是 I2S 通道句柄，event 是 I2S 事件数据，user_ctx 是用户上下文指针
IRAM_ATTR bool AudioCodec::on_sent(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    // 将用户上下文指针转换为 AudioCodec 指针
    auto audio_codec = (AudioCodec*)user_ctx;
    // 检查输出是否启用且输出准备好的回调函数是否存在
    if (audio_codec->output_enabled_ && audio_codec->on_output_ready_) {
        // 调用输出准备好的回调函数
        return audio_codec->on_output_ready_();
    }
    // 否则，返回 false
    return false;
}

// 定义一个名为 on_recv 的静态成员函数，作为 I2S 接收完成事件的回调函数
// IRAM_ATTR 表示该函数应放在内部 RAM 中执行，以提高执行速度
// 参数 handle 是 I2S 通道句柄，event 是 I2S 事件数据，user_ctx 是用户上下文指针
IRAM_ATTR bool AudioCodec::on_recv(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
    // 将用户上下文指针转换为 AudioCodec 指针
    auto audio_codec = (AudioCodec*)user_ctx;
    // 检查输入是否启用且输入准备好的回调函数是否存在
    if (audio_codec->input_enabled_ && audio_codec->on_input_ready_) {
        // 调用输入准备好的回调函数
        return audio_codec->on_input_ready_();
    }
    // 否则，返回 false
    return false;
}

// 定义一个名为 Start 的成员函数，用于启动音频编解码功能
void AudioCodec::Start() {
    // 创建一个名为 "audio" 的设置对象，不自动保存设置
    Settings settings("audio", false);
    // 从设置中获取输出音量，如果设置中不存在，则使用默认的输出音量
    output_volume_ = settings.GetInt("output_volume", output_volume_);

    // 注册音频数据接收回调
    i2s_event_callbacks_t rx_callbacks = {};
    // 将 on_recv 函数作为接收完成事件的回调函数
    rx_callbacks.on_recv = on_recv;
    // 注册接收回调函数到 I2S 接收通道
    i2s_channel_register_event_callback(rx_handle_, &rx_callbacks, this);

    // 注册音频数据发送回调
    i2s_event_callbacks_t tx_callbacks = {};
    // 将 on_sent 函数作为发送完成事件的回调函数
    tx_callbacks.on_sent = on_sent;
    // 注册发送回调函数到 I2S 发送通道
    i2s_channel_register_event_callback(tx_handle_, &tx_callbacks, this);

    // 启用 I2S 发送通道
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    // 启用 I2S 接收通道
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));

    // 启用音频输入
    EnableInput(true);
    // 启用音频输出
    EnableOutput(true);
}

// 定义一个名为 SetOutputVolume 的成员函数，用于设置音频输出音量
// 参数 volume 是要设置的音量值
void AudioCodec::SetOutputVolume(int volume) {
    // 将传入的音量值赋值给成员变量 output_volume_
    output_volume_ = volume;
    // 记录日志，显示设置的输出音量
    ESP_LOGI(TAG, "Set output volume to %d", output_volume_);
    
    // 创建一个名为 "audio" 的设置对象，自动保存设置
    Settings settings("audio", true);
    // 将输出音量保存到设置中
    settings.SetInt("output_volume", output_volume_);
}

// 定义一个名为 EnableInput 的成员函数，用于启用或禁用音频输入
// 参数 enable 为 true 时启用输入，为 false 时禁用输入
void AudioCodec::EnableInput(bool enable) {
    // 检查输入状态是否已经是要设置的状态，如果是则直接返回
    if (enable == input_enabled_) {
        return;
    }
    // 更新输入启用状态
    input_enabled_ = enable;
    // 记录日志，显示输入启用状态的变化
    ESP_LOGI(TAG, "Set input enable to %s", enable ? "true" : "false");
}

// 定义一个名为 EnableOutput 的成员函数，用于启用或禁用音频输出
// 参数 enable 为 true 时启用输出，为 false 时禁用输出
void AudioCodec::EnableOutput(bool enable) {
    // 检查输出状态是否已经是要设置的状态，如果是则直接返回
    if (enable == output_enabled_) {
        return;
    }
    // 更新输出启用状态
    output_enabled_ = enable;
    // 记录日志，显示输出启用状态的变化
    ESP_LOGI(TAG, "Set output enable to %s", enable ? "true" : "false");
}