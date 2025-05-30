#ifndef WAKE_WORD_DETECT_H
#define WAKE_WORD_DETECT_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "model_path.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"

#include <list>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "audio_codec.h"
#include <model_path.h>

class WakeWordDetect {
public:
    WakeWordDetect();
    ~WakeWordDetect();

    void Initialize(AudioCodec* codec);
    void Feed(const std::vector<int16_t>& data);
    void StartDetection();
    void StopDetection();
    bool IsDetectionRunning();
    size_t GetFeedSize();

private:
    esp_wn_iface_t *wakenet_iface_ = nullptr;
    model_iface_data_t *wakenet_data_ = nullptr;
    srmodel_list_t *wakenet_model_ = nullptr;
    EventGroupHandle_t event_group_;
    AudioCodec* codec_ = nullptr;
};

#endif
