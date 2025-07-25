#include "esp_wake_word.h"
#include "application.h"

#include <esp_log.h>
#include <model_path.h>
#include <arpa/inet.h>
#include <sstream>

#define DETECTION_RUNNING_EVENT 1

#define TAG "EspWakeWord"

EspWakeWord::EspWakeWord() {
    event_group_ = xEventGroupCreate();
}

EspWakeWord::~EspWakeWord() {
    if (wakenet_data_ != nullptr) {
        wakenet_iface_->destroy(wakenet_data_);
        esp_srmodel_deinit(wakenet_model_);
    }

    vEventGroupDelete(event_group_);
}

bool EspWakeWord::Initialize(AudioCodec* codec) {
    codec_ = codec;

    wakenet_model_ = esp_srmodel_init("model");
    if (wakenet_model_ == nullptr || wakenet_model_->num == -1) {
        ESP_LOGE(TAG, "Failed to initialize wakenet model");
        return false;
    }
    if(wakenet_model_->num > 1) {
        ESP_LOGW(TAG, "More than one model found, using the first one");
    } else if (wakenet_model_->num == 0) {
        ESP_LOGE(TAG, "No model found");
        return false;
    }
    char *model_name = wakenet_model_->model_name[0];
    wakenet_iface_ = (esp_wn_iface_t*)esp_wn_handle_from_name(model_name);
    wakenet_data_ = wakenet_iface_->create(model_name, DET_MODE_95);

    int frequency = wakenet_iface_->get_samp_rate(wakenet_data_);
    int audio_chunksize = wakenet_iface_->get_samp_chunksize(wakenet_data_);
    ESP_LOGI(TAG, "Wake word(%s),freq: %d, chunksize: %d", model_name, frequency, audio_chunksize);

    return true;
}

void EspWakeWord::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    wake_word_detected_callback_ = callback;
}

void EspWakeWord::Start() {
    xEventGroupSetBits(event_group_, DETECTION_RUNNING_EVENT);
}

void EspWakeWord::Stop() {
    xEventGroupClearBits(event_group_, DETECTION_RUNNING_EVENT);
}

void EspWakeWord::Feed(const std::vector<int16_t>& data) {
    int res = wakenet_iface_->detect(wakenet_data_, (int16_t *)data.data());
    if (res > 0) {
        Stop();
        last_detected_wake_word_ = wakenet_iface_->get_word_name(wakenet_data_, res);

        if (wake_word_detected_callback_) {
            wake_word_detected_callback_(last_detected_wake_word_);
        }
    }
}

size_t EspWakeWord::GetFeedSize() {
    if (wakenet_data_ == nullptr) {
        return 0;
    }
    return wakenet_iface_->get_samp_chunksize(wakenet_data_) * codec_->input_channels();
}

void EspWakeWord::EncodeWakeWordData() {
}

bool EspWakeWord::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    return false;
}
