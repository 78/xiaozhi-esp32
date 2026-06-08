#include "agora_rtc_protocol.h"
#include "board.h"
#include "settings.h"
#include "application.h"

#include <cstring>
#include <cJSON.h>
#include <esp_log.h>
#include "assets/lang_config.h"

#define TAG "AgoraRTC"

// Global instance pointer for static callback routing
static AgoraRtcProtocol* g_instance = nullptr;

AgoraRtcProtocol::AgoraRtcProtocol() {
    event_group_handle_ = xEventGroupCreate();
    g_instance = this;
}

AgoraRtcProtocol::~AgoraRtcProtocol() {
    CloseAudioChannel(false);
    FiniSdk();
    vEventGroupDelete(event_group_handle_);
    g_instance = nullptr;
}

bool AgoraRtcProtocol::Start() {
    return true;
}

bool AgoraRtcProtocol::InitSdk() {
    if (sdk_initialized_) {
        return true;
    }

    Settings settings("agora", false);
    std::string app_id = settings.GetString("app_id");
    if (app_id.empty()) {
        ESP_LOGE(TAG, "Agora app_id not configured");
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        return false;
    }

    agora_rtc_event_handler_t handler = {};
    handler.on_join_channel_success = OnJoinChannelSuccess;
    handler.on_error = OnError;
    handler.on_user_joined = OnUserJoined;
    handler.on_user_offline = OnUserOffline;
    handler.on_user_joined_with_user_account = OnUserJoinedWithUserAccount;
    handler.on_user_offline_with_user_account = OnUserOfflineWithUserAccount;
    handler.on_audio_data = OnAudioData;
    handler.on_connection_lost = OnConnectionLost;
    handler.on_reconnecting = OnReconnecting;
    handler.on_rejoin_channel_success = OnRejoinChannelSuccess;

    rtc_service_option_t option = {};
    option.area_code = AREA_CODE_GLOB;
    option.log_cfg.log_level = RTC_LOG_WARNING;
    option.log_cfg.log_disable = false;
    option.use_string_uid = true;

    int ret = agora_rtc_init(app_id.c_str(), &handler, &option);
    if (ret != ERR_OKAY) {
        ESP_LOGE(TAG, "agora_rtc_init failed: %d (%s)", ret, agora_rtc_err_2_str(ret));
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        return false;
    }

    sdk_initialized_ = true;
    ESP_LOGI(TAG, "Agora RTC SDK initialized (string_uid enabled, RTM enabled), version: %s",
             agora_rtc_get_version());
    return true;
}

void AgoraRtcProtocol::FiniSdk() {
    if (!sdk_initialized_) {
        return;
    }
    if (rtm_logged_in_) {
        agora_rtc_logout_rtm();
        rtm_logged_in_ = false;
    }
    agora_rtc_fini();
    sdk_initialized_ = false;
    ESP_LOGI(TAG, "Agora RTC SDK finalized");
}

bool AgoraRtcProtocol::OpenAudioChannel() {
    error_occurred_ = false;

    if (!InitSdk()) {
        return false;
    }

    Settings settings("agora", false);
    std::string channel = settings.GetString("channel");
    std::string token = settings.GetString("token");
    std::string user_account = settings.GetString("user_account");
    std::string rtm_token = settings.GetString("rtm_token");
    std::string rtm_uid = settings.GetString("rtm_uid");
    remote_rtm_uid_ = settings.GetString("remote_rtm_uid");

    if (channel.empty()) {
        ESP_LOGE(TAG, "Agora channel not configured");
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        return false;
    }

    // --- Login RTM ---
    if (!rtm_uid.empty()) {
        const char* rtm_token_ptr = rtm_token.empty() ? nullptr : rtm_token.c_str();

        agora_rtm_handler_t rtm_handler = {};
        rtm_handler.on_rtm_event = OnRtmEvent;
        rtm_handler.on_rtm_data = OnRtmData;
        rtm_handler.on_rtm_send_data_result = OnRtmSendDataResult;

        ESP_LOGI(TAG, "Logging in RTM, uid: %s", rtm_uid.c_str());
        int ret = agora_rtc_login_rtm(rtm_uid.c_str(), rtm_token_ptr, &rtm_handler);
        if (ret != ERR_OKAY) {
            ESP_LOGE(TAG, "agora_rtc_login_rtm failed: %d (%s)", ret, agora_rtc_err_2_str(ret));
            SetError(Lang::Strings::SERVER_NOT_CONNECTED);
            return false;
        }

        // Wait for RTM login event
        EventBits_t bits = xEventGroupWaitBits(event_group_handle_, AGORA_RTM_LOGIN_EVENT,
                                               pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
        if (!(bits & AGORA_RTM_LOGIN_EVENT)) {
            ESP_LOGE(TAG, "RTM login timeout");
            agora_rtc_logout_rtm();
            SetError(Lang::Strings::SERVER_TIMEOUT);
            return false;
        }
        ESP_LOGI(TAG, "RTM login success");
    }

    // --- Create connection and join channel with string uid ---
    int ret = agora_rtc_create_connection(&conn_id_);
    if (ret != ERR_OKAY) {
        ESP_LOGE(TAG, "agora_rtc_create_connection failed: %d (%s)", ret, agora_rtc_err_2_str(ret));
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        return false;
    }

    // Configure channel options
    rtc_channel_options_t options = {};
    options.auto_subscribe_audio = true;
    options.auto_subscribe_video = false;
    options.enable_audio_jitter_buffer = true;
    options.enable_audio_mixer = false;

    // Use G722 codec for PCM input at 16kHz
    options.audio_codec_opt.audio_codec_type = AUDIO_CODEC_TYPE_G722;
    options.audio_codec_opt.pcm_sample_rate = 16000;
    options.audio_codec_opt.pcm_channel_num = 1;
    options.audio_codec_opt.pcm_duration = 20;

    const char* token_ptr = token.empty() ? nullptr : token.c_str();

    if (!user_account.empty()) {
        // Join with string UID (user account)
        ESP_LOGI(TAG, "Joining channel: %s, user_account: %s", channel.c_str(), user_account.c_str());
        ret = agora_rtc_join_channel_with_user_account(conn_id_, channel.c_str(),
                                                       user_account.c_str(), token_ptr, &options);
    } else {
        // Fallback to numeric UID
        uint32_t uid = (uint32_t)settings.GetInt("uid");
        ESP_LOGI(TAG, "Joining channel: %s, uid: %lu", channel.c_str(), (unsigned long)uid);
        ret = agora_rtc_join_channel(conn_id_, channel.c_str(), uid, token_ptr, &options);
    }

    if (ret != ERR_OKAY) {
        ESP_LOGE(TAG, "agora_rtc_join_channel failed: %d (%s)", ret, agora_rtc_err_2_str(ret));
        agora_rtc_destroy_connection(conn_id_);
        conn_id_ = CONNECTION_ID_INVALID;
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        return false;
    }

    // Wait for join success
    EventBits_t bits = xEventGroupWaitBits(event_group_handle_, AGORA_JOINED_EVENT,
                                           pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & AGORA_JOINED_EVENT)) {
        ESP_LOGE(TAG, "Join channel timeout");
        agora_rtc_leave_channel(conn_id_);
        agora_rtc_destroy_connection(conn_id_);
        conn_id_ = CONNECTION_ID_INVALID;
        SetError(Lang::Strings::SERVER_TIMEOUT);
        return false;
    }

    if (on_audio_channel_opened_ != nullptr) {
        on_audio_channel_opened_();
    }

    return true;
}

void AgoraRtcProtocol::CloseAudioChannel(bool send_goodbye) {
    (void)send_goodbye;

    if (conn_id_ != CONNECTION_ID_INVALID) {
        agora_rtc_leave_channel(conn_id_);
        agora_rtc_destroy_connection(conn_id_);
        conn_id_ = CONNECTION_ID_INVALID;
    }
    joined_ = false;

    if (rtm_logged_in_) {
        agora_rtc_logout_rtm();
        rtm_logged_in_ = false;
    }

    if (on_audio_channel_closed_ != nullptr) {
        on_audio_channel_closed_();
    }
}

bool AgoraRtcProtocol::IsAudioChannelOpened() const {
    return joined_ && !error_occurred_ && !IsTimeout();
}

bool AgoraRtcProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    if (!joined_ || conn_id_ == CONNECTION_ID_INVALID) {
        return false;
    }

    audio_frame_info_t info = {};
    info.data_type = AUDIO_DATA_TYPE_PCM;

    int ret = agora_rtc_send_audio_data(conn_id_, packet->payload.data(),
                                        packet->payload.size(), &info);
    if (ret != ERR_OKAY) {
        ESP_LOGW(TAG, "send_audio_data failed: %d", ret);
        return false;
    }
    return true;
}

bool AgoraRtcProtocol::SendText(const std::string& text) {
    if (!joined_) {
        return false;
    }

    // Prefer RTM for signaling if available
    if (rtm_logged_in_ && !remote_rtm_uid_.empty()) {
        static uint32_t msg_id_counter = 0;
        int ret = agora_rtc_send_rtm_data(remote_rtm_uid_.c_str(), text.c_str(),
                                          text.size(), ++msg_id_counter, nullptr);
        if (ret != ERR_OKAY) {
            ESP_LOGW(TAG, "send_rtm_data failed: %d", ret);
            return false;
        }
        return true;
    }

    // Fallback to data stream
    if (conn_id_ == CONNECTION_ID_INVALID) {
        return false;
    }

    static int stream_id = -1;
    if (stream_id < 0) {
        int ret = agora_rtc_create_data_stream(conn_id_, &stream_id, true, true);
        if (ret != ERR_OKAY) {
            ESP_LOGE(TAG, "create_data_stream failed: %d", ret);
            return false;
        }
    }

    int ret = agora_rtc_send_stream_message(conn_id_, stream_id, text.c_str(), text.size());
    if (ret != ERR_OKAY) {
        ESP_LOGW(TAG, "send_stream_message failed: %d", ret);
        return false;
    }
    return true;
}

// ==================== RTC Static Callbacks ====================

void AgoraRtcProtocol::OnJoinChannelSuccess(connection_id_t conn_id, uint32_t uid, int elapsed_ms) {
    ESP_LOGI(TAG, "Join channel success, uid: %lu, elapsed: %d ms", (unsigned long)uid, elapsed_ms);
    if (g_instance) {
        g_instance->joined_ = true;
        g_instance->last_incoming_time_ = std::chrono::steady_clock::now();
        xEventGroupSetBits(g_instance->event_group_handle_, AGORA_JOINED_EVENT);
    }
}

void AgoraRtcProtocol::OnError(connection_id_t conn_id, int code, const char* msg) {
    ESP_LOGE(TAG, "Error %d: %s", code, msg ? msg : "unknown");
    if (g_instance) {
        g_instance->SetError(msg ? msg : "Agora RTC error");
    }
}

void AgoraRtcProtocol::OnUserJoined(connection_id_t conn_id, uint32_t uid, int elapsed_ms) {
    ESP_LOGI(TAG, "Remote user joined, uid: %lu", (unsigned long)uid);
}

void AgoraRtcProtocol::OnUserOffline(connection_id_t conn_id, uint32_t uid, int reason) {
    ESP_LOGI(TAG, "Remote user offline, uid: %lu, reason: %d", (unsigned long)uid, reason);
}

void AgoraRtcProtocol::OnUserJoinedWithUserAccount(connection_id_t conn_id, const user_info_t* user, int elapsed_ms) {
    ESP_LOGI(TAG, "Remote user joined, account: %s, uid: %lu",
             user->user_account, (unsigned long)user->uid);
}

void AgoraRtcProtocol::OnUserOfflineWithUserAccount(connection_id_t conn_id, const user_info_t* user, int reason) {
    ESP_LOGI(TAG, "Remote user offline, account: %s, reason: %d", user->user_account, reason);
}

void AgoraRtcProtocol::OnAudioData(connection_id_t conn_id, uint32_t uid, uint16_t sent_ts,
                                   const void* data_ptr, size_t data_len,
                                   const audio_frame_info_t* info_ptr) {
    if (!g_instance || !g_instance->on_incoming_audio_) {
        return;
    }
    g_instance->last_incoming_time_ = std::chrono::steady_clock::now();

    auto packet = std::make_unique<AudioStreamPacket>();
    packet->sample_rate = g_instance->server_sample_rate_;
    packet->frame_duration = g_instance->server_frame_duration_;
    packet->timestamp = sent_ts;
    packet->payload.assign((uint8_t*)data_ptr, (uint8_t*)data_ptr + data_len);

    g_instance->on_incoming_audio_(std::move(packet));
}

void AgoraRtcProtocol::OnConnectionLost(connection_id_t conn_id) {
    ESP_LOGW(TAG, "Connection lost");
    if (g_instance) {
        g_instance->SetError("Connection lost");
    }
}

void AgoraRtcProtocol::OnReconnecting(connection_id_t conn_id) {
    ESP_LOGW(TAG, "Reconnecting...");
}

void AgoraRtcProtocol::OnRejoinChannelSuccess(connection_id_t conn_id, uint32_t uid, int elapsed_ms) {
    ESP_LOGI(TAG, "Rejoin channel success, uid: %lu", (unsigned long)uid);
    if (g_instance) {
        g_instance->joined_ = true;
        g_instance->last_incoming_time_ = std::chrono::steady_clock::now();
    }
}

// ==================== RTM Static Callbacks ====================

void AgoraRtcProtocol::OnRtmEvent(const char* rtm_uid, rtm_event_type_e event_type, rtm_err_code_e err_code) {
    ESP_LOGI(TAG, "RTM event: uid=%s, type=%d, err=%d", rtm_uid ? rtm_uid : "null", event_type, err_code);

    if (!g_instance) {
        return;
    }

    if (event_type == RTM_EVENT_TYPE_LOGIN) {
        if (err_code == ERR_RTM_OK) {
            g_instance->rtm_logged_in_ = true;
            xEventGroupSetBits(g_instance->event_group_handle_, AGORA_RTM_LOGIN_EVENT);
        } else {
            ESP_LOGE(TAG, "RTM login failed: %d", err_code);
            g_instance->SetError("RTM login failed");
        }
    } else if (event_type == RTM_EVENT_TYPE_KICKOFF) {
        ESP_LOGW(TAG, "RTM kicked off");
        g_instance->rtm_logged_in_ = false;
    } else if (event_type == RTM_EVENT_TYPE_EXIT) {
        ESP_LOGI(TAG, "RTM exit");
        g_instance->rtm_logged_in_ = false;
    }
}

void AgoraRtcProtocol::OnRtmData(const char* rtm_uid, const void* msg, size_t msg_len, const char* custom_type) {
    if (!g_instance) {
        return;
    }

    g_instance->last_incoming_time_ = std::chrono::steady_clock::now();

    // Parse JSON message from RTM
    if (g_instance->on_incoming_json_) {
        auto root = cJSON_ParseWithLength((const char*)msg, msg_len);
        if (root) {
            g_instance->on_incoming_json_(root);
            cJSON_Delete(root);
        } else {
            ESP_LOGW(TAG, "RTM data is not valid JSON, len=%zu", msg_len);
        }
    }
}

void AgoraRtcProtocol::OnRtmSendDataResult(const char* rtm_uid, uint32_t msg_id, rtm_msg_state_e state) {
    if (state != RTM_MSG_STATE_RECEIVED) {
        ESP_LOGW(TAG, "RTM send msg_id=%lu state=%d", (unsigned long)msg_id, state);
    }
}
