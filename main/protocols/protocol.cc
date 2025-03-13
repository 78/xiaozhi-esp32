#include "protocol.h"

#include <esp_log.h>

#define TAG "Protocol"  // 定义日志标签

// 设置 JSON 消息到达时的回调函数
void Protocol::OnIncomingJson(std::function<void(const cJSON* root)> callback) {
    on_incoming_json_ = callback;  // 将传入的回调函数赋值给 on_incoming_json_
}

// 设置音频数据到达时的回调函数
void Protocol::OnIncomingAudio(std::function<void(std::vector<uint8_t>&& data)> callback) {
    on_incoming_audio_ = callback;  // 将传入的回调函数赋值给 on_incoming_audio_
}

// 设置音频通道打开时的回调函数
void Protocol::OnAudioChannelOpened(std::function<void()> callback) {
    on_audio_channel_opened_ = callback;  // 将传入的回调函数赋值给 on_audio_channel_opened_
}

// 设置音频通道关闭时的回调函数
void Protocol::OnAudioChannelClosed(std::function<void()> callback) {
    on_audio_channel_closed_ = callback;  // 将传入的回调函数赋值给 on_audio_channel_closed_
}

// 设置网络错误发生时的回调函数
void Protocol::OnNetworkError(std::function<void(const std::string& message)> callback) {
    on_network_error_ = callback;  // 将传入的回调函数赋值给 on_network_error_
}

// 设置错误信息并触发网络错误回调
void Protocol::SetError(const std::string& message) {
    error_occurred_ = true;  // 标记错误发生
    if (on_network_error_ != nullptr) {
        on_network_error_(message);  // 调用网络错误回调函数
    }
}

// 发送中止说话的消息
void Protocol::SendAbortSpeaking(AbortReason reason) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"abort\"";  // 构建中止消息的基本结构
    if (reason == kAbortReasonWakeWordDetected) {
        message += ",\"reason\":\"wake_word_detected\"";  // 如果中止原因是唤醒词检测到，添加原因字段
    }
    message += "}";
    SendText(message);  // 发送消息
}

// 发送唤醒词检测到的消息
void Protocol::SendWakeWordDetected(const std::string& wake_word) {
    std::string json = "{\"session_id\":\"" + session_id_ + 
                      "\",\"type\":\"listen\",\"state\":\"detect\",\"text\":\"" + wake_word + "\"}";  // 构建唤醒词检测消息
    SendText(json);  // 发送消息
}

// 发送开始监听的消息
void Protocol::SendStartListening(ListeningMode mode) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\"";  // 构建开始监听消息的基本结构
    message += ",\"type\":\"listen\",\"state\":\"start\"";  // 添加类型和状态字段
    if (mode == kListeningModeAlwaysOn) {
        message += ",\"mode\":\"realtime\"";  // 如果监听模式是始终开启，添加模式字段
    } else if (mode == kListeningModeAutoStop) {
        message += ",\"mode\":\"auto\"";  // 如果监听模式是自动停止，添加模式字段
    } else {
        message += ",\"mode\":\"manual\"";  // 如果监听模式是手动，添加模式字段
    }
    message += "}";
    SendText(message);  // 发送消息
}

// 发送停止监听的消息
void Protocol::SendStopListening() {
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"listen\",\"state\":\"stop\"}";  // 构建停止监听消息
    SendText(message);  // 发送消息
}

// 发送 IoT 描述符的消息
void Protocol::SendIotDescriptors(const std::string& descriptors) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"iot\",\"descriptors\":" + descriptors + "}";  // 构建 IoT 描述符消息
    SendText(message);  // 发送消息
}

// 发送 IoT 状态的消息
void Protocol::SendIotStates(const std::string& states) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"iot\",\"states\":" + states + "}";  // 构建 IoT 状态消息
    SendText(message);  // 发送消息
}

// 检查是否超时
bool Protocol::IsTimeout() const {
    const int kTimeoutSeconds = 120;  // 定义超时时间为 120 秒
    auto now = std::chrono::steady_clock::now();  // 获取当前时间
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_incoming_time_);  // 计算距离上次接收消息的时间间隔
    bool timeout = duration.count() > kTimeoutSeconds;  // 判断是否超时
    if (timeout) {
        ESP_LOGE(TAG, "Channel timeout %lld seconds", duration.count());  // 如果超时，记录错误日志
    }
    return timeout;  // 返回是否超时
}