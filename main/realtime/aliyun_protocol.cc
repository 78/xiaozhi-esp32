#include "aliyun_protocol.h"

#include <esp_log.h>
#include <cJSON.h>
#include <mbedtls/base64.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "application.h"
#include "board.h"
#include "debug_trace.h"

namespace {
constexpr char kTag[] = "AliyunRealtime";
constexpr EventBits_t kCreated = BIT0;
constexpr EventBits_t kUpdated = BIT1;
constexpr EventBits_t kError = BIT2;

const char* StringValue(const cJSON* object, const char* name) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : nullptr;
}

std::string SessionUpdate(bool manual) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "session.update");
    cJSON* session = cJSON_AddObjectToObject(root, "session");
    cJSON* modalities = cJSON_AddArrayToObject(session, "modalities");
    cJSON_AddItemToArray(modalities, cJSON_CreateString("text"));
    cJSON_AddItemToArray(modalities, cJSON_CreateString("audio"));
    if (manual) {
        cJSON_AddNullToObject(session, "turn_detection");
    } else {
        cJSON* vad = cJSON_AddObjectToObject(session, "turn_detection");
        cJSON_AddStringToObject(vad, "type", "server_vad");
        cJSON_AddNumberToObject(vad, "silence_duration_ms", 800);
    }
    cJSON* transcription = cJSON_AddObjectToObject(session, "input_audio_transcription");
    cJSON_AddStringToObject(transcription, "model", "qwen3-asr-flash-realtime");
    cJSON* audio = cJSON_AddObjectToObject(session, "audio");
    cJSON* input = cJSON_AddObjectToObject(audio, "input");
    cJSON* format = cJSON_AddObjectToObject(input, "format");
    cJSON_AddStringToObject(format, "type", "pcm");
    cJSON_AddNumberToObject(format, "sample_rate", 16000);
    cJSON_AddStringToObject(format, "sample_format", "s16le");
    cJSON_AddNumberToObject(format, "channels", 1);
    cJSON_AddStringToObject(format, "packing", "interleaved");
    cJSON_AddStringToObject(format, "channel_layout", "mono");
    cJSON* output = cJSON_AddObjectToObject(audio, "output");
    cJSON_AddStringToObject(output, "voice", "Tina");
    format = cJSON_AddObjectToObject(output, "format");
    cJSON_AddStringToObject(format, "type", "pcm");
    cJSON_AddNumberToObject(format, "sample_rate", 24000);
    char* json = cJSON_PrintUnformatted(root);
    std::string result = json ? json : "";
    cJSON_free(json);
    cJSON_Delete(root);
    return result;
}
}  // namespace

AliyunProtocol::AliyunProtocol(AliyunRealtimeConfig config) : config_(std::move(config)) {
    ready_events_ = xEventGroupCreate();
    DebugTrace::Instance().Configure(config_);
    server_sample_rate_ = 24000;
    server_frame_duration_ = 20;
}

AliyunProtocol::~AliyunProtocol() {
    CloseAudioChannel();
    vEventGroupDelete(ready_events_);
}

bool AliyunProtocol::Start() { return true; }

bool AliyunProtocol::OpenAudioChannel() {
    if (!config_.IsReady())
        return false;
    xEventGroupClearBits(ready_events_, kCreated | kUpdated | kError);
    error_occurred_ = false;
    DebugTrace::Instance().Open("manual");
    trace_open_.store(true);
    const uint32_t generation = ++connection_generation_;
    websocket_ = Board::GetInstance().GetNetwork()->CreateWebSocket(1);
    if (!websocket_) {
        CloseAudioChannel();
        return false;
    }
    std::string authorization = "Bearer " + config_.api_key;
    websocket_->SetHeader("Authorization", authorization.c_str());
    websocket_->OnData([this](const char* data, size_t len, bool binary) {
        if (!binary && len <= 256 * 1024) {
            DebugTrace::Instance().WebSocketRx(data, len);
            HandleEvent(data, len);
        }
    });
    std::weak_ptr<WebSocket> weak = websocket_;
    websocket_->OnPing([weak](const char* data, size_t len) {
        Application::GetInstance().Schedule([weak, value = std::string(data, len)]() {
            if (auto socket = weak.lock())
                socket->SendPong(value.data(), value.size());
        });
    });
    websocket_->OnDisconnected([this, generation]() {
        if (generation != connection_generation_.load())
            return;
        xEventGroupSetBits(ready_events_, kError);
        listening_.store(false);
        if (trace_open_.exchange(false))
            DebugTrace::Instance().Close();
        if (on_audio_channel_closed_)
            on_audio_channel_closed_();
    });
    const auto url = config_.WebSocketUrl();
    auto connected = websocket_->Connect(url.c_str());
    if (!connected) {
        ESP_LOGE(kTag, "WebSocket connect failed: %s", connected.error().ToString().c_str());
        CloseAudioChannel();
        return false;
    }
    auto bits = xEventGroupWaitBits(ready_events_, kCreated | kError, pdTRUE, pdFALSE,
                                    pdMS_TO_TICKS(10000));
    if (!(bits & kCreated) || (bits & kError)) {
        CloseAudioChannel();
        return false;
    }
    if (!SendText(SessionUpdate(true))) {
        CloseAudioChannel();
        return false;
    }
    bits = xEventGroupWaitBits(ready_events_, kUpdated | kError, pdTRUE, pdFALSE,
                               pdMS_TO_TICKS(10000));
    if (!(bits & kUpdated) || (bits & kError)) {
        CloseAudioChannel();
        return false;
    }
    if (on_audio_channel_opened_)
        on_audio_channel_opened_();
    return true;
}

void AliyunProtocol::CloseAudioChannel(bool) {
    listening_.store(false);
    waiting_commit_.store(false);
    response_active_.store(false);
    websocket_.reset();
    if (trace_open_.exchange(false)) {
        DebugTrace::Instance().Close();
    }
}

bool AliyunProtocol::IsAudioChannelOpened() const {
    return websocket_ && websocket_->IsConnected() && !error_occurred_;
}

bool AliyunProtocol::SendText(const std::string& text) {
    bool sent = IsAudioChannelOpened() && websocket_->Send(text);
    DebugTrace::Instance().WebSocketTx(text.data(), text.size(), sent);
    return sent;
}

bool AliyunProtocol::SendAudio(std::unique_ptr<AudioStreamPacket> packet) {
    if (!listening_.load() || !packet || !packet->pcm || packet->payload.empty() ||
        packet->payload.size() > 19200)
        return false;
    size_t encoded_length = 0;
    std::string encoded(4 * ((packet->payload.size() + 2) / 3) + 1, '\0');
    if (mbedtls_base64_encode(reinterpret_cast<unsigned char*>(encoded.data()), encoded.size(),
                              &encoded_length, packet->payload.data(), packet->payload.size()) != 0)
        return false;
    encoded.resize(encoded_length);
    DebugTrace::Instance().Pcm(true, packet->payload.data(), packet->payload.size(), 16000);
    return SendText("{\"type\":\"input_audio_buffer.append\",\"audio\":\"" + encoded + "\"}");
}

void AliyunProtocol::SendStartListening(ListeningMode mode) {
    const bool manual = mode == kListeningModeManualStop;
    if (mode == kListeningModeRealtime)
        return;
    if (manual_.load() != manual) {
        if (!SendText(SessionUpdate(manual)))
            return;
        manual_.store(manual);
    }
    response_active_.store(false);
    listening_.store(true);
    DebugTrace::Instance().SetMode(manual ? "manual" : "auto");
    DebugTrace::Instance().Event("listening_start", manual ? "manual" : "auto");
}

void AliyunProtocol::SendStopListening() {
    listening_.store(false);
    DebugTrace::Instance().Event("listening_stop");
    if (manual_.load() && !waiting_commit_.exchange(true)) {
        if (!SendText("{\"type\":\"input_audio_buffer.commit\"}"))
            waiting_commit_.store(false);
    }
}

void AliyunProtocol::SendAbortSpeaking(AbortReason) {
    listening_.store(false);
    if (response_active_.load())
        SendText("{\"type\":\"response.cancel\"}");
}

void AliyunProtocol::SendWakeWordDetected(const std::string&) {}
void AliyunProtocol::SendMcpMessage(const std::string&) {}

void AliyunProtocol::HandleEvent(const char* data, size_t size) {
    cJSON* root = cJSON_ParseWithLength(data, size);
    if (!root)
        return;
    const char* type = StringValue(root, "type");
    if (!type) {
        cJSON_Delete(root);
        return;
    }
    if (strcmp(type, "session.created") == 0) {
        auto session = cJSON_GetObjectItemCaseSensitive(root, "session");
        const char* id = StringValue(session, "id");
        if (id)
            session_id_ = id;
        xEventGroupSetBits(ready_events_, kCreated);
    } else if (strcmp(type, "session.updated") == 0) {
        xEventGroupSetBits(ready_events_, kUpdated);
    } else if (strcmp(type, "input_audio_buffer.committed") == 0) {
        if (waiting_commit_.exchange(false)) {
            Application::GetInstance().Schedule(
                [this]() { SendText("{\"type\":\"response.create\"}"); });
        }
    } else if (strcmp(type, "response.created") == 0) {
        const char* id = StringValue(cJSON_GetObjectItemCaseSensitive(root, "response"), "id");
        if (id)
            response_id_ = id;
        response_active_.store(true);
        listening_.store(false);
        if (on_response_started_)
            on_response_started_();
    } else if (strcmp(type, "response.audio.delta") == 0) {
        ESP_LOGI(kTag, "PCM delta received");
        const char* delta = StringValue(root, "delta");
        const char* id = StringValue(root, "response_id");
        if (delta && id && response_id_ == id && on_incoming_audio_) {
            size_t length = strlen(delta);
            if (length <= 128 * 1024) {
                std::vector<uint8_t> pcm(length * 3 / 4 + 4);
                size_t count = 0;
                if (mbedtls_base64_decode(pcm.data(), pcm.size(), &count,
                                          reinterpret_cast<const unsigned char*>(delta),
                                          length) == 0 &&
                    count % 2 == 0) {
                    DebugTrace::Instance().Pcm(false, pcm.data(), count, 24000);
                    for (size_t offset = 0; offset < count; offset += 4800) {
                        auto packet = std::make_unique<AudioStreamPacket>();
                        packet->pcm = true;
                        packet->sample_rate = 24000;
                        packet->frame_duration = 100;
                        size_t end = std::min(offset + 4800, count);
                        packet->payload.assign(pcm.begin() + offset, pcm.begin() + end);
                        on_incoming_audio_(std::move(packet));
                    }
                }
            }
        }
    } else if (strcmp(type, "conversation.item.input_audio_transcription.completed") == 0 ||
               strcmp(type, "response.audio_transcript.done") == 0) {
        const char* value = StringValue(root, "transcript");
        if (value && on_transcript_)
            on_transcript_(type[0] == 'c', value);
    } else if (strcmp(type, "response.done") == 0) {
        const char* id = StringValue(cJSON_GetObjectItemCaseSensitive(root, "response"), "id");
        if (id && response_id_ == id) {
            response_active_.store(false);
            if (on_response_done_)
                on_response_done_();
        }
    } else if (strcmp(type, "error") == 0) {
        const cJSON* error = cJSON_GetObjectItemCaseSensitive(root, "error");
        const char* code = StringValue(error, "code");
        ESP_LOGE(kTag, "Cloud error: %s", code ? code : "unknown");
        xEventGroupSetBits(ready_events_, kError);
        SetError(code ? code : "Aliyun realtime error");
    }
    cJSON_Delete(root);
}
