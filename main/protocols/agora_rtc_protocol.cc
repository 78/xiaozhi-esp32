#include "agora_rtc_protocol.h"
#include "board.h"
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

bool AgoraRtcProtocol::InitSdk(const std::string& app_id) {
    if (sdk_initialized_) {
        return true;
    }

    agora_rtc_event_handler_t handler = {};
    handler.on_join_channel_success = OnJoinChannelSuccess;
    handler.on_error = OnError;
    handler.on_user_joined_with_user_account = OnUserJoinedWithUserAccount;
    handler.on_user_offline_with_user_account = OnUserOfflineWithUserAccount;
    handler.on_audio_data = OnAudioData;
    handler.on_connection_lost = OnConnectionLost;
    handler.on_reconnecting = OnReconnecting;
    handler.on_rejoin_channel_success = OnRejoinChannelSuccess;

    rtc_service_option_t option = {};
    option.area_code = AREA_CODE_GLOB;
    option.log_cfg.log_level = RTC_LOG_NOTICE;
    option.log_cfg.log_disable = false;
    option.use_string_uid = true;

    int ret = agora_rtc_init(app_id.c_str(), &handler, &option);
    if (ret != ERR_OKAY) {
        ESP_LOGE(TAG, "agora_rtc_init failed: %d (%s)", ret, agora_rtc_err_2_str(ret));
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        return false;
    }

    sdk_initialized_ = true;
    ESP_LOGI(TAG, "Agora RTC SDK initialized, version: %s", agora_rtc_get_version());
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

bool AgoraRtcProtocol::RunPairingFlow() {
    // Delegate to Application's pairing task via DeviceApiClient
    // This is called from Application::AgoraPairingTask()
    // The actual pairing logic is in Application since it needs display/audio access
    return device_api_.HasDeviceToken();
}

bool AgoraRtcProtocol::OpenAudioChannel() {
    // Prevent double-join
    if (joined_ && conn_id_ != CONNECTION_ID_INVALID) {
        ESP_LOGW(TAG, "Already in channel, skip OpenAudioChannel");
        return true;
    }

    error_occurred_ = false;

    // Step 1: Call Device API to start conversation and get RTC params
    ESP_LOGI(TAG, "Starting conversation via Device API...");
    ConversationInfo info;
    if (!device_api_.StartConversation(info)) {
        auto err = device_api_.GetLastError();
        ESP_LOGE(TAG, "StartConversation failed, error=%d", (int)err);

        // For 502 server errors, retry once after 2 seconds
        if (err == DeviceApiError::kServerError) {
            ESP_LOGW(TAG, "Server error, retrying in 2s...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            if (!device_api_.StartConversation(info)) {
                err = device_api_.GetLastError();
                ESP_LOGE(TAG, "StartConversation retry failed, error=%d", (int)err);
                SetError(Lang::Strings::SERVER_NOT_CONNECTED);
                return false;
            }
            // Retry succeeded, continue below
        } else if (err == DeviceApiError::kUnauthenticated || err == DeviceApiError::kTokenRevoked ||
                   err == DeviceApiError::kNotBound) {
            SetError("设备未绑定，请重新配对");
            return false;
        } else {
            SetError(Lang::Strings::SERVER_NOT_CONNECTED);
            return false;
        }
    }

    current_conversation_ = info;
    ESP_LOGI(TAG, "Conversation started: id=%s, channel=%s, uid=%s, agent_uid=%s",
             info.conversation_id.c_str(), info.rtc.channel.c_str(),
             info.rtc.uid.c_str(), info.rtc.agent_uid.c_str());

    // Step 2: Initialize SDK with the app_id from server
    if (!InitSdk(info.rtc.app_id)) {
        return false;
    }

    // Step 3: Login RTM using local_uid as RTM UID
    std::string rtm_uid = info.rtc.uid;
    remote_rtm_uid_ = info.rtc.agent_uid;

    agora_rtm_handler_t rtm_handler = {};
    rtm_handler.on_rtm_event = OnRtmEvent;
    rtm_handler.on_rtm_data = OnRtmData;
    rtm_handler.on_rtm_send_data_result = OnRtmSendDataResult;

    ESP_LOGI(TAG, "Logging in RTM, uid: %s", rtm_uid.c_str());
    int ret = agora_rtc_login_rtm(rtm_uid.c_str(), info.rtc.token.c_str(), &rtm_handler);
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

    // Step 4: Create connection and join channel
    ret = agora_rtc_create_connection(&conn_id_);
    if (ret != ERR_OKAY) {
        ESP_LOGE(TAG, "agora_rtc_create_connection failed: %d (%s)", ret, agora_rtc_err_2_str(ret));
        agora_rtc_logout_rtm();
        rtm_logged_in_ = false;
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        return false;
    }

    // Configure channel options
    rtc_channel_options_t options = {};
    options.auto_subscribe_audio = true;
    options.auto_subscribe_video = false;
    options.enable_audio_jitter_buffer = true;
    options.enable_audio_mixer = false;
    options.enable_audio_decode = true;
    options.enable_audio_ai_qos = true;
    options.enable_audio_downlink_aec = true;

    // Use SDK built-in G722 codec for PCM input at 16kHz
    options.audio_codec_opt.audio_codec_type = AUDIO_CODEC_TYPE_G722;
    options.audio_codec_opt.pcm_sample_rate = 16000;
    options.audio_codec_opt.pcm_channel_num = 1;
    options.audio_codec_opt.pcm_duration = 60;

    // Join channel with uid from server (string uid, use token)
    ESP_LOGI(TAG, "Joining channel: %s, uid: %s, token: %.8s...",
             info.rtc.channel.c_str(), info.rtc.uid.c_str(),
             info.rtc.token.empty() ? "none" : info.rtc.token.c_str());

    ret = agora_rtc_join_channel_with_user_account(conn_id_, info.rtc.channel.c_str(),
                                                   info.rtc.uid.c_str(),
                                                   info.rtc.token.empty() ? nullptr : info.rtc.token.c_str(),
                                                   &options);
    if (ret != ERR_OKAY) {
        ESP_LOGE(TAG, "agora_rtc_join_channel failed: %d (%s)", ret, agora_rtc_err_2_str(ret));
        agora_rtc_destroy_connection(conn_id_);
        conn_id_ = CONNECTION_ID_INVALID;
        agora_rtc_logout_rtm();
        rtm_logged_in_ = false;
        SetError(Lang::Strings::SERVER_NOT_CONNECTED);
        return false;
    }

    // Wait for join success
    bits = xEventGroupWaitBits(event_group_handle_, AGORA_JOINED_EVENT,
                               pdTRUE, pdFALSE, pdMS_TO_TICKS(10000));
    if (!(bits & AGORA_JOINED_EVENT)) {
        ESP_LOGE(TAG, "Join channel timeout");
        agora_rtc_leave_channel(conn_id_);
        agora_rtc_destroy_connection(conn_id_);
        conn_id_ = CONNECTION_ID_INVALID;
        agora_rtc_logout_rtm();
        rtm_logged_in_ = false;
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
        ESP_LOGI(TAG, "Left channel and destroyed connection");
    }
    joined_ = false;

    if (rtm_logged_in_) {
        agora_rtc_logout_rtm();
        rtm_logged_in_ = false;
        ESP_LOGI(TAG, "RTM logged out");
    }

    // Stop conversation via Device API
    if (!current_conversation_.conversation_id.empty()) {
        device_api_.StopConversation(current_conversation_.conversation_id, "device_hangup");
        current_conversation_ = ConversationInfo{};
    }

    if (on_audio_channel_closed_ != nullptr) {
        on_audio_channel_closed_();
    }
}

bool AgoraRtcProtocol::IsAudioChannelOpened() const {
    return joined_ && rtm_logged_in_ && !error_occurred_ && !IsTimeout();
}

bool AgoraRtcProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    if (!joined_ || conn_id_ == CONNECTION_ID_INVALID) {
        return false;
    }

    // Expected PCM frame size: 60ms @ 16kHz mono 16-bit = 1920 bytes
    static const size_t kExpectedFrameSize = 960 * sizeof(int16_t); // 1920 bytes
    static uint32_t send_count = 0;

    // Skip packets that don't match expected PCM frame size
    if (packet->payload.size() != kExpectedFrameSize) {
        return true;
    }

    if (++send_count % 16 == 0) {
        ESP_LOGI(TAG, "SendAudio: total=%lu, size=%d", (unsigned long)send_count, (int)packet->payload.size());
    }

    // Interleave mic + ref for downlink AEC: [mic1, ref1, mic2, ref2, ...]
    const int16_t* mic_data = (const int16_t*)packet->payload.data();
    size_t mic_samples = packet->payload.size() / sizeof(int16_t); // 960

    // Get ref data from ring buffer
    std::vector<int16_t> ref_data(mic_samples, 0);
    {
        std::lock_guard<std::mutex> lock(ref_mutex_);
        size_t available = std::min(ref_buffer_.size(), mic_samples);
        if (available > 0) {
            std::copy(ref_buffer_.begin(), ref_buffer_.begin() + available, ref_data.begin());
            ref_buffer_.erase(ref_buffer_.begin(), ref_buffer_.begin() + available);
        }
    }

    // Build interleaved buffer: mic1 ref1 mic2 ref2 ...
    std::vector<int16_t> interleaved(mic_samples * 2);
    for (size_t i = 0; i < mic_samples; i++) {
        interleaved[i * 2] = mic_data[i];
        interleaved[i * 2 + 1] = ref_data[i];
    }

    audio_frame_info_t info = {};
    info.data_type = AUDIO_DATA_TYPE_PCM;

    int ret = agora_rtc_send_audio_data(conn_id_, interleaved.data(),
                                        interleaved.size() * sizeof(int16_t), &info);
    if (ret != ERR_OKAY) {
        ESP_LOGW(TAG, "send_audio_data failed: %d", ret);
        return false;
    }
    return true;
}

bool AgoraRtcProtocol::SendText(const std::string& text) {
    if (!rtm_logged_in_ || remote_rtm_uid_.empty()) {
        ESP_LOGW(TAG, "RTM not available, cannot send text");
        return false;
    }

    int ret = agora_rtc_send_rtm_data(remote_rtm_uid_.c_str(), text.c_str(),
                                      text.size(), ++rtm_msg_id_, nullptr);
    if (ret != ERR_OKAY) {
        ESP_LOGW(TAG, "send_rtm_data failed: %d (%s)", ret, agora_rtc_err_2_str(ret));
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
    if (!g_instance) {
        return;
    }
    g_instance->last_incoming_time_ = std::chrono::steady_clock::now();

    // Store downlink PCM into ref ring buffer for downlink AEC
    {
        std::lock_guard<std::mutex> lock(g_instance->ref_mutex_);
        const int16_t* pcm = (const int16_t*)data_ptr;
        size_t samples = data_len / sizeof(int16_t);
        g_instance->ref_buffer_.insert(g_instance->ref_buffer_.end(), pcm, pcm + samples);
        if (g_instance->ref_buffer_.size() > kRefBufferMaxSamples) {
            g_instance->ref_buffer_.erase(g_instance->ref_buffer_.begin(),
                g_instance->ref_buffer_.begin() + (g_instance->ref_buffer_.size() - kRefBufferMaxSamples));
        }
    }

    if (!g_instance->on_incoming_audio_) {
        return;
    }

    static uint32_t recv_count = 0;
    if (++recv_count % 16 == 0) {
        ESP_LOGI(TAG, "RecvAudio: total=%lu, size=%d, type=%d",
                 (unsigned long)recv_count, (int)data_len, (int)info_ptr->data_type);
    }

    if (data_len == 0 || data_len > 32000) {
        ESP_LOGW(TAG, "RecvAudio: invalid data_len=%d, skipping", (int)data_len);
        return;
    }

    auto packet = std::make_unique<AudioStreamPacket>();
    packet->sample_rate = 16000;
    packet->frame_duration = 0;
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
        g_instance->SetError("RTM kicked off");
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
        ESP_LOGW(TAG, "RTM msg_id=%lu state=%d (not received)", (unsigned long)msg_id, state);
    }
}
