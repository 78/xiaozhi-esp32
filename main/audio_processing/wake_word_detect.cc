#include "wake_word_detect.h"
#include "application.h"

#include <esp_log.h>
#include <model_path.h>
#include <arpa/inet.h>
#include <sstream>

#define DETECTION_RUNNING_EVENT 1 // 定义事件标志位，表示检测任务正在运行

static const char* TAG = "WakeWordDetect"; // 日志标签

// 构造函数，初始化事件组和相关数据结构
WakeWordDetect::WakeWordDetect()
    : afe_detection_data_(nullptr), // 初始化AFE检测数据为空
      wake_word_pcm_(), // 初始化PCM数据存储
      wake_word_opus_() { // 初始化Opus编码数据存储

    event_group_ = xEventGroupCreate(); // 创建事件组
}

// 析构函数，释放资源
WakeWordDetect::~WakeWordDetect() {
    if (afe_detection_data_ != nullptr) {
        esp_afe_sr_v1.destroy(afe_detection_data_); // 销毁AFE检测数据
    }

    if (wake_word_encode_task_stack_ != nullptr) {
        heap_caps_free(wake_word_encode_task_stack_); // 释放编码任务栈内存
    }

    vEventGroupDelete(event_group_); // 删除事件组
}

// 初始化函数，配置AFE（音频前端）并启动音频检测任务
void WakeWordDetect::Initialize(int channels, bool reference) {
    channels_ = channels; // 设置音频通道数
    reference_ = reference; // 设置是否使用参考信号
    int ref_num = reference_ ? 1 : 0; // 根据是否使用参考信号确定参考信号数量

    // 初始化唤醒词模型
    srmodel_list_t *models = esp_srmodel_init("model");
    for (int i = 0; i < models->num; i++) {
        ESP_LOGI(TAG, "Model %d: %s", i, models->model_name[i]); // 日志：输出模型名称
        if (strstr(models->model_name[i], ESP_WN_PREFIX) != NULL) {
            wakenet_model_ = models->model_name[i]; // 设置唤醒词模型
            auto words = esp_srmodel_get_wake_words(models, wakenet_model_); // 获取唤醒词
            // 按分号分割唤醒词
            std::stringstream ss(words);
            std::string word;
            while (std::getline(ss, word, ';')) {
                wake_words_.push_back(word); // 将唤醒词存入列表
            }
        }
    }

    // AFE配置结构体
    afe_config_t afe_config = {
        .aec_init = reference_, // 是否启用回声消除
        .se_init = true, // 启用语音增强
        .vad_init = true, // 启用语音活动检测
        .wakenet_init = true, // 启用唤醒词检测
        .voice_communication_init = false, // 不启用语音通信模式
        .voice_communication_agc_init = false, // 不启用自动增益控制
        .voice_communication_agc_gain = 10, // 自动增益控制增益值
        .vad_mode = VAD_MODE_3, // 语音活动检测模式
        .wakenet_model_name = wakenet_model_, // 唤醒词模型名称
        .wakenet_model_name_2 = NULL, // 第二个唤醒词模型名称
        .wakenet_mode = DET_MODE_90, // 唤醒词检测模式
        .afe_mode = SR_MODE_HIGH_PERF, // AFE高性能模式
        .afe_perferred_core = 1, // AFE任务运行的核心
        .afe_perferred_priority = 1, // AFE任务的优先级
        .afe_ringbuf_size = 50, // AFE环形缓冲区大小
        .memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM, // 内存分配模式（优先使用PSRAM）
        .afe_linear_gain = 1.0, // AFE线性增益
        .agc_mode = AFE_MN_PEAK_AGC_MODE_2, // 自动增益控制模式
        .pcm_config = {
            .total_ch_num = channels_, // 总通道数
            .mic_num = channels_ - ref_num, // 麦克风数量
            .ref_num = ref_num, // 参考信号数量
            .sample_rate = 16000 // 采样率
        },
        .debug_init = false, // 不启用调试模式
        .debug_hook = {{ AFE_DEBUG_HOOK_MASE_TASK_IN, NULL }, { AFE_DEBUG_HOOK_FETCH_TASK_IN, NULL }}, // 调试钩子
        .afe_ns_mode = NS_MODE_SSP, // 噪声抑制模式
        .afe_ns_model_name = NULL, // 噪声抑制模型名称
        .fixed_first_channel = true, // 固定第一个通道
    };

    // 根据配置创建AFE检测数据
    afe_detection_data_ = esp_afe_sr_v1.create_from_config(&afe_config);

    // 创建音频检测任务
    xTaskCreate([](void* arg) {
        auto this_ = (WakeWordDetect*)arg;
        this_->AudioDetectionTask(); // 运行音频检测任务
        vTaskDelete(NULL); // 任务完成后删除任务
    }, "audio_detection", 4096 * 2, this, 2, nullptr);
}

// 设置唤醒词检测回调函数
void WakeWordDetect::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    wake_word_detected_callback_ = callback; // 设置唤醒词检测回调函数
}

// 设置语音活动检测状态变化回调函数
void WakeWordDetect::OnVadStateChange(std::function<void(bool speaking)> callback) {
    vad_state_change_callback_ = callback; // 设置语音活动检测状态变化回调函数
}

// 启动检测任务
void WakeWordDetect::StartDetection() {
    xEventGroupSetBits(event_group_, DETECTION_RUNNING_EVENT); // 设置事件标志位，表示检测任务正在运行
}

// 停止检测任务
void WakeWordDetect::StopDetection() {
    xEventGroupClearBits(event_group_, DETECTION_RUNNING_EVENT); // 清除事件标志位，表示检测任务停止运行
}

// 检查检测任务是否正在运行
bool WakeWordDetect::IsDetectionRunning() {
    return xEventGroupGetBits(event_group_) & DETECTION_RUNNING_EVENT; // 获取事件标志位并检查是否正在运行
}

// 输入音频数据
void WakeWordDetect::Feed(const std::vector<int16_t>& data) {
    input_buffer_.insert(input_buffer_.end(), data.begin(), data.end()); // 将数据插入输入缓冲区

    auto feed_size = esp_afe_sr_v1.get_feed_chunksize(afe_detection_data_) * channels_; // 获取每次喂入AFE的数据大小
    while (input_buffer_.size() >= feed_size) {
        esp_afe_sr_v1.feed(afe_detection_data_, input_buffer_.data()); // 将数据喂入AFE
        input_buffer_.erase(input_buffer_.begin(), input_buffer_.begin() + feed_size); // 删除已处理的数据
    }
}

// 音频检测任务
void WakeWordDetect::AudioDetectionTask() {
    auto fetch_size = esp_afe_sr_v1.get_fetch_chunksize(afe_detection_data_); // 获取每次从AFE获取的数据大小
    auto feed_size = esp_afe_sr_v1.get_feed_chunksize(afe_detection_data_); // 获取每次喂入AFE的数据大小
    ESP_LOGI(TAG, "Audio detection task started, feed size: %d fetch size: %d",
        feed_size, fetch_size); // 日志：音频检测任务启动

    while (true) {
        xEventGroupWaitBits(event_group_, DETECTION_RUNNING_EVENT, pdFALSE, pdTRUE, portMAX_DELAY); // 等待检测任务运行标志位

        auto res = esp_afe_sr_v1.fetch(afe_detection_data_); // 从AFE获取处理后的数据
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            if (res != nullptr) {
                ESP_LOGI(TAG, "Error code: %d", res->ret_value); // 日志：错误代码
            }
            continue; // 如果获取数据失败，则继续
        }

        // 存储唤醒词数据用于语音识别，例如识别说话者
        StoreWakeWordData((uint16_t*)res->data, res->data_size / sizeof(uint16_t));

        // 语音活动检测状态变化
        if (vad_state_change_callback_) {
            if (res->vad_state == AFE_VAD_SPEECH && !is_speaking_) {
                is_speaking_ = true;
                vad_state_change_callback_(true); // 调用语音活动检测状态变化回调函数
            } else if (res->vad_state == AFE_VAD_SILENCE && is_speaking_) {
                is_speaking_ = false;
                vad_state_change_callback_(false); // 调用语音活动检测状态变化回调函数
            }
        }

        // 检测到唤醒词
        if (res->wakeup_state == WAKENET_DETECTED) {
            StopDetection(); // 停止检测任务
            last_detected_wake_word_ = wake_words_[res->wake_word_index - 1]; // 获取检测到的唤醒词

            if (wake_word_detected_callback_) {
                wake_word_detected_callback_(last_detected_wake_word_); // 调用唤醒词检测回调函数
            }
        }
    }
}

// 存储唤醒词数据
void WakeWordDetect::StoreWakeWordData(uint16_t* data, size_t samples) {
    // 将音频数据存储到wake_word_pcm_
    wake_word_pcm_.emplace_back(std::vector<int16_t>(data, data + samples));
    // 保留约2秒的数据，检测时长为32ms（采样率为16000，每次处理512个样本）
    while (wake_word_pcm_.size() > 2000 / 32) {
        wake_word_pcm_.pop_front(); // 删除最早的数据
    }
}

// 编码唤醒词数据
void WakeWordDetect::EncodeWakeWordData() {
    wake_word_opus_.clear(); // 清空Opus编码数据
    if (wake_word_encode_task_stack_ == nullptr) {
        wake_word_encode_task_stack_ = (StackType_t*)heap_caps_malloc(4096 * 8, MALLOC_CAP_SPIRAM); // 分配编码任务栈内存
    }
    wake_word_encode_task_ = xTaskCreateStatic([](void* arg) {
        auto this_ = (WakeWordDetect*)arg;
        {
            auto start_time = esp_timer_get_time(); // 获取开始时间
            auto encoder = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS); // 创建Opus编码器
            encoder->SetComplexity(0); // 设置编码复杂度为0（最快）

            // 对PCM数据进行编码
            for (auto& pcm: this_->wake_word_pcm_) {
                encoder->Encode(std::move(pcm), [this_](std::vector<uint8_t>&& opus) {
                    std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);
                    this_->wake_word_opus_.emplace_back(std::move(opus)); // 存储编码后的数据
                    this_->wake_word_cv_.notify_all(); // 通知等待的线程
                });
            }
            this_->wake_word_pcm_.clear(); // 清空PCM数据

            auto end_time = esp_timer_get_time(); // 获取结束时间
            ESP_LOGI(TAG, "Encode wake word opus %zu packets in %lld ms",
                this_->wake_word_opus_.size(), (end_time - start_time) / 1000); // 日志：编码完成

            std::lock_guard<std::mutex> lock(this_->wake_word_mutex_);
            this_->wake_word_opus_.push_back(std::vector<uint8_t>()); // 添加空数据表示编码结束
            this_->wake_word_cv_.notify_all(); // 通知等待的线程
        }
        vTaskDelete(NULL); // 任务完成后删除任务
    }, "encode_detect_packets", 4096 * 8, this, 2, wake_word_encode_task_stack_, &wake_word_encode_task_buffer_);
}

// 获取编码后的唤醒词数据
bool WakeWordDetect::GetWakeWordOpus(std::vector<uint8_t>& opus) {
    std::unique_lock<std::mutex> lock(wake_word_mutex_);
    wake_word_cv_.wait(lock, [this]() {
        return !wake_word_opus_.empty(); // 等待编码数据可用
    });
    opus.swap(wake_word_opus_.front()); // 获取编码数据
    wake_word_opus_.pop_front(); // 删除已获取的数据
    return !opus.empty(); // 返回是否获取到有效数据
}