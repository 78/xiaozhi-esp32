#include "custom_wake_word.h"
#include "application.h"

#include <esp_log.h>
#include <model_path.h>
#include <arpa/inet.h>
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_afe_sr_models.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include <sstream>

#define DETECTION_RUNNING_EVENT 1

#define TAG "CustomWakeWord"


CustomWakeWord::CustomWakeWord()
    : afe_data_(nullptr),
      wake_word_pcm_(),
      wake_word_opus_() {

    event_group_ = xEventGroupCreate();
}

CustomWakeWord::~CustomWakeWord() {
    if (afe_data_ != nullptr) {
        afe_iface_->destroy(afe_data_);
    }

    // 清理 multinet 资源
    if (multinet_model_data_ != nullptr && multinet_ != nullptr) {
        multinet_->destroy(multinet_model_data_);
        multinet_model_data_ = nullptr;
    }

    if (wake_word_encode_task_stack_ != nullptr) {
        heap_caps_free(wake_word_encode_task_stack_);
    }

    vEventGroupDelete(event_group_);
}

bool CustomWakeWord::Initialize(AudioCodec* codec) {
    codec_ = codec;

    models = esp_srmodel_init("model");
    if (models == nullptr || models->num == -1) {
        ESP_LOGE(TAG, "Failed to initialize wakenet model");
        return false;
    }

    // 初始化 multinet (命令词识别)
    mn_name_ = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_CHINESE);
    if (mn_name_ == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize multinet, mn_name is nullptr");
        ESP_LOGI(TAG, "Please refer to https://pcn7cs20v8cr.feishu.cn/wiki/CpQjwQsCJiQSWSkYEvrcxcbVnwh to add custom wake word");
        return false;
    }

    ESP_LOGI(TAG, "multinet:%s", mn_name_);
    multinet_ = esp_mn_handle_from_name(mn_name_);
    multinet_model_data_ = multinet_->create(mn_name_, 2000);  // 2秒超时
    multinet_->set_det_threshold(multinet_model_data_, 0.5);
    esp_mn_commands_clear();
    esp_mn_commands_add(1, CONFIG_CUSTOM_WAKE_WORD);  // 添加自定义唤醒词作为命令词
    esp_mn_commands_update();
    
    // 打印所有的命令词
    multinet_->print_active_speech_commands(multinet_model_data_);
    ESP_LOGI(TAG, "Custom wake word: %s", CONFIG_CUSTOM_WAKE_WORD);

    // 初始化 afe
    int ref_num = codec_->input_reference() ? 1 : 0;
    std::string input_format;
    for (int i = 0; i < codec_->input_channels() - ref_num; i++) {
        input_format.push_back('M');
    }
    for (int i = 0; i < ref_num; i++) {
        input_format.push_back('R');
    }

    afe_config_t* afe_config = afe_config_init(input_format.c_str(), models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    afe_config->aec_init = codec_->input_reference();
    afe_config->aec_mode = AEC_MODE_SR_HIGH_PERF;
    afe_config->afe_perferred_core = 1;
    afe_config->afe_perferred_priority = 1;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    
    afe_iface_ = esp_afe_handle_from_config(afe_config);
    afe_data_ = afe_iface_->create_from_config(afe_config);

    xTaskCreate([](void* arg) {
        auto this_ = (CustomWakeWord*)arg;
        this_->AudioDetectionTask();
        vTaskDelete(NULL);
    }, "audio_detection", 16384, this, 3, nullptr);

    return true;
}

void CustomWakeWord::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    wake_word_detected_callback_ = callback;
}

void CustomWakeWord::Start() {
    xEventGroupSetBits(event_group_, DETECTION_RUNNING_EVENT);
}

void CustomWakeWord::Stop() {
    xEventGroupClearBits(event_group_, DETECTION_RUNNING_EVENT);
    if (afe_data_ != nullptr) {
        afe_iface_->reset_buffer(afe_data_);
    }
}

void CustomWakeWord::Feed(const std::vector<int16_t>& data) {
    if (afe_data_ == nullptr) {
        return;
    }
    afe_iface_->feed(afe_data_, data.data());
}

size_t CustomWakeWord::GetFeedSize() {
    if (afe_data_ == nullptr) {
        return 0;
    }
    return afe_iface_->get_feed_chunksize(afe_data_) * codec_->input_channels();
}

void CustomWakeWord::AudioDetectionTask() {
    auto fetch_size = afe_iface_->get_fetch_chunksize(afe_data_);
    auto feed_size = afe_iface_->get_feed_chunksize(afe_data_);

    // 检查 multinet 是否已正确初始化
    if (multinet_ == nullptr || multinet_model_data_ == nullptr) {
        ESP_LOGE(TAG, "Multinet not initialized properly");
        return;
    }

    int mu_chunksize = multinet_->get_samp_chunksize(multinet_model_data_);
    assert(mu_chunksize == feed_size);

    ESP_LOGI(TAG, "Audio detection task started, feed size: %d fetch size: %d", feed_size, fetch_size);

    // 禁用wakenet，直接使用multinet检测自定义唤醒词
    afe_iface_->disable_wakenet(afe_data_);

    while (true) {
        xEventGroupWaitBits(event_group_, DETECTION_RUNNING_EVENT, pdFALSE, pdTRUE, portMAX_DELAY);

        auto res = afe_iface_->fetch_with_delay(afe_data_, portMAX_DELAY);
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            ESP_LOGW(TAG, "Fetch failed, continue");
            continue;
        }

        // 存储音频数据用于语音识别
        StoreWakeWordData(res->data, res->data_size / sizeof(int16_t));

        // 直接使用multinet检测自定义唤醒词
        esp_mn_state_t mn_state = multinet_->detect(multinet_model_data_, res->data);
        
        if (mn_state == ESP_MN_STATE_DETECTING) {
            // 仍在检测中，继续
            continue;
        } else if (mn_state == ESP_MN_STATE_DETECTED) {
            // 检测到自定义唤醒词
            esp_mn_results_t *mn_result = multinet_->get_results(multinet_model_data_);
            ESP_LOGI(TAG, "Custom wake word detected: command_id=%d, string=%s, prob=%f", 
                    mn_result->command_id[0], mn_result->string, mn_result->prob[0]);
            
            if (mn_result->command_id[0] == 1) {  // 自定义唤醒词
                ESP_LOGI(TAG, "Custom wake word '%s' detected successfully!", CONFIG_CUSTOM_WAKE_WORD);
                
                // 停止检测
                Stop();
                last_detected_wake_word_ = CONFIG_CUSTOM_WAKE_WORD_DISPLAY;
                
                // 调用回调
                if (wake_word_detected_callback_) {
                    wake_word_detected_callback_(last_detected_wake_word_);
                }
                
                // 清理multinet状态，准备下次检测
                multinet_->clean(multinet_model_data_);
                ESP_LOGI(TAG, "Ready for next detection");
            }
        } else if (mn_state == ESP_MN_STATE_TIMEOUT) {
            // 超时，清理状态继续检测
            ESP_LOGD(TAG, "Command word detection timeout, cleaning state");
            multinet_->clean(multinet_model_data_);
            continue;
        }
    }
    
    ESP_LOGI(TAG, "Audio detection task ended");
}

void CustomWakeWord::StoreWakeWordData(const int16_t* data, size_t samples) {
    // store audio data to wake_word_pcm_
    wake_word_pcm_.emplace_back(std::vector<int16_t>(data, data + samples));
    // keep about 2 seconds of data, detect duration is 30ms (sample_rate == 16000, chunksize == 512)
    while (wake_word_pcm_.size() > 2000 / 30) {
        wake_word_pcm_.pop_front();
    }
}

void CustomWakeWord::EncodeWakeWordData() {
    wake_word_opus_.clear();
    if (wake_word_encode_task_stack_ == nullptr) {
        wake_word_encode_task_stack_ = (StackType_t*)heap_caps_malloc(4096 * 8, MALLOC_CAP_SPIRAM);
    }
    wake_word_encode_task_ = xTaskCreateStatic([](void* arg) {
        auto this_ = (CustomWakeWord*)arg;
        {
            auto start_time = esp_timer_get_time();
            auto encoder = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);
            encoder->SetComplexity(0); // 0 is the fastest

            int packets = 0;
            for (auto& pcm: this_->wake_word_pcm_) {
                encoder->Encode(std::move(pcm), [this_](std::vector<uint8_t>&& opus) {
                    std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);
                    this_->wake_word_opus_.emplace_back(std::move(opus));
                    this_->wake_word_cv_.notify_all();
                });
                packets++;
            }
            this_->wake_word_pcm_.clear();

            auto end_time = esp_timer_get_time();
            ESP_LOGI(TAG, "Encode wake word opus %d packets in %ld ms", packets, (long)((end_time - start_time) / 1000));

            std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);
            this_->wake_word_opus_.push_back(std::vector<uint8_t>());
            this_->wake_word_cv_.notify_all();
        }
        vTaskDelete(NULL);
    }, "encode_detect_packets", 4096 * 8, this, 2, wake_word_encode_task_stack_, &wake_word_encode_task_buffer_);
}

bool CustomWakeWord::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    std::unique_lock<std::mutex> lock(wake_word_mutex_);
    wake_word_cv_.wait(lock, [this]() {
        return !wake_word_opus_.empty();
    });
    opus.swap(wake_word_opus_.front());
    wake_word_opus_.pop_front();
    return !opus.empty();
}
