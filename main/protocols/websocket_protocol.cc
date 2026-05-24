#include "websocket_protocol.h"
#include "board.h"
#include "system_info.h"
#include "application.h"
#include "settings.h"

#include <cstring>
#include <algorithm>
#include <cJSON.h>
#include <esp_log.h>
#include <arpa/inet.h>
#include "assets/lang_config.h"

#define TAG "WS"

WebsocketProtocol::WebsocketProtocol() {
    event_group_handle_ = xEventGroupCreate();

    // Ping timer: 周期触发, 把 SendPing 调度到 main task (避免与析构竞争 websocket_)
    esp_timer_create_args_t ping_args = {
        .callback = [](void* arg) {
            auto* self = static_cast<WebsocketProtocol*>(arg);
            auto alive = self->alive_;
            Application::GetInstance().Schedule([self, alive]() {
                if (!*alive) return;
                self->SendPing();
            });
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ws_ping",
        .skip_unhandled_events = true
    };
    esp_timer_create(&ping_args, &ping_timer_);

    // Reconnect timer: one-shot, 触发时把 ConnectAndHello 调度到 main task
    esp_timer_create_args_t reconnect_args = {
        .callback = [](void* arg) {
            auto* self = static_cast<WebsocketProtocol*>(arg);
            auto alive = self->alive_;
            Application::GetInstance().Schedule([self, alive]() {
                if (!*alive) return;
                ESP_LOGI(TAG, "Reconnecting to websocket server");
                self->ConnectAndHello();
            });
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ws_reconnect",
        .skip_unhandled_events = true
    };
    esp_timer_create(&reconnect_args, &reconnect_timer_);
}

WebsocketProtocol::~WebsocketProtocol() {
    // 先 mark dead, 让 timer / scheduled callback 直接 return
    *alive_ = false;

    if (ping_timer_ != nullptr) {
        esp_timer_stop(ping_timer_);
        esp_timer_delete(ping_timer_);
    }
    if (reconnect_timer_ != nullptr) {
        esp_timer_stop(reconnect_timer_);
        esp_timer_delete(reconnect_timer_);
    }
    {
        std::lock_guard<std::mutex> lock(websocket_mutex_);
        websocket_.reset();
    }
    vEventGroupDelete(event_group_handle_);
}

bool WebsocketProtocol::Start() {
    // 启动即建立长连接 (替代旧的 "用时再连" 行为, 支持服务器主动下发)
    return ConnectAndHello();
}

bool WebsocketProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    std::lock_guard<std::mutex> lock(websocket_mutex_);
    if (websocket_ == nullptr || !websocket_->IsConnected()) {
        return false;
    }

    if (version_ == 2) {
        std::string serialized;
        serialized.resize(sizeof(BinaryProtocol2) + packet->payload.size());
        auto bp2 = (BinaryProtocol2*)serialized.data();
        bp2->version = htons(version_);
        bp2->type = 0;
        bp2->reserved = 0;
        bp2->timestamp = htonl(packet->timestamp);
        bp2->payload_size = htonl(packet->payload.size());
        memcpy(bp2->payload, packet->payload.data(), packet->payload.size());

        return websocket_->Send(serialized.data(), serialized.size(), true);
    } else if (version_ == 3) {
        std::string serialized;
        serialized.resize(sizeof(BinaryProtocol3) + packet->payload.size());
        auto bp3 = (BinaryProtocol3*)serialized.data();
        bp3->type = 0;
        bp3->reserved = 0;
        bp3->payload_size = htons(packet->payload.size());
        memcpy(bp3->payload, packet->payload.data(), packet->payload.size());

        return websocket_->Send(serialized.data(), serialized.size(), true);
    } else {
        return websocket_->Send(packet->payload.data(), packet->payload.size(), true);
    }
}

bool WebsocketProtocol::SendText(const std::string& text) {
    std::lock_guard<std::mutex> lock(websocket_mutex_);
    if (websocket_ == nullptr || !websocket_->IsConnected()) {
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
    // 仅指 audio session 是否激活, 不等于 WS 长连接是否在
    return audio_session_active_.load() && !error_occurred_ && !IsTimeout();
}

void WebsocketProtocol::CloseAudioChannel(bool send_goodbye) {
    (void)send_goodbye;  // Websocket doesn't need to send goodbye message
    // 仅结束 audio session, 不断 WS 长连接 (让 server push 仍可用)
    bool was_active = audio_session_active_.exchange(false);
    if (was_active && on_audio_channel_closed_ != nullptr) {
        on_audio_channel_closed_();
    }
}

bool WebsocketProtocol::OpenAudioChannel() {
    bool ws_alive;
    {
        std::lock_guard<std::mutex> lock(websocket_mutex_);
        ws_alive = (websocket_ != nullptr && websocket_->IsConnected());
    }
    // 长连接掉了 (例如重连退避中), 同步建一次
    if (!ws_alive) {
        if (!ConnectAndHello()) {
            return false;
        }
    }
    audio_session_active_.store(true);
    if (on_audio_channel_opened_ != nullptr) {
        on_audio_channel_opened_();
    }
    return true;
}

bool WebsocketProtocol::ConnectAndHello() {
    Settings settings("websocket", false);
    std::string url = settings.GetString("url");
    std::string token = settings.GetString("token");
    int version = settings.GetInt("version");
    if (version != 0) {
        version_ = version;
    }

    error_occurred_ = false;

    auto network = Board::GetInstance().GetNetwork();
    auto new_ws = network->CreateWebSocket(1);
    if (new_ws == nullptr) {
        ESP_LOGE(TAG, "Failed to create websocket");
        ScheduleReconnect();
        return false;
    }

    if (!token.empty()) {
        // If token not has a space, add "Bearer " prefix
        if (token.find(" ") == std::string::npos) {
            token = "Bearer " + token;
        }
        new_ws->SetHeader("Authorization", token.c_str());
    }
    new_ws->SetHeader("Protocol-Version", std::to_string(version_).c_str());
    new_ws->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    new_ws->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());

    // 把新 ws 安装到成员变量 (旧的会析构)
    {
        std::lock_guard<std::mutex> lock(websocket_mutex_);
        websocket_ = std::move(new_ws);
    }

    websocket_->OnData([this](const char* data, size_t len, bool binary) {
        if (binary) {
            if (on_incoming_audio_ != nullptr) {
                if (version_ == 2) {
                    BinaryProtocol2* bp2 = (BinaryProtocol2*)data;
                    bp2->version = ntohs(bp2->version);
                    bp2->type = ntohs(bp2->type);
                    bp2->timestamp = ntohl(bp2->timestamp);
                    bp2->payload_size = ntohl(bp2->payload_size);
                    auto payload = (uint8_t*)bp2->payload;
                    on_incoming_audio_(std::make_unique<AudioStreamPacket>(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = bp2->timestamp,
                        .payload = std::vector<uint8_t>(payload, payload + bp2->payload_size)
                    }));
                } else if (version_ == 3) {
                    BinaryProtocol3* bp3 = (BinaryProtocol3*)data;
                    bp3->type = bp3->type;
                    bp3->payload_size = ntohs(bp3->payload_size);
                    auto payload = (uint8_t*)bp3->payload;
                    on_incoming_audio_(std::make_unique<AudioStreamPacket>(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = 0,
                        .payload = std::vector<uint8_t>(payload, payload + bp3->payload_size)
                    }));
                } else {
                    on_incoming_audio_(std::make_unique<AudioStreamPacket>(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = 0,
                        .payload = std::vector<uint8_t>((uint8_t*)data, (uint8_t*)data + len)
                    }));
                }
            }
        } else {
            // Parse JSON data
            auto root = cJSON_ParseWithLength(data, len);
            auto type = cJSON_GetObjectItem(root, "type");
            if (cJSON_IsString(type)) {
                if (strcmp(type->valuestring, "hello") == 0) {
                    ParseServerHello(root);
                } else if (strcmp(type->valuestring, "pong") == 0) {
                    last_pong_time_us_.store(esp_timer_get_time());
                } else {
                    if (on_incoming_json_ != nullptr) {
                        on_incoming_json_(root);
                    }
                }
            } else {
                ESP_LOGE(TAG, "Missing message type, data: %s", std::string(data, len).c_str());
            }
            cJSON_Delete(root);
        }
        last_incoming_time_ = std::chrono::steady_clock::now();
    });

    websocket_->OnDisconnected([this]() {
        ESP_LOGI(TAG, "Websocket disconnected");
        auto alive = alive_;  // capture into Schedule lambda
        Application::GetInstance().Schedule([this, alive]() {
            if (!*alive) return;
            HandleDisconnected();
        });
    });

    ESP_LOGI(TAG, "Connecting to websocket server: %s with version: %d", url.c_str(), version_);
    xEventGroupClearBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT);
    if (!websocket_->Connect(url.c_str())) {
        ESP_LOGE(TAG, "Failed to connect to websocket server, code=%d", websocket_->GetLastError());
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        ScheduleReconnect();
        return false;
    }

    // Send hello message to describe the client
    auto message = GetHelloMessage();
    if (!SendText(message)) {
        ScheduleReconnect();
        return false;
    }

    // Wait for server hello
    EventBits_t bits = xEventGroupWaitBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT)) {
        ESP_LOGE(TAG, "Failed to receive server hello");
        SetError(Lang::Strings::SERVER_TIMEOUT);
        ScheduleReconnect();
        return false;
    }

    // 长连接建立完成
    reconnect_backoff_s_ = WEBSOCKET_RECONNECT_INITIAL_S;
    last_pong_time_us_.store(esp_timer_get_time());
    StartPingTimer();

    if (on_connected_ != nullptr) {
        on_connected_();
    }
    // 注意: 不调 on_audio_channel_opened_ — 那由 OpenAudioChannel 在 audio session 开始时触发
    return true;
}

std::string WebsocketProtocol::GetHelloMessage() {
    // keys: message type, version, audio_params (format, sample_rate, channels)
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "hello");
    cJSON_AddNumberToObject(root, "version", version_);
    cJSON* features = cJSON_CreateObject();
#if CONFIG_USE_SERVER_AEC
    cJSON_AddBoolToObject(features, "aec", true);
#endif
    cJSON_AddBoolToObject(features, "mcp", true);
    cJSON_AddBoolToObject(features, "heartbeat", true);
    cJSON_AddItemToObject(root, "features", features);
    cJSON_AddStringToObject(root, "transport", "websocket");
    cJSON* audio_params = cJSON_CreateObject();
    cJSON_AddStringToObject(audio_params, "format", "opus");
    cJSON_AddNumberToObject(audio_params, "sample_rate", 16000);
    cJSON_AddNumberToObject(audio_params, "channels", 1);
    cJSON_AddNumberToObject(audio_params, "frame_duration", OPUS_FRAME_DURATION_MS);
    cJSON_AddItemToObject(root, "audio_params", audio_params);
    auto json_str = cJSON_PrintUnformatted(root);
    std::string message(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return message;
}

void WebsocketProtocol::ParseServerHello(const cJSON* root) {
    auto transport = cJSON_GetObjectItem(root, "transport");
    if (transport == nullptr || strcmp(transport->valuestring, "websocket") != 0) {
        ESP_LOGE(TAG, "Unsupported transport: %s", transport->valuestring);
        return;
    }

    auto session_id = cJSON_GetObjectItem(root, "session_id");
    if (cJSON_IsString(session_id)) {
        session_id_ = session_id->valuestring;
        ESP_LOGI(TAG, "Session ID: %s", session_id_.c_str());
    }

    auto audio_params = cJSON_GetObjectItem(root, "audio_params");
    if (cJSON_IsObject(audio_params)) {
        auto sample_rate = cJSON_GetObjectItem(audio_params, "sample_rate");
        if (cJSON_IsNumber(sample_rate)) {
            server_sample_rate_ = sample_rate->valueint;
        }
        auto frame_duration = cJSON_GetObjectItem(audio_params, "frame_duration");
        if (cJSON_IsNumber(frame_duration)) {
            server_frame_duration_ = frame_duration->valueint;
        }
    }

    xEventGroupSetBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT);
}

void WebsocketProtocol::StartPingTimer() {
    if (ping_timer_ != nullptr) {
        esp_timer_stop(ping_timer_);
        esp_timer_start_periodic(ping_timer_, WEBSOCKET_PING_INTERVAL_MS * 1000ULL);
    }
}

void WebsocketProtocol::StopPingTimer() {
    if (ping_timer_ != nullptr) {
        esp_timer_stop(ping_timer_);
    }
}

void WebsocketProtocol::SendPing() {
    // 此函数在 main task 运行 (由 ping_timer 回调 Schedule 进来), 与析构串行
    int64_t now_us = esp_timer_get_time();
    int64_t last_pong = last_pong_time_us_.load();
    if (last_pong > 0 && (now_us - last_pong) > (int64_t)WEBSOCKET_PONG_TIMEOUT_MS * 1000LL) {
        ESP_LOGW(TAG, "Pong timeout (>%d ms), closing socket to trigger reconnect", WEBSOCKET_PONG_TIMEOUT_MS);
        {
            std::lock_guard<std::mutex> lock(websocket_mutex_);
            websocket_.reset();
        }
        // 我们主动 reset, OnDisconnected 可能不会被触发, 直接做清理
        StopPingTimer();
        bool was_active = audio_session_active_.exchange(false);
        if (was_active && on_audio_channel_closed_ != nullptr) {
            on_audio_channel_closed_();
        }
        ScheduleReconnect();
        return;
    }

    std::string ping = R"({"type":"ping","ts":)" + std::to_string(now_us / 1000) + "}";
    SendText(ping);
}

void WebsocketProtocol::ScheduleReconnect() {
    if (reconnect_timer_ == nullptr) return;
    ESP_LOGI(TAG, "Schedule websocket reconnect in %d s", reconnect_backoff_s_);
    esp_timer_stop(reconnect_timer_);
    esp_timer_start_once(reconnect_timer_, (uint64_t)reconnect_backoff_s_ * 1000000ULL);
    reconnect_backoff_s_ = std::min(reconnect_backoff_s_ * 2, WEBSOCKET_RECONNECT_MAX_S);
}

void WebsocketProtocol::HandleDisconnected() {
    // 此函数在 main task 中运行 (由 OnDisconnected 回调 Schedule 进来)
    StopPingTimer();

    bool was_active = audio_session_active_.exchange(false);
    if (was_active && on_audio_channel_closed_ != nullptr) {
        on_audio_channel_closed_();
    }
    if (on_disconnected_ != nullptr) {
        on_disconnected_();
    }

    ScheduleReconnect();
}
