#include "agora_rtc_protocol.h"
#include "board.h"
#include "application.h"

#include <cstring>
#include <cJSON.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include "assets/lang_config.h"

#define TAG "AgoraRTC"

// Global instance pointer for static callback routing
static AgoraRtcProtocol* g_instance = nullptr;

AgoraRtcProtocol::AgoraRtcProtocol() {
    event_group_handle_ = xEventGroupCreate();
    g_instance = this;

    // Allocate lock-free ring buffer in PSRAM for downlink AEC reference
    ref_ring_buffer_ = std::make_unique<LockFreeRingBuffer>(kRefBufferMaxSamples);
    if (!ref_ring_buffer_->IsValid()) {
        ESP_LOGE(TAG, "Failed to allocate ref ring buffer");
    }

    // Allocate lock-free ring buffer in PSRAM for downlink PCM (512KB)
    downlink_ring_buffer_ = std::make_unique<LockFreeRingBuffer>(kDownlinkBufferSamples);
    if (!downlink_ring_buffer_->IsValid()) {
        ESP_LOGE(TAG, "Failed to allocate downlink ring buffer");
    }

    // Binary semaphore for thread-safe downlink task termination
    downlink_exit_sem_ = xSemaphoreCreateBinary();
}

AgoraRtcProtocol::~AgoraRtcProtocol() {
    CloseAudioChannel(false);
    FiniSdk();
    if (downlink_exit_sem_) {
        vSemaphoreDelete(downlink_exit_sem_);
    }
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
    handler.on_user_mute_audio = OnUserMuteAudio;
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
    return device_api_.HasDeviceToken();
}

void AgoraRtcProtocol::DownlinkTask(void* arg) {
    AgoraRtcProtocol* self = static_cast<AgoraRtcProtocol*>(arg);
    self->DownlinkTaskLoop();
    vTaskDelete(NULL);
}

void AgoraRtcProtocol::DownlinkTaskLoop() {
    ESP_LOGI(TAG, "Downlink task started");

    const size_t frame_bytes = kDownlinkFrameSamples * sizeof(int16_t);
    const TickType_t frame_ticks = pdMS_TO_TICKS(60);
    TickType_t last_wake = xTaskGetTickCount();

    while (downlink_task_running_) {
        // Check for clear-buffer request (set by OnUserMuteAudio from SDK thread).
        // Reset is done here in the downlink task's own context to avoid SPSC race:
        //   downlink_ring_buffer_  — producer is OnAudioData (SDK thread), consumer is this task
        //   ref_ring_buffer_       — producer is this task, consumer is main task (SendAudio)
        // Both Reset() calls are safe because Write and Read are not interleaved within this loop iteration.
        if (downlink_clear_requested_.exchange(false)) {
            ESP_LOGI(TAG, "Clear ring buffers on mute request");
            downlink_ring_buffer_->Reset();
            ref_ring_buffer_->Reset();
        }

        // Read one PCM frame from ring buffer; zero-fills if not enough data
        auto packet = std::make_unique<AudioStreamPacket>();
        packet->sample_rate = 16000;
        packet->frame_duration = 60;
        packet->payload.resize(frame_bytes);
        int16_t* pcm_buf = reinterpret_cast<int16_t*>(packet->payload.data());
        downlink_ring_buffer_->Read(pcm_buf, kDownlinkFrameSamples);

        // Store into ref ring buffer for downlink AEC (lock-free write)
        if (ref_ring_buffer_) {
            ref_ring_buffer_->Write(pcm_buf, kDownlinkFrameSamples);
        }

        // Push to AudioService decode queue
        if (on_incoming_audio_) {
            on_incoming_audio_(std::move(packet));
        }

        // Precise 60ms period: vTaskDelayUntil accounts for processing time
        // so the actual interval is always exactly 60ms regardless of code path latency
        vTaskDelayUntil(&last_wake, frame_ticks);
    }

    // Signal exit semaphore so CloseAudioChannel can wait deterministically
    if (downlink_exit_sem_) {
        xSemaphoreGive(downlink_exit_sem_);
    }
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

        if (err == DeviceApiError::kServerError) {
            ESP_LOGW(TAG, "Server error, retrying in 2s...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            if (!device_api_.StartConversation(info)) {
                err = device_api_.GetLastError();
                ESP_LOGE(TAG, "StartConversation retry failed, error=%d", (int)err);
                SetError(Lang::Strings::SERVER_NOT_CONNECTED);
                return false;
            }
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

    // Configure channel options: jitter buffer OFF, AI QoS ON
    rtc_channel_options_t options = {};
    options.auto_subscribe_audio = true;
    options.auto_subscribe_video = false;
    options.enable_audio_jitter_buffer = false;
    options.enable_audio_mixer = false;
    options.enable_audio_decode = true;
    options.enable_audio_ai_qos = AGORA_AI_QOS;
    options.enable_audio_downlink_aec = true;

    options.audio_codec_opt.audio_codec_type = AUDIO_CODEC_TYPE_G722;
    options.audio_codec_opt.pcm_sample_rate = 16000;
    options.audio_codec_opt.pcm_channel_num = 1;
    options.audio_codec_opt.pcm_duration = 60;

    ESP_LOGI(TAG, "Joining channel: %s, uid: %s, ai_qos: %d, token: %.8s...",
             info.rtc.channel.c_str(), info.rtc.uid.c_str(), AGORA_AI_QOS,
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

    // Start downlink processing task
    downlink_ring_buffer_->Reset();
    downlink_task_running_ = true;
    xTaskCreate(DownlinkTask, "agora_dl", 2048 * 4, this, 6, &downlink_task_handle_);

    if (on_audio_channel_opened_ != nullptr) {
        on_audio_channel_opened_();
    }

    return true;
}

void AgoraRtcProtocol::CloseAudioChannel(bool send_goodbye) {
    (void)send_goodbye;

    // Stop downlink task: set flag and wait for semaphore confirmation
    downlink_task_running_ = false;
    if (downlink_task_handle_ != nullptr) {
        if (downlink_exit_sem_) {
            // Block until DownlinkTaskLoop exits the while loop and gives the semaphore
            xSemaphoreTake(downlink_exit_sem_, portMAX_DELAY);
        }
        downlink_task_handle_ = nullptr;
    }

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

    // Clear ring buffers
    if (downlink_ring_buffer_) {
        downlink_ring_buffer_->Reset();
    }
    if (ref_ring_buffer_) {
        ref_ring_buffer_->Reset();
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

    if (packet->payload.size() != kExpectedFrameSize) {
        return true;
    }

    // Interleave mic + ref for downlink AEC: [mic1, ref1, mic2, ref2, ...]
    const int16_t* mic_data = (const int16_t*)packet->payload.data();
    size_t mic_samples = packet->payload.size() / sizeof(int16_t); // 960

    std::vector<int16_t> ref_data(mic_samples, 0);
    if (ref_ring_buffer_) {
        ref_ring_buffer_->Read(ref_data.data(), mic_samples);
    }

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

    if (data_len == 0 || data_len > 32000) {
        ESP_LOGW(TAG, "RecvAudio: invalid data_len=%d, skipping", (int)data_len);
        return;
    }

    // Write raw PCM into downlink ring buffer (lock-free, PSRAM)
    // Consumer is DownlinkTask reading at fixed 60ms intervals.
    g_instance->downlink_ring_buffer_->Write(
        static_cast<const int16_t*>(data_ptr), data_len / sizeof(int16_t));
}

void AgoraRtcProtocol::OnUserMuteAudio(connection_id_t conn_id, uint32_t uid, bool muted) {
    ESP_LOGI(TAG, "UserMuteAudio: uid=%lu, muted=%d", (unsigned long)uid, (int)muted);
    if (g_instance && muted) {
        // Don't Reset() here — Reset is not safe from the SDK thread because
        // downlink_ring_buffer_ producer (OnAudioData) and consumer (DownlinkTask)
        // may be mid-operation. Instead, flag the downlink task to clear in its own context.
        g_instance->downlink_clear_requested_.store(true, std::memory_order_release);
    }
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
