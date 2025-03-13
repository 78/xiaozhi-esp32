#include "audio_processor.h"
#include <esp_log.h>

#define PROCESSOR_RUNNING 0x01 // 定义事件标志位，表示处理器正在运行

static const char* TAG = "AudioProcessor"; // 日志标签

// 构造函数，初始化事件组
AudioProcessor::AudioProcessor()
    : afe_communication_data_(nullptr) { // 初始化AFE通信数据为空
    event_group_ = xEventGroupCreate(); // 创建事件组
}

// 初始化函数，配置AFE（音频前端）并启动音频处理任务
void AudioProcessor::Initialize(int channels, bool reference) {
    channels_ = channels; // 设置音频通道数
    reference_ = reference; // 设置是否使用参考信号
    int ref_num = reference_ ? 1 : 0; // 根据是否使用参考信号确定参考信号数量

    // AFE配置结构体
    afe_config_t afe_config = {
        .aec_init = false, // 不启用回声消除
        .se_init = true, // 启用语音增强
        .vad_init = false, // 不启用语音活动检测
        .wakenet_init = false, // 不启用唤醒词检测
        .voice_communication_init = true, // 启用语音通信模式
        .voice_communication_agc_init = true, // 启用自动增益控制
        .voice_communication_agc_gain = 10, // 自动增益控制增益值
        .vad_mode = VAD_MODE_3, // 语音活动检测模式
        .wakenet_model_name = NULL, // 唤醒词模型名称
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
            .sample_rate = 16000, // 采样率
        },
        .debug_init = false, // 不启用调试模式
        .debug_hook = {{ AFE_DEBUG_HOOK_MASE_TASK_IN, NULL }, { AFE_DEBUG_HOOK_FETCH_TASK_IN, NULL }}, // 调试钩子
        .afe_ns_mode = NS_MODE_SSP, // 噪声抑制模式
        .afe_ns_model_name = NULL, // 噪声抑制模型名称
        .fixed_first_channel = true, // 固定第一个通道
    };

    // 根据配置创建AFE通信数据
    afe_communication_data_ = esp_afe_vc_v1.create_from_config(&afe_config);
    
    // 创建音频处理任务
    xTaskCreate([](void* arg) {
        auto this_ = (AudioProcessor*)arg;
        this_->AudioProcessorTask(); // 运行音频处理任务
        vTaskDelete(NULL); // 任务完成后删除任务
    }, "audio_communication", 4096 * 2, this, 2, NULL);
}

// 析构函数，释放资源
AudioProcessor::~AudioProcessor() {
    if (afe_communication_data_ != nullptr) {
        esp_afe_vc_v1.destroy(afe_communication_data_); // 销毁AFE通信数据
    }
    vEventGroupDelete(event_group_); // 删除事件组
}

// 输入音频数据
void AudioProcessor::Input(const std::vector<int16_t>& data) {
    input_buffer_.insert(input_buffer_.end(), data.begin(), data.end()); // 将数据插入输入缓冲区

    auto feed_size = esp_afe_vc_v1.get_feed_chunksize(afe_communication_data_) * channels_; // 获取每次喂入AFE的数据大小
    while (input_buffer_.size() >= feed_size) {
        auto chunk = input_buffer_.data(); // 获取输入缓冲区的数据
        esp_afe_vc_v1.feed(afe_communication_data_, chunk); // 将数据喂入AFE
        input_buffer_.erase(input_buffer_.begin(), input_buffer_.begin() + feed_size); // 删除已处理的数据
    }
}

// 启动音频处理器
void AudioProcessor::Start() {
    xEventGroupSetBits(event_group_, PROCESSOR_RUNNING); // 设置事件标志位，表示处理器正在运行
}

// 停止音频处理器
void AudioProcessor::Stop() {
    xEventGroupClearBits(event_group_, PROCESSOR_RUNNING); // 清除事件标志位，表示处理器停止运行
}

// 检查音频处理器是否正在运行
bool AudioProcessor::IsRunning() {
    return xEventGroupGetBits(event_group_) & PROCESSOR_RUNNING; // 获取事件标志位并检查是否正在运行
}

// 设置输出回调函数
void AudioProcessor::OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) {
    output_callback_ = callback; // 设置输出回调函数
}

// 音频处理任务
void AudioProcessor::AudioProcessorTask() {
    auto fetch_size = esp_afe_sr_v1.get_fetch_chunksize(afe_communication_data_); // 获取每次从AFE获取的数据大小
    auto feed_size = esp_afe_sr_v1.get_feed_chunksize(afe_communication_data_); // 获取每次喂入AFE的数据大小
    ESP_LOGI(TAG, "Audio communication task started, feed size: %d fetch size: %d",
        feed_size, fetch_size); // 日志：音频通信任务启动

    while (true) {
        xEventGroupWaitBits(event_group_, PROCESSOR_RUNNING, pdFALSE, pdTRUE, portMAX_DELAY); // 等待处理器运行标志位

        auto res = esp_afe_vc_v1.fetch(afe_communication_data_); // 从AFE获取处理后的数据
        if ((xEventGroupGetBits(event_group_) & PROCESSOR_RUNNING) == 0) {
            continue; // 如果处理器未运行，则继续等待
        }
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            if (res != nullptr) {
                ESP_LOGI(TAG, "Error code: %d", res->ret_value); // 日志：错误代码
            }
            continue; // 如果获取数据失败，则继续
        }

        if (output_callback_) {
            output_callback_(std::vector<int16_t>(res->data, res->data + res->data_size / sizeof(int16_t))); // 调用输出回调函数
        }
    }
}