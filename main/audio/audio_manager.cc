#include "audio/audio_manager.h"
#include "audio_service.h"
#include "esp_log.h"

static const char* TAG = "AudioManager";

AudioManager& AudioManager::GetInstance() {
    static AudioManager inst;
    return inst;
}

void AudioManager::Init() {
    ESP_LOGI(TAG, "AudioManager init");
}

bool AudioManager::StartRecording() {
    ESP_LOGI(TAG, "StartRecording (skeleton)");
    return true;
}

bool AudioManager::StopRecording() {
    ESP_LOGI(TAG, "StopRecording (skeleton)");
    if (on_recording_finished_) {
        on_recording_finished_(std::vector<uint8_t>{});
    }
    return true;
}

bool AudioManager::PlayPcm(const uint8_t* data, size_t len) {
    ESP_LOGI(TAG, "PlayPcm (skeleton), len=%u", (unsigned)len);
    (void)data; (void)len;
    return true;
}

void AudioManager::RegisterOnRecordingFinished(std::function<void(std::vector<uint8_t>)> cb) {
    on_recording_finished_ = cb;
}
