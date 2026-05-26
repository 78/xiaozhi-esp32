#include "agora_rtc_protocol.h"
#include "audio_service.h"
#include "settings.h"
#include "board.h"
#include "system_info.h"

#include <cstring>
#include <esp_log.h>

#define TAG "AgoraRTC"

// Singleton pointer for static callbacks
static AgoraRtcProtocol* g_instance = nullptr;

AgoraRtcProtocol::AgoraRtcProtocol() {
    event_group_handle_ = xEventGroupCreate();
    g_instance = this;
}

AgoraRtcProtocol::~AgoraRtcProtocol() {
    CloseAudioChannel(false);
    g_instance = nullptr;
    vEventGroupDelete(event_group_handle_);
}

bool AgoraRtcProtocol::Start() {
    return true;
}

bool AgoraRtcProtocol::OpenAudioChannel() {
    Settings settings("agora", false);
    app_id_ = settings.GetString("app_id");
    channel_name_ = settings.GetString("channel");
    token_ = settings.GetString("token");
    uid_ = settings.GetInt("uid");

    if (app_id_.empty() || channel_name_.empty()) {
        ESP_LOGE(TAG, "Missing app_id or channel in settings");
        SetError("Agora config missing");
        return false;
    }

    error_occurred_ = false;

    // Init Agora SDK
    agora_rtc_event_handler_t handler = {};
    handler.on_join_channel_success = OnJoinChannelSuccess;
    handler.on_connection_lost = OnConnectionLost;
    handler.on_rejoin_channel_success = OnRejoinChannelSuccess;
    handler.on_error = OnError;
    handler.on_audio_data = OnAudioData;
    handler.on_user_joined = OnUserJoined;
    handler.on_user_offline = OnUserOffline;
    handler.on_stream_message = OnStreamMessage;

    rtc_service_option_t service_opt = {};
    service_opt.area_code = AREA_CODE_GLOB;
    service_opt.log_cfg.log_level = RTC_LOG_WARNING;

    int ret = agora_rtc_init(app_id_.c_str(), &handler, &service_opt);
    if (ret < 0) {
        ESP_LOGE(TAG, "agora_rtc_init failed: %s", agora_rtc_err_2_str(ret));
        SetError("Agora SDK init failed");
        return false;
    }

    // Create connection
    ret = agora_rtc_create_connection(&conn_id_);
    if (ret < 0) {
        ESP_LOGE(TAG, "agora_rtc_create_connection failed: %s", agora_rtc_err_2_str(ret));
        agora_rtc_fini();
        SetError("Agora create connection failed");
        return false;
    }

    // Join channel with OPUS audio codec for sending PCM
    rtc_channel_options_t channel_opt = {};
    channel_opt.auto_subscribe_audio = true;
    channel_opt.auto_subscribe_video = false;
    channel_opt.enable_audio_jitter_buffer = true;
    channel_opt.enable_audio_mixer = false;
    // Use built-in codec to send PCM as OPUS
    channel_opt.audio_codec_opt.audio_codec_type = AUDIO_CODEC_DISABLED;
    channel_opt.audio_codec_opt.pcm_sample_rate = 16000;
    channel_opt.audio_codec_opt.pcm_channel_num = 1;
    channel_opt.audio_codec_opt.pcm_duration = OPUS_FRAME_DURATION_MS;

    const char* tk = token_.empty() ? nullptr : token_.c_str();
    ret = agora_rtc_join_channel(conn_id_, channel_name_.c_str(), uid_, tk, &channel_opt);
    if (ret < 0) {
        ESP_LOGE(TAG, "agora_rtc_join_channel failed: %s", agora_rtc_err_2_str(ret));
        agora_rtc_destroy_connection(conn_id_);
        agora_rtc_fini();
        conn_id_ = CONNECTION_ID_INVALID;
        SetError("Agora join channel failed");
        return false;
    }

    // Wait for join success
    EventBits_t bits = xEventGroupWaitBits(event_group_handle_, AGORA_PROTOCOL_JOINED_EVENT,
                                           pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & AGORA_PROTOCOL_JOINED_EVENT)) {
        ESP_LOGE(TAG, "Join channel timeout");
        agora_rtc_leave_channel(conn_id_);
        agora_rtc_destroy_connection(conn_id_);
        agora_rtc_fini();
        conn_id_ = CONNECTION_ID_INVALID;
        SetError("Agora join timeout");
        return false;
    }

    channel_opened_ = true;
    last_incoming_time_ = std::chrono::steady_clock::now();

    if (on_audio_channel_opened_) {
        on_audio_channel_opened_();
    }

    ESP_LOGI(TAG, "Audio channel opened, channel=%s uid=%u", channel_name_.c_str(), uid_);
    return true;
}

void AgoraRtcProtocol::CloseAudioChannel(bool send_goodbye) {
    if (!channel_opened_) {
        return;
    }
    channel_opened_ = false;
    joined_ = false;

    if (conn_id_ != CONNECTION_ID_INVALID) {
        agora_rtc_leave_channel(conn_id_);
        agora_rtc_destroy_connection(conn_id_);
        conn_id_ = CONNECTION_ID_INVALID;
    }
    agora_rtc_fini();

    if (on_audio_channel_closed_) {
        on_audio_channel_closed_();
    }
    ESP_LOGI(TAG, "Audio channel closed");
}

bool AgoraRtcProtocol::IsAudioChannelOpened() const {
    return channel_opened_ && joined_ && !error_occurred_ && !IsTimeout();
}

bool AgoraRtcProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    if (!IsAudioChannelOpened()) {
        return false;
    }

    // Send OPUS encoded data directly
    audio_frame_info_t info = {};
    info.data_type = AUDIO_DATA_TYPE_OPUS;

    int ret = agora_rtc_send_audio_data(conn_id_, packet->payload.data(), packet->payload.size(), &info);
    if (ret < 0) {
        ESP_LOGE(TAG, "Send audio failed: %s", agora_rtc_err_2_str(ret));
        return false;
    }
    return true;
}

bool AgoraRtcProtocol::SendText(const std::string& text) {
    if (!IsAudioChannelOpened()) {
        return false;
    }

    // Use data stream to send JSON text messages
    int stream_id = 0;
    int ret = agora_rtc_create_data_stream(conn_id_, &stream_id, true, true);
    if (ret < 0) {
        ESP_LOGE(TAG, "Create data stream failed: %s", agora_rtc_err_2_str(ret));
        return false;
    }

    ret = agora_rtc_send_stream_message(conn_id_, stream_id, text.c_str(), text.size());
    if (ret < 0) {
        ESP_LOGE(TAG, "Send stream message failed: %s", agora_rtc_err_2_str(ret));
        return false;
    }
    return true;
}

// --- Static callbacks ---

void AgoraRtcProtocol::OnJoinChannelSuccess(connection_id_t conn_id, uint32_t uid, int elapsed_ms) {
    ESP_LOGI(TAG, "Joined channel, uid=%u elapsed=%dms", uid, elapsed_ms);
    if (g_instance) {
        g_instance->uid_ = uid;
        g_instance->joined_ = true;
        xEventGroupSetBits(g_instance->event_group_handle_, AGORA_PROTOCOL_JOINED_EVENT);
    }
}

void AgoraRtcProtocol::OnConnectionLost(connection_id_t conn_id) {
    ESP_LOGW(TAG, "Connection lost");
    if (g_instance) {
        g_instance->joined_ = false;
        g_instance->SetError("Agora connection lost");
    }
}

void AgoraRtcProtocol::OnRejoinChannelSuccess(connection_id_t conn_id, uint32_t uid, int elapsed_ms) {
    ESP_LOGI(TAG, "Rejoined channel, uid=%u", uid);
    if (g_instance) {
        g_instance->joined_ = true;
        g_instance->last_incoming_time_ = std::chrono::steady_clock::now();
    }
}

void AgoraRtcProtocol::OnError(connection_id_t conn_id, int code, const char* msg) {
    ESP_LOGE(TAG, "Error %d: %s", code, msg ? msg : "");
    if (g_instance && (code == ERR_INVALID_APP_ID || code == ERR_INVALID_TOKEN || code == ERR_TOKEN_EXPIRED)) {
        g_instance->SetError(msg ? msg : "Agora error");
    }
}

void AgoraRtcProtocol::OnAudioData(connection_id_t conn_id, uint32_t uid, uint16_t sent_ts,
                                    const void* data, size_t len, const audio_frame_info_t* info) {
    if (!g_instance || !g_instance->on_incoming_audio_) {
        return;
    }
    g_instance->last_incoming_time_ = std::chrono::steady_clock::now();

    g_instance->on_incoming_audio_(std::make_unique<AudioStreamPacket>(AudioStreamPacket{
        .sample_rate = g_instance->server_sample_rate_,
        .frame_duration = g_instance->server_frame_duration_,
        .timestamp = sent_ts,
        .payload = std::vector<uint8_t>((uint8_t*)data, (uint8_t*)data + len)
    }));
}

void AgoraRtcProtocol::OnUserJoined(connection_id_t conn_id, uint32_t uid, int elapsed_ms) {
    ESP_LOGI(TAG, "Remote user %u joined", uid);
}

void AgoraRtcProtocol::OnUserOffline(connection_id_t conn_id, uint32_t uid, int reason) {
    ESP_LOGI(TAG, "Remote user %u offline, reason=%d", uid, reason);
}

void AgoraRtcProtocol::OnStreamMessage(connection_id_t conn_id, uint32_t uid, int stream_id,
                                        const char* data, size_t length, uint64_t sent_ts) {
    if (!g_instance || !g_instance->on_incoming_json_) {
        return;
    }
    g_instance->last_incoming_time_ = std::chrono::steady_clock::now();

    auto root = cJSON_ParseWithLength(data, length);
    if (root) {
        g_instance->on_incoming_json_(root);
        cJSON_Delete(root);
    }
}
