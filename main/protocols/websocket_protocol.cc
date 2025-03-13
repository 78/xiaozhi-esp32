#include "websocket_protocol.h"
#include "board.h"
#include "system_info.h"
#include "application.h"

#include <cstring>
#include <cJSON.h>
#include <esp_log.h>
#include <arpa/inet.h>
#include "assets/lang_config.h"

#define TAG "WS"  // 定义日志标签

// WebsocketProtocol 构造函数
WebsocketProtocol::WebsocketProtocol() {
    event_group_handle_ = xEventGroupCreate();  // 创建一个事件组，用于任务间同步
}

// WebsocketProtocol 析构函数
WebsocketProtocol::~WebsocketProtocol() {
    if (websocket_ != nullptr) {
        delete websocket_;  // 删除 WebSocket 对象
    }
    vEventGroupDelete(event_group_handle_);  // 删除事件组
}

// 启动 WebSocket 协议
void WebsocketProtocol::Start() {
    // 目前为空，可能用于未来的初始化操作
}

// 发送音频数据
void WebsocketProtocol::SendAudio(const std::vector<uint8_t>& data) {
    if (websocket_ == nullptr) {
        return;  // 如果 WebSocket 对象为空，直接返回
    }

    websocket_->Send(data.data(), data.size(), true);  // 发送二进制音频数据
}

// 发送文本消息
void WebsocketProtocol::SendText(const std::string& text) {
    if (websocket_ == nullptr) {
        return;  // 如果 WebSocket 对象为空，直接返回
    }

    if (!websocket_->Send(text)) {  // 发送文本消息
        ESP_LOGE(TAG, "Failed to send text: %s", text.c_str());  // 如果发送失败，记录错误日志
        SetError(Lang::Strings::SERVER_ERROR);  // 设置错误信息
    }
}

// 检查音频通道是否已打开
bool WebsocketProtocol::IsAudioChannelOpened() const {
    return websocket_ != nullptr && websocket_->IsConnected() && !error_occurred_ && !IsTimeout();  // 如果 WebSocket 已连接且无错误且未超时，返回 true
}

// 关闭音频通道
void WebsocketProtocol::CloseAudioChannel() {
    if (websocket_ != nullptr) {
        delete websocket_;  // 删除 WebSocket 对象
        websocket_ = nullptr;
    }
}

// 打开音频通道
bool WebsocketProtocol::OpenAudioChannel() {
    if (websocket_ != nullptr) {
        delete websocket_;  // 如果 WebSocket 对象已存在，先删除
    }

    error_occurred_ = false;  // 重置错误标志
    std::string url = CONFIG_WEBSOCKET_URL;  // 获取 WebSocket 服务器 URL
    std::string token = "Bearer " + std::string(CONFIG_WEBSOCKET_ACCESS_TOKEN);  // 构建认证令牌
    websocket_ = Board::GetInstance().CreateWebSocket();  // 创建 WebSocket 对象
    websocket_->SetHeader("Authorization", token.c_str());  // 设置认证头
    websocket_->SetHeader("Protocol-Version", "1");  // 设置协议版本头
    websocket_->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());  // 设置设备 ID 头
    websocket_->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());  // 设置客户端 ID 头

    // 设置数据到达时的回调函数
    websocket_->OnData([this](const char* data, size_t len, bool binary) {
        if (binary) {
            if (on_incoming_audio_ != nullptr) {
                on_incoming_audio_(std::vector<uint8_t>((uint8_t*)data, (uint8_t*)data + len));  // 处理二进制音频数据
            }
        } else {
            // 解析 JSON 数据
            auto root = cJSON_Parse(data);  // 解析 JSON 数据
            auto type = cJSON_GetObjectItem(root, "type");  // 获取消息类型
            if (type != NULL) {
                if (strcmp(type->valuestring, "hello") == 0) {
                    ParseServerHello(root);  // 处理服务器 hello 消息
                } else {
                    if (on_incoming_json_ != nullptr) {
                        on_incoming_json_(root);  // 调用自定义的 JSON 消息处理函数
                    }
                }
            } else {
                ESP_LOGE(TAG, "Missing message type, data: %s", data);  // 如果消息类型缺失，记录错误日志
            }
            cJSON_Delete(root);  // 删除 JSON 对象
        }
        last_incoming_time_ = std::chrono::steady_clock::now();  // 更新最后接收消息的时间
    });

    // 设置断开连接时的回调函数
    websocket_->OnDisconnected([this]() {
        ESP_LOGI(TAG, "Websocket disconnected");  // 记录断开连接日志
        if (on_audio_channel_closed_ != nullptr) {
            on_audio_channel_closed_();  // 调用音频通道关闭回调函数
        }
    });

    if (!websocket_->Connect(url.c_str())) {  // 连接 WebSocket 服务器
        ESP_LOGE(TAG, "Failed to connect to websocket server");  // 如果连接失败，记录错误日志
        SetError(Lang::Strings::SERVER_NOT_FOUND);  // 设置错误信息
        return false;
    }

    // 发送 hello 消息描述客户端信息
    std::string message = "{";
    message += "\"type\":\"hello\",";  // 消息类型
    message += "\"version\": 1,";  // 协议版本
    message += "\"transport\":\"websocket\",";  // 传输方式
    message += "\"audio_params\":{";  // 音频参数
    message += "\"format\":\"opus\", \"sample_rate\":16000, \"channels\":1, \"frame_duration\":" + std::to_string(OPUS_FRAME_DURATION_MS);
    message += "}}";
    websocket_->Send(message);  // 发送 hello 消息

    // 等待服务器 hello 响应
    EventBits_t bits = xEventGroupWaitBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT)) {
        ESP_LOGE(TAG, "Failed to receive server hello");  // 如果未收到服务器 hello 消息，记录错误日志
        SetError(Lang::Strings::SERVER_TIMEOUT);  // 设置错误信息
        return false;
    }

    if (on_audio_channel_opened_ != nullptr) {
        on_audio_channel_opened_();  // 调用音频通道打开回调函数
    }

    return true;
}

// 解析服务器 hello 消息
void WebsocketProtocol::ParseServerHello(const cJSON* root) {
    auto transport = cJSON_GetObjectItem(root, "transport");  // 获取传输方式
    if (transport == nullptr || strcmp(transport->valuestring, "websocket") != 0) {
        ESP_LOGE(TAG, "Unsupported transport: %s", transport->valuestring);  // 如果不支持该传输方式，记录错误日志
        return;
    }

    auto audio_params = cJSON_GetObjectItem(root, "audio_params");  // 获取音频参数
    if (audio_params != NULL) {
        auto sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");  // 获取采样率
        if (sample_rate != NULL) {
            server_sample_rate_ = sample_rate->valueint;  // 设置服务器采样率
        }
    }

    xEventGroupSetBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT);  // 设置 SERVER_HELLO 事件
}