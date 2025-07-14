#include "websocket_protocol.h"
#include "board.h"
#include "system_info.h"
#include "application.h"
#include "ota.h"

#include <cstring>
#include <cJSON.h>
#include <esp_log.h>
#include <arpa/inet.h>
#include "assets/lang_config.h"

#define TAG "WS"

WebsocketProtocol::WebsocketProtocol() {
    event_group_handle_ = xEventGroupCreate();
}

WebsocketProtocol::~WebsocketProtocol() {
    if (websocket_ != nullptr) {
        delete websocket_;
    }
    vEventGroupDelete(event_group_handle_);
}

void WebsocketProtocol::Start() {
}

void WebsocketProtocol::SendAudio(const std::vector<uint8_t>& data) {
    if (websocket_ == nullptr) {
        return;
    }

    websocket_->Send(data.data(), data.size(), true);
}

bool WebsocketProtocol::SendText(const std::string& text) {
    if (websocket_ == nullptr) {
        return false;
    }

    if (!websocket_->Send(text)) {
        ESP_LOGE(TAG, "Failed to send text: %s", text.c_str());
        SetError(Lang::Strings::SERVER_ERROR);
        return false;
    }

    return true;
}

bool WebsocketProtocol::IsAudioChannelOpened() const {
    return websocket_ != nullptr && websocket_->IsConnected() && !error_occurred_ && !IsTimeout();
}

void WebsocketProtocol::CloseAudioChannel() {
    if (websocket_ != nullptr) {
        delete websocket_;
        websocket_ = nullptr;
    }
}

bool WebsocketProtocol::OpenAudioChannel() {
    ESP_LOGI(TAG, "🔓 开始打开WebSocket音频通道...");
    if (websocket_ != nullptr) {
        delete websocket_;
    }

    error_occurred_ = false;
    
    // 从OTA获取websocket配置
    auto& application = Application::GetInstance();
    auto& ota = application.GetOta();
    
    std::string url;
    std::string token;
    
    if (ota.HasWebsocketConfig()) {
        url = ota.GetWebsocketUrl();
        token = "Bearer " + ota.GetWebsocketToken();
        ESP_LOGI(TAG, "Using websocket config from OTA: %s", url.c_str());
    } else {
        // 如果OTA中没有配置，则报错，因为现在必须从OTA获取配置
        ESP_LOGE(TAG, "No websocket config found in OTA response");
        SetError(Lang::Strings::SERVER_ERROR);
        return false;
    }
    
    websocket_ = Board::GetInstance().CreateWebSocket();
    websocket_->SetHeader("Authorization", token.c_str());
    websocket_->SetHeader("Protocol-Version", "1");
    websocket_->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    
    // 尝试获取存储的Client-Id
    std::string client_id = SystemInfo::GetClientId();
    
    // 只使用NVS中存储的Client-Id，如果没有则报错
    if (client_id.empty()) {
        ESP_LOGE(TAG, "No Client-Id found in NVS, WebSocket connection cannot be established");
        SetError(Lang::Strings::SERVER_ERROR);
        return false;
    }
    
    ESP_LOGI(TAG, "Using Client-Id for WebSocket connection: %s", client_id.c_str());
    websocket_->SetHeader("Client-Id", client_id.c_str());

    websocket_->OnData([this](const char* data, size_t len, bool binary) {
        if (binary) {
            if (on_incoming_audio_ != nullptr) {
                on_incoming_audio_(std::vector<uint8_t>((uint8_t*)data, (uint8_t*)data + len));
            }
        } else {
            // Parse JSON data - 修复：创建null终止的字符串
            std::string json_string(data, len);  // 安全地创建字符串副本
            auto root = cJSON_Parse(json_string.c_str());
            auto type = cJSON_GetObjectItem(root, "type");
            if (type != NULL) {
                if (strcmp(type->valuestring, "hello") == 0) {
                    ParseServerHello(root);
                } else {
                    if (on_incoming_json_ != nullptr) {
                        on_incoming_json_(root);
                    }
                }
            } else {
                // 兼容性处理：检查是否是裸IoT命令（缺少type字段但有name、method、parameters）
                auto name = cJSON_GetObjectItem(root, "name");
                auto method = cJSON_GetObjectItem(root, "method");
                auto parameters = cJSON_GetObjectItem(root, "parameters");
                
                if (name != NULL && method != NULL && parameters != NULL) {
                    ESP_LOGI(TAG, "检测到裸IoT命令，自动包装为标准格式");
                    
                    // 创建标准的IoT消息格式
                    cJSON* wrapped_root = cJSON_CreateObject();
                    cJSON_AddStringToObject(wrapped_root, "type", "iot");
                    
                    cJSON* commands_array = cJSON_CreateArray();
                    cJSON_AddItemToArray(commands_array, cJSON_Duplicate(root, 1));
                    cJSON_AddItemToObject(wrapped_root, "commands", commands_array);
                    
                    if (on_incoming_json_ != nullptr) {
                        on_incoming_json_(wrapped_root);
                    }
                    
                    cJSON_Delete(wrapped_root);
                } else {
                    ESP_LOGE(TAG, "Missing message type, data: %s", json_string.c_str());
                }
            }
            cJSON_Delete(root);
        }
        last_incoming_time_ = std::chrono::steady_clock::now();
    });

    websocket_->OnDisconnected([this]() {
        ESP_LOGI(TAG, "Websocket disconnected");
        if (on_audio_channel_closed_ != nullptr) {
            on_audio_channel_closed_();
        }
    });

    if (!websocket_->Connect(url.c_str())) {
        ESP_LOGE(TAG, "Failed to connect to websocket server");
        SetError(Lang::Strings::SERVER_NOT_FOUND);
        return false;
    }

    // Send hello message to describe the client
    // keys: message type, version, audio_params (format, sample_rate, channels)
    std::string message = "{";
    message += "\"type\":\"hello\",";
    message += "\"version\": 1,";
    message += "\"transport\":\"websocket\",";
    message += "\"audio_params\":{";
    message += "\"format\":\"opus\", \"sample_rate\":24000, \"channels\":1, \"frame_duration\":" + std::to_string(OPUS_FRAME_DURATION_MS);
    message += "}}";
    ESP_LOGI(TAG, "📤 发送WebSocket Hello消息: %s", message.c_str());
    if (!SendText(message)) {
        return false;
    }

    // Wait for server hello
    EventBits_t bits = xEventGroupWaitBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT)) {
        ESP_LOGE(TAG, "Failed to receive server hello");
        SetError(Lang::Strings::SERVER_TIMEOUT);
        return false;
    }

    if (on_audio_channel_opened_ != nullptr) {
        on_audio_channel_opened_();
    }

    return true;
}

void WebsocketProtocol::ParseServerHello(const cJSON* root) {
    ESP_LOGI(TAG, "📥 收到WebSocket服务器Hello响应");
    
    auto transport = cJSON_GetObjectItem(root, "transport");
    if (transport == nullptr || strcmp(transport->valuestring, "websocket") != 0) {
        ESP_LOGE(TAG, "Unsupported transport: %s", transport->valuestring);
        return;
    }

    auto audio_params = cJSON_GetObjectItem(root, "audio_params");
    if (audio_params != NULL) {
        auto sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        if (sample_rate != NULL) {
            server_sample_rate_ = sample_rate->valueint;
        }
        auto frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
        if (frame_duration != NULL) {
            server_frame_duration_ = frame_duration->valueint;
        }
        ESP_LOGI(TAG, "🎵 WebSocket服务器音频参数: [采样率:%d, 帧长度:%dms]", 
                 server_sample_rate_, server_frame_duration_);
    } else {
        ESP_LOGW(TAG, "⚠️  服务器Hello响应中没有音频参数，使用默认值");
    }

    xEventGroupSetBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT);
}
