// beat_sync.cc — 随音乐摇摆实现
//
// BPM 检测算法（轻量自相关，无需 FFT）:
//   1. 从板载 I2S 麦克风采集 ~5 秒 16kHz mono 16-bit PCM
//   2. 50ms 窗口 RMS → 100 点能量包络
//   3. 一阶差分提取 onset（起音）强度
//   4. 自相关搜索 BPM 60~180 范围内的最佳周期
//   5. half_ms = 30000 / bpm（使 2×half_ms/cycle 的摇摆对齐 1 拍）
//
// 硬件: AUDIO_I2S_MIC_* (GPIO 16/17/18), 16kHz I2S STD mono

#include "beat_sync.h"
#include "dog_control.h"
#include "board.h"
#include "audio_codec.h"
#include "application.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

static const char* TAG = "BeatSync";

/*============================================================================
  音频采集参数（与 config.h I2S 麦克风一致）
============================================================================*/
#define BEAT_SAMPLE_RATE      16000
#define BEAT_CAPTURE_SEC      5
#define BEAT_TOTAL_SAMPLES    (BEAT_SAMPLE_RATE * BEAT_CAPTURE_SEC)      // 80000
#define BEAT_WINDOW_MS        50
#define BEAT_WINDOW_SAMPLES   (BEAT_SAMPLE_RATE * BEAT_WINDOW_MS / 1000) // 800
#define BEAT_ENVELOPE_SIZE    (BEAT_CAPTURE_SEC * 1000 / BEAT_WINDOW_MS) // 100

/* BPM 检测范围 */
#define BPM_MIN  60
#define BPM_MAX  180

/*============================================================================
  采集音频（从现有 AudioCodec 的 I2S 麦克风读取）
  返回 malloc 的 int16_t 数组（调用者 free），失败返回 NULL
============================================================================*/
static int16_t* capture_audio(int* out_samples) {
    *out_samples = 0;

    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec();
    if (!codec) {
        ESP_LOGE(TAG, "AudioCodec 不可用");
        return NULL;
    }

    /*----------------------------------------------------------
     * 暂停唤醒词检测，独占 I2S 麦克风通道
     *
     * audio_input 任务清除事件位后阻塞在 xEventGroupWaitBits，
     * 不会再消费 I2S 数据。我们独占比它优先级低的通道。
     *
     * 不通过 ReadAudioData（它内部有复杂的状态管理），
     * 直接用 codec->InputData() + 积极的通道重启用。
     *----------------------------------------------------------*/
    auto& audio_service = Application::GetInstance().GetAudioService();
    bool wake_word_was_running = audio_service.IsWakeWordRunning();
    bool audio_proc_was_running = audio_service.IsAudioProcessorRunning();

    if (wake_word_was_running) {
        ESP_LOGI(TAG, "暂停唤醒词检测…");
        audio_service.EnableWakeWordDetection(false);
    }
    if (audio_proc_was_running) {
        ESP_LOGI(TAG, "暂停语音处理…");
        audio_service.EnableVoiceProcessing(false);
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // 确保 I2S 通道已启用
    if (!codec->input_enabled()) {
        codec->EnableInput(true);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    int16_t* buf = NULL;
    int total = 0;
    int timeout = 0;

    buf = (int16_t*)malloc(BEAT_TOTAL_SAMPLES * sizeof(int16_t));
    if (!buf) {
        ESP_LOGE(TAG, "无法分配音频缓冲区 (%d 字节)", BEAT_TOTAL_SAMPLES * 2);
        goto cleanup;
    }

    ESP_LOGI(TAG, "开始采集 %d 秒音频…", BEAT_CAPTURE_SEC);

    while (total < BEAT_TOTAL_SAMPLES) {
        // 使用 160 样本 chunk（=10ms），与 audio_input 任务一致，
        // STD I2S DMA 描述符=6×240，160 落在 DMA 帧大小之内
        int chunk = 160;
        if (total + chunk > BEAT_TOTAL_SAMPLES) {
            chunk = BEAT_TOTAL_SAMPLES - total;
        }

        std::vector<int16_t> chunk_vec(chunk);
        if (codec->InputData(chunk_vec)) {
            memcpy(buf + total, chunk_vec.data(), chunk * sizeof(int16_t));
            total += chunk;
            timeout = 0;
        } else {
            // 通道可能被电源定时器关闭——立即重新启用并重试
            if (!codec->input_enabled()) {
                codec->EnableInput(true);
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;  // 不增加 timeout，立即重试
            }
            timeout++;
            if (timeout > 200) {
                ESP_LOGE(TAG, "麦克风读取超时 (%d 连续失败)", timeout);
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }

    if (total < BEAT_SAMPLE_RATE) {
        ESP_LOGE(TAG, "采集数据不足 (%d < %d)", total, BEAT_SAMPLE_RATE);
        free(buf);
        buf = NULL;
    } else {
        *out_samples = total;
        ESP_LOGI(TAG, "采集完成: %d 样本 (%.1f 秒)", total, (float)total / BEAT_SAMPLE_RATE);
    }

cleanup:
    if (wake_word_was_running) {
        ESP_LOGI(TAG, "恢复唤醒词检测");
        audio_service.EnableWakeWordDetection(true);
    }
    if (audio_proc_was_running) {
        ESP_LOGI(TAG, "恢复语音处理");
        audio_service.EnableVoiceProcessing(true);
    }

    return buf;
}

/*============================================================================
  BPM 检测：自相关法
  输入: pcm (int16_t), sample_count
  返回: BPM (60~180)，失败返回 -1
============================================================================*/
static int detect_bpm_from_pcm(const int16_t* pcm, int sample_count) {
    /* ---- 1. 计算 RMS 能量包络 ---- */
    int window_count = sample_count / BEAT_WINDOW_SAMPLES;
    if (window_count > BEAT_ENVELOPE_SIZE) {
        window_count = BEAT_ENVELOPE_SIZE;
    }
    if (window_count < 30) {
        ESP_LOGE(TAG, "数据太短，窗口数=%d（需≥30）", window_count);
        return -1;
    }

    float energy[BEAT_ENVELOPE_SIZE];
    for (int w = 0; w < window_count; w++) {
        int64_t sum_sq = 0;
        const int16_t* win = pcm + w * BEAT_WINDOW_SAMPLES;
        int win_samples = BEAT_WINDOW_SAMPLES;
        for (int i = 0; i < win_samples; i++) {
            int32_t s = win[i];
            sum_sq += (int64_t)s * s;
        }
        energy[w] = sqrtf((float)sum_sq / win_samples);
    }

    /* ---- 2. 计算 onset 强度（一阶差分，截断负值）---- */
    float onset[BEAT_ENVELOPE_SIZE - 1];
    for (int i = 1; i < window_count; i++) {
        float diff = energy[i] - energy[i - 1];
        onset[i - 1] = (diff > 0.0f) ? diff : 0.0f;
    }
    int onset_count = window_count - 1;
    if (onset_count < 20) return -1;

    /* ---- 3. 去除直流分量 ---- */
    float onset_mean = 0.0f;
    for (int i = 0; i < onset_count; i++) onset_mean += onset[i];
    onset_mean /= onset_count;
    for (int i = 0; i < onset_count; i++) onset[i] -= onset_mean;

    /* ---- 4. 自相关搜索 BPM 60~180 ---- */
    // 50ms 窗口 → 周期 333ms (BPM180) 对应 lag ≈ 6.7
    //                周期 1000ms (BPM60) 对应 lag = 20
    const int min_lag = 6;   // BPM=180 上限
    const int max_lag = 20;  // BPM=60  下限
    // 自适应边界：不超过 onset_count 的一半
    int search_max = max_lag;
    if (search_max > onset_count / 2) search_max = onset_count / 2;
    if (search_max <= min_lag) search_max = min_lag + 10;

    float best_corr = -1.0f;
    int   best_lag  = 0;

    for (int lag = min_lag; lag <= search_max; lag++) {
        float sum_xy = 0.0f, sum_xx = 0.0f, sum_yy = 0.0f;
        int N = onset_count - lag;
        for (int i = 0; i < N; i++) {
            float x = onset[i];
            float y = onset[i + lag];
            sum_xy += x * y;
            sum_xx += x * x;
            sum_yy += y * y;
        }
        float corr = (sum_xx > 0.0f && sum_yy > 0.0f)
                     ? sum_xy / sqrtf(sum_xx * sum_yy)
                     : 0.0f;

        if (corr > best_corr) {
            best_corr = corr;
            best_lag  = lag;
        }
    }

    if (best_lag <= 0 || best_corr < 0.15f) {
        ESP_LOGW(TAG, "未检测到明显节拍 (best_corr=%.3f, best_lag=%d)", best_corr, best_lag);
        return -1;
    }

    int period_ms = best_lag * BEAT_WINDOW_MS;
    int bpm = 60000 / period_ms;

    // 约束到合理范围
    if (bpm < BPM_MIN) bpm = BPM_MIN;
    if (bpm > BPM_MAX) bpm = BPM_MAX;

    ESP_LOGI(TAG, "BPM 检测完成: %d BPM (周期=%dms, lag=%d, corr=%.3f)",
             bpm, period_ms, best_lag, best_corr);
    return bpm;
}

/*============================================================================
  执行四个随机基础摇摆动作（对齐 BPM）
  基础动作池: FB, LR, Twist, UpDown（各 2×half_ms/cycle）
  每个动作 2 cycles（2 拍），动作间 1 拍间隔
  half_ms = 30000/bpm 使 1 cycle = 1 beat
============================================================================*/
static void run_four_random_sways(int bpm) {
    // half_ms = 60000/bpm: 1 cycle = 2 beats, 每个动作 2 cycles = 4 beats
    // 120 BPM → half_ms=500（对齐标准 Dog_Dance 速度）
    // 180 BPM → half_ms=333
    int half_ms = 60000 / bpm;
    if (half_ms < 100) half_ms = 100;
    if (half_ms > 600) half_ms = 600;

    ESP_LOGI(TAG, "开始四动作摇摆: BPM=%d, half_ms=%d", bpm, half_ms);

    // Fisher-Yates 随机顺序
    int order[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }

    const char* names[] = {"前后摇摆", "左右摇摆", "旋转摇摆", "上下摇摆"};

    for (int i = 0; i < 4; i++) {
        int idx = order[i];
        ESP_LOGI(TAG, "[%d/4] %s (half_ms=%d, cycles=2)", i + 1, names[idx], half_ms);

        // 每个动作执行两遍：2 cycles × 2 = 4 cycles = 8 beats
        switch (idx) {
            case 0: Dog_SwingFB(2, half_ms);     Dog_SwingFB(2, half_ms);      break;
            case 1: Dog_SwingLR(1, 2, half_ms);  Dog_SwingLR(1, 2, half_ms);   break;
            case 2: Dog_SwingTwist(2, half_ms);   Dog_SwingTwist(2, half_ms);    break;
            case 3: Dog_SwingUpDown(2, half_ms);  Dog_SwingUpDown(2, half_ms);   break;
        }
        // 动作连续执行，不停顿——每个 action 内部已包含 4-beat 节奏
    }

    ESP_LOGI(TAG, "四动作摇摆完成");
}

/*============================================================================
  公开接口
============================================================================*/

int beat_sync_detect_bpm(void) {
    int sample_count = 0;
    int16_t* pcm = capture_audio(&sample_count);
    if (!pcm) return -1;

    int bpm = detect_bpm_from_pcm(pcm, sample_count);
    free(pcm);
    return bpm;
}

int beat_sync_run(void) {
    int sample_count = 0;
    int16_t* pcm = capture_audio(&sample_count);
    if (!pcm) return -1;

    int bpm = detect_bpm_from_pcm(pcm, sample_count);
    free(pcm);

    if (bpm <= 0) {
        ESP_LOGE(TAG, "BPM 检测失败，无法执行摇摆");
        return -1;
    }

    run_four_random_sways(bpm);
    return 0;
}
