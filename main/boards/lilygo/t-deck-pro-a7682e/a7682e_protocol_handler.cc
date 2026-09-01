#include "a7682e_protocol_handler.h"

#include <cstring>
#include <utility>

#include <esp_log.h>

namespace {

constexpr char TAG[] = "A7682eProtocol";

}  // namespace

A7682eProtocolHandler::A7682eProtocolHandler(A7682eModem& modem) : modem_(modem) {}

void A7682eProtocolHandler::SetApplicationContext(BoardProtocolContext context) {
    context_ = std::move(context);
}

bool A7682eProtocolHandler::HandleIncomingJson(const cJSON* root) {
    auto type = cJSON_GetObjectItem(root, "type");
    if (!cJSON_IsString(type) || strcmp(type->valuestring, "tts") != 0) {
        return false;
    }

    auto state = cJSON_GetObjectItem(root, "state");
    if (!cJSON_IsString(state)) {
        return false;
    }

    if (strcmp(state->valuestring, "start") == 0) {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        const uint32_t generation = tts_generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
        accepting_tts_sentences_.store(true, std::memory_order_release);
        active_tts_generation_.store(generation, std::memory_order_release);
        local_tts_drain_generation_.store(0, std::memory_order_release);
        discarded_audio_frame_count_.store(0, std::memory_order_relaxed);
        discarded_audio_byte_count_.store(0, std::memory_order_relaxed);
        modem_.StopSpeech();
        return false;
    }

    if (strcmp(state->valuestring, "sentence_start") == 0) {
        auto text = cJSON_GetObjectItem(root, "text");
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        const uint32_t generation = tts_generation_.load(std::memory_order_acquire);
        const bool can_submit = cJSON_IsString(text) && generation != 0 &&
                                accepting_tts_sentences_.load(std::memory_order_acquire) &&
                                IsCurrentGeneration(generation);
        if (can_submit && !modem_.SpeakText(text->valuestring)) {
            ESP_LOGW(TAG, "TTS queue is full or text is invalid");
        } else if (!cJSON_IsString(text)) {
            ESP_LOGW(TAG, "TTS sentence has no text; server audio will be discarded");
        }
        return false;
    }

    if (strcmp(state->valuestring, "stop") == 0) {
        std::lock_guard<std::mutex> lock(lifecycle_mutex_);
        accepting_tts_sentences_.store(false, std::memory_order_release);
        const uint32_t generation = active_tts_generation_.load(std::memory_order_acquire);
        if (generation == 0) {
            return false;
        }
        local_tts_drain_generation_.store(generation, std::memory_order_release);
        return true;
    }

    return false;
}

bool A7682eProtocolHandler::HandleIncomingAudio(std::unique_ptr<AudioStreamPacket>& packet) {
    if (packet != nullptr) {
        discarded_audio_frame_count_.fetch_add(1, std::memory_order_relaxed);
        discarded_audio_byte_count_.fetch_add(static_cast<uint32_t>(packet->payload.size()),
                                              std::memory_order_relaxed);
    }
    packet.reset();
    return true;
}

void A7682eProtocolHandler::OnAudioChannelClosed() { CancelSpeech(); }

void A7682eProtocolHandler::OnAbortSpeaking() {
    const uint32_t cancellation_generation = CancelSpeech();
    if (context_.schedule && context_.on_speech_finished) {
        context_.schedule(
            [this, cancellation_generation, on_speech_finished = context_.on_speech_finished]() {
                if (tts_generation_.load(std::memory_order_acquire) == cancellation_generation) {
                    on_speech_finished();
                }
            });
    }
}

void A7682eProtocolHandler::OnApplicationTick() {
    std::unique_lock<std::mutex> lock(lifecycle_mutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
        return;
    }

    const uint32_t generation = local_tts_drain_generation_.load(std::memory_order_acquire);
    if (generation == 0) {
        return;
    }
    if (!IsCurrentGeneration(generation) ||
        accepting_tts_sentences_.load(std::memory_order_acquire)) {
        local_tts_drain_generation_.store(0, std::memory_order_release);
        return;
    }
    if (modem_.IsSpeechBusy()) {
        return;
    }
    if (!IsCurrentGeneration(generation) ||
        accepting_tts_sentences_.load(std::memory_order_acquire)) {
        local_tts_drain_generation_.store(0, std::memory_order_release);
        return;
    }

    uint32_t expected_drain_generation = generation;
    if (!local_tts_drain_generation_.compare_exchange_strong(expected_drain_generation, 0,
                                                             std::memory_order_acq_rel)) {
        return;
    }
    uint32_t expected_active_generation = generation;
    if (!active_tts_generation_.compare_exchange_strong(expected_active_generation, 0,
                                                        std::memory_order_acq_rel)) {
        return;
    }
    if (tts_generation_.load(std::memory_order_acquire) != generation) {
        return;
    }

    const uint32_t frame_count =
        discarded_audio_frame_count_.exchange(0, std::memory_order_relaxed);
    const uint32_t byte_count = discarded_audio_byte_count_.exchange(0, std::memory_order_relaxed);
    ESP_LOGI(TAG, "Local TTS drained; discarded server audio frames=%u bytes=%u", frame_count,
             byte_count);
    if (context_.on_speech_finished) {
        context_.on_speech_finished();
    }
}

bool A7682eProtocolHandler::IsCurrentGeneration(uint32_t generation) const {
    return tts_generation_.load(std::memory_order_acquire) == generation &&
           active_tts_generation_.load(std::memory_order_acquire) == generation;
}

uint32_t A7682eProtocolHandler::CancelSpeech() {
    std::lock_guard<std::mutex> lock(lifecycle_mutex_);
    const uint32_t cancellation_generation =
        tts_generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
    active_tts_generation_.store(0, std::memory_order_release);
    accepting_tts_sentences_.store(false, std::memory_order_release);
    local_tts_drain_generation_.store(0, std::memory_order_release);
    discarded_audio_frame_count_.store(0, std::memory_order_relaxed);
    discarded_audio_byte_count_.store(0, std::memory_order_relaxed);
    modem_.StopSpeech();
    return cancellation_generation;
}
