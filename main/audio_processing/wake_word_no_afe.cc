#include "wake_word_no_afe.h"
#include "application.h"

#include <esp_log.h>
#include <model_path.h>
#include <arpa/inet.h>
#include <sstream>

#define DETECTION_RUNNING_EVENT 1

static const char* TAG = "WakeWordDetect";

WakeWordDetect::WakeWordDetect() {
    event_group_ = xEventGroupCreate();
}

WakeWordDetect::~WakeWordDetect() {
    if (wakenet_data_ != nullptr) {
        wakenet_iface_->destroy(wakenet_data_);
        esp_srmodel_deinit(wakenet_model_);
    }

    vEventGroupDelete(event_group_);
}

void WakeWordDetect::Initialize(AudioCodec* codec) {
    codec_ = codec;

    wakenet_model_ = esp_srmodel_init("model");

    if(wakenet_model_->num > 1) {
        ESP_LOGW(TAG, "More than one model found, using the first one");
    }
    char *model_name = wakenet_model_->model_name[0];
    wakenet_iface_ = (esp_wn_iface_t*)esp_wn_handle_from_name(model_name);
    wakenet_data_ = wakenet_iface_->create(model_name, DET_MODE_95);

    int frequency = wakenet_iface_->get_samp_rate(wakenet_data_);
    int audio_chunksize = wakenet_iface_->get_samp_chunksize(wakenet_data_);
    ESP_LOGI(TAG, "Wake word(%s),freq: %d, chunksize: %d", model_name, frequency, audio_chunksize);
}

void WakeWordDetect::StartDetection() {
    xEventGroupSetBits(event_group_, DETECTION_RUNNING_EVENT);
}

void WakeWordDetect::StopDetection() {
    xEventGroupClearBits(event_group_, DETECTION_RUNNING_EVENT);
}

bool WakeWordDetect::IsDetectionRunning() {
    return xEventGroupGetBits(event_group_) & DETECTION_RUNNING_EVENT;
}

void WakeWordDetect::Feed(const std::vector<int16_t>& data) {
    int res = wakenet_iface_->detect(wakenet_data_, (int16_t *)data.data());
    if (res > 0) {
        ESP_LOGI(TAG, "Wake word detected");
        auto& app = Application::GetInstance();
        app.ToggleChatState();
    }
}

size_t WakeWordDetect::GetFeedSize() {

    return wakenet_iface_->get_samp_chunksize(wakenet_data_) * codec_->input_channels();
}
