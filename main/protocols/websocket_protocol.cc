#include "websocket_protocol.h"
#include "application.h"
#include "board.h"
#include "settings.h"
#include "system_info.h"

#include <esp_log.h>
#include <arpa/inet.h>
#include <cJSON.h>
#include <cstring>
#include "assets/lang_config.h"

#define TAG "WS"

WebsocketProtocol::WebsocketProtocol() { event_group_handle_ = xEventGroupCreate(); }

WebsocketProtocol::~WebsocketProtocol() {
    DisconnectControlChannel();
    vEventGroupDelete(event_group_handle_);
}

bool WebsocketProtocol::Start() {
    return true;
}

bool WebsocketProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
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
    if (persistent_mode_) {
        return audio_channel_open_ && websocket_ != nullptr && websocket_->IsConnected() && !error_occurred_;
    }
    return websocket_ != nullptr && websocket_->IsConnected() && !error_occurred_ && !IsTimeout();
}

void WebsocketProtocol::CloseAudioChannel(bool send_goodbye) {
    (void)send_goodbye;
    if (persistent_mode_) {
        if (audio_channel_open_) {
            audio_channel_open_ = false;
            if (on_audio_channel_closed_ != nullptr) {
                on_audio_channel_closed_();
            }
        }
        return;
    }
    StopPingTimer();
    should_reconnect_ = false;
    websocket_.reset();
}

bool WebsocketProtocol::OpenAudioChannel() {
    if (persistent_mode_ && websocket_ != nullptr && websocket_->IsConnected()) {
        audio_channel_open_ = true;
        error_occurred_ = false;
        last_incoming_time_ = std::chrono::steady_clock::now();
        if (on_audio_channel_opened_ != nullptr) {
            on_audio_channel_opened_();
        }
        return true;
    }

    Settings settings("websocket", false);
    std::string url = settings.GetString("url");
    std::string token = settings.GetString("token");
    int version = settings.GetInt("version");
    if (version != 0) {
        version_ = version;
    }

    error_occurred_ = false;

    if (websocket_) {
        websocket_.reset();
    }

    auto network = Board::GetInstance().GetNetwork();
    websocket_ = network->CreateWebSocket(1);
    if (websocket_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create websocket");
        return false;
    }

    if (!token.empty()) {
        if (token.find(" ") == std::string::npos) {
            token = "Bearer " + token;
        }
        websocket_->SetHeader("Authorization", token.c_str());
    }
    websocket_->SetHeader("Protocol-Version", std::to_string(version_).c_str());
    websocket_->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    websocket_->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());

    SetupCallbacks();

    ESP_LOGI(TAG, "Connecting to websocket server: %s with version: %d", url.c_str(), version_);
    if (!websocket_->Connect(url.c_str())) {
        ESP_LOGE(TAG, "Failed to connect to websocket server, code=%d", websocket_->GetLastError());
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        return false;
    }

    auto message = GetHelloMessage();
    if (!SendText(message)) {
        return false;
    }

    EventBits_t bits = xEventGroupWaitBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT)) {
        ESP_LOGE(TAG, "Failed to receive server hello");
        SetError(Lang::Strings::SERVER_TIMEOUT);
        return false;
    }

    audio_channel_open_ = true;
    if (on_audio_channel_opened_ != nullptr) {
        on_audio_channel_opened_();
    }

    return true;
}

bool WebsocketProtocol::ConnectControlChannel() {
    if (persistent_mode_ && websocket_ != nullptr && websocket_->IsConnected()) {
        return true;
    }

    Settings settings("websocket", false);
    std::string url = settings.GetString("url");
    std::string token = settings.GetString("token");
    int version = settings.GetInt("version");
    if (version != 0) {
        version_ = version;
    }

    error_occurred_ = false;
    should_reconnect_ = true;
    audio_channel_open_ = false;

    if (websocket_) {
        websocket_.reset();
    }

    auto network = Board::GetInstance().GetNetwork();
    websocket_ = network->CreateWebSocket(1);
    if (websocket_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create websocket for control channel");
        ScheduleReconnect();
        return false;
    }

    if (!token.empty()) {
        if (token.find(" ") == std::string::npos) {
            token = "Bearer " + token;
        }
        websocket_->SetHeader("Authorization", token.c_str());
    }
    websocket_->SetHeader("Protocol-Version", std::to_string(version_).c_str());
    websocket_->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    websocket_->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());

    SetupCallbacks();

    ESP_LOGI(TAG, "Connecting persistent websocket: %s with version: %d", url.c_str(), version_);
    if (!websocket_->Connect(url.c_str())) {
        ESP_LOGE(TAG, "Failed to connect persistent websocket, code=%d", websocket_->GetLastError());
        ScheduleReconnect();
        return false;
    }

    auto message = GetHelloMessage();
    if (!SendText(message)) {
        ScheduleReconnect();
        return false;
    }

    EventBits_t bits = xEventGroupWaitBits(event_group_handle_, WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT, pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT)) {
        ESP_LOGE(TAG, "Failed to receive server hello for control channel");
        ScheduleReconnect();
        return false;
    }

    persistent_mode_ = true;
    reconnect_delay_seconds_ = RECONNECT_INITIAL_DELAY_SECONDS;
    StartPingTimer();

    ESP_LOGI(TAG, "Persistent websocket established");
    return true;
}

void WebsocketProtocol::DisconnectControlChannel() {
    persistent_mode_ = false;
    should_reconnect_ = false;
    audio_channel_open_ = false;

    StopPingTimer();
    CancelReconnect();

    if (websocket_) {
        websocket_->Close();
        websocket_.reset();
    }
}

void WebsocketProtocol::SetupCallbacks() {
    websocket_->OnData([this](const char* data, size_t len, bool binary) {
        if (binary) {
            if (on_incoming_audio_ != nullptr) {
                ESP_LOGI(TAG, "Incoming binary: %zu bytes", len);
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
                        .payload = std::vector<uint8_t>(payload, payload + bp2->payload_size)}));
                } else if (version_ == 3) {
                    BinaryProtocol3* bp3 = (BinaryProtocol3*)data;
                    bp3->payload_size = ntohs(bp3->payload_size);
                    auto payload = (uint8_t*)bp3->payload;
                    on_incoming_audio_(std::make_unique<AudioStreamPacket>(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = 0,
                        .payload = std::vector<uint8_t>(payload, payload + bp3->payload_size)}));
                } else {
                    on_incoming_audio_(std::make_unique<AudioStreamPacket>(AudioStreamPacket{
                        .sample_rate = server_sample_rate_,
                        .frame_duration = server_frame_duration_,
                        .timestamp = 0,
                        .payload = std::vector<uint8_t>((uint8_t*)data, (uint8_t*)data + len)}));
                }
            }
        } else {
            ESP_LOGI(TAG, "Incoming text: %.*s", (int)len, data);
            auto root = cJSON_ParseWithLength(data, len);
            auto type = cJSON_GetObjectItem(root, "type");
            if (cJSON_IsString(type)) {
                if (strcmp(type->valuestring, "hello") == 0) {
                    ParseServerHello(root);
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
        bool was_open = audio_channel_open_;
        audio_channel_open_ = false;
        if (was_open && on_audio_channel_closed_ != nullptr) {
            on_audio_channel_closed_();
        }
        if (should_reconnect_) {
            ScheduleReconnect();
        }
    });
}

void WebsocketProtocol::StartPingTimer() {
    StopPingTimer();
    esp_timer_create_args_t args = {
        .callback = [](void* arg) {
            auto* self = static_cast<WebsocketProtocol*>(arg);
            if (self->websocket_ && self->websocket_->IsConnected()) {
                self->websocket_->Ping();
            } else if (self->should_reconnect_) {
                self->ScheduleReconnect();
            }
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ws_ping",
        .skip_unhandled_events = true
    };
    esp_timer_create(&args, &ping_timer_);
    esp_timer_start_periodic(ping_timer_, PING_INTERVAL_SECONDS * 1000000);
}

void WebsocketProtocol::StopPingTimer() {
    if (ping_timer_ != nullptr) {
        esp_timer_stop(ping_timer_);
        esp_timer_delete(ping_timer_);
        ping_timer_ = nullptr;
    }
}

void WebsocketProtocol::ScheduleReconnect() {
    CancelReconnect();
    esp_timer_create_args_t args = {
        .callback = [](void* arg) {
            auto* self = static_cast<WebsocketProtocol*>(arg);
            self->reconnect_timer_ = nullptr;
            ESP_LOGI(TAG, "Attempting reconnect (delay=%ds)", self->reconnect_delay_seconds_);
            if (self->ConnectControlChannel()) {
                self->reconnect_delay_seconds_ = RECONNECT_INITIAL_DELAY_SECONDS;
            } else {
                self->reconnect_delay_seconds_ = std::min(
                    self->reconnect_delay_seconds_ * RECONNECT_BACKOFF_MULTIPLIER,
                    RECONNECT_MAX_DELAY_SECONDS);
                self->ScheduleReconnect();
            }
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ws_reconnect",
        .skip_unhandled_events = true
    };
    esp_timer_create(&args, &reconnect_timer_);
    esp_timer_start_once(reconnect_timer_, reconnect_delay_seconds_ * 1000000);
}

void WebsocketProtocol::CancelReconnect() {
    if (reconnect_timer_ != nullptr) {
        esp_timer_stop(reconnect_timer_);
        esp_timer_delete(reconnect_timer_);
        reconnect_timer_ = nullptr;
    }

}

std::string WebsocketProtocol::GetHelloMessage() {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "hello");
    cJSON_AddNumberToObject(root, "version", version_);
    cJSON* features = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "features", features);
    AddTextFontCapabilities(root);
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
