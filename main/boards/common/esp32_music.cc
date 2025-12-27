#include "esp32_music.h"
#include "board.h"
#include "system_info.h"
#include "audio/audio_codec.h"
#include "application.h"
#include "protocols/protocol.h"
#include "display/display.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_pthread.h>
#include <esp_timer.h>
#include <mbedtls/sha256.h>
#include <cJSON.h>
#include <cstring>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <cctype>  // 为isdigit函数
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "Esp32Music"

// Esp32Music 构造函数
Esp32Music::Esp32Music()
    : last_downloaded_data_(""),
      current_music_url_(""),
      current_song_name_(""),
      song_name_displayed_(false),
      current_lyric_url_(""),
      current_lyric_index_(-1),
      is_lyric_running_(false),
      is_playing_(false),
      is_downloading_(false),
      current_play_time_ms_(0),
      last_frame_time_ms_(0),
      total_frames_decoded_(0),
      buffer_size_(0),
      mp3_decoder_(nullptr),
      mp3_decoder_initialized_(false),
      index_manager_(nullptr),
      index_initialized_(false) {
    ESP_LOGI(TAG, "Esp32Music constructor called");

    // 初始化 MP3 解码器
    mp3_decoder_ = MP3InitDecoder();
    if (mp3_decoder_) {
        mp3_decoder_initialized_ = true;
        ESP_LOGI(TAG, "MP3 decoder initialized successfully");
    } else {
        ESP_LOGE(TAG, "Failed to initialize MP3 decoder");
    }

    // 异步初始化音乐索引（防止阻塞初始化流程）
    xTaskCreate([](void* arg) {
        Esp32Music* this_ptr = static_cast<Esp32Music*>(arg);
        vTaskDelay(pdMS_TO_TICKS(500));  // 等待系统稳定
        if (!this_ptr->index_initialized_) {
            if (this_ptr->InitializeIndex()) {
                ESP_LOGI(TAG, "Music index initialized in background task");
            } else {
                ESP_LOGW(TAG, "Failed to initialize music index in background");
            }
        }
        vTaskDelete(NULL);
    }, "music_index_init", 4096, this, 1, nullptr);

    // 监听设备状态变化：唤醒/对话时暂停，回到待机自动继续
    DeviceStateEventManager::GetInstance().RegisterStateChangeCallback(
        [this](DeviceState prev, DeviceState curr) {
            if (!is_playing_.load()) {
                return;
            }

            // 用户主动唤醒：idle/listening → connecting，进入对话流程，暂停音乐
            if ((prev == kDeviceStateIdle || prev == kDeviceStateListening) &&
                curr == kDeviceStateConnecting) {
                ESP_LOGI(TAG, "User wakeup detected, pausing music playback");
                paused_for_dialog_ = true;
                user_wakeup_pause_ = true;
                pause_log_emitted_ = false;
                return;
            }

            // 若是用户唤醒引发的对话：connecting/listening/speaking 全程保持暂停
            if (user_wakeup_pause_.load()) {
                // 对话结束判定：回到 idle（会话关闭）才恢复音乐
                if (curr == kDeviceStateIdle) {
                    ESP_LOGI(TAG, "Dialog finished (Idle), resuming music playback");
                    paused_for_dialog_ = false;
                    user_wakeup_pause_ = false;
                    pause_log_emitted_ = false;
                    return;
                }

                // 对话进行中：保持暂停（特别是 listening 阶段需要暂停，否则会边听边放）
                if (curr == kDeviceStateConnecting ||
                    curr == kDeviceStateListening ||
                    curr == kDeviceStateSpeaking) {
                    paused_for_dialog_ = true;
                    pause_log_emitted_ = false;
                    return;
                }
            }

            // 非用户唤醒对话：idle/listening 允许播放
            if (curr == kDeviceStateIdle || curr == kDeviceStateListening) {
                paused_for_dialog_ = false;
                pause_log_emitted_ = false;
            }
        });
}

// Esp32Music 析构函数
Esp32Music::~Esp32Music() {
    ESP_LOGI(TAG, "Esp32Music destructor called");

    // 停止所有播放和下载线程
    is_playing_ = false;
    is_downloading_ = false;
    is_lyric_running_ = false;

    // 等待线程退出
    if (play_thread_.joinable()) {
        try {
            play_thread_.join();
        } catch (const std::exception& e) {
            ESP_LOGW(TAG, "Exception while joining play_thread: %s", e.what());
        }
    }

    if (download_thread_.joinable()) {
        try {
            download_thread_.join();
        } catch (const std::exception& e) {
            ESP_LOGW(TAG, "Exception while joining download_thread: %s", e.what());
        }
    }

    if (lyric_thread_.joinable()) {
        try {
            lyric_thread_.join();
        } catch (const std::exception& e) {
            ESP_LOGW(TAG, "Exception while joining lyric_thread: %s", e.what());
        }
    }

    // 关闭 SD 卡文件
    CloseSdCardFile();

    // 清理音频缓冲区
    ClearAudioBuffer();

    // 清理 MP3 解码器
    if (mp3_decoder_initialized_ && mp3_decoder_) {
        MP3FreeDecoder(mp3_decoder_);
        mp3_decoder_ = nullptr;
        mp3_decoder_initialized_ = false;
    }

    // 清理索引管理器
    if (index_manager_) {
        index_manager_.reset();
    }

    ESP_LOGI(TAG, "Esp32Music destructor completed");
}

// 打开 SD 卡文件
bool Esp32Music::OpenSdCardFile(const std::string& file_path) {
    ESP_LOGI(TAG, "Opening SD card file: %s", file_path.c_str());

    // 先验证文件完整性
    if (!ValidateMP3File(file_path)) {
        ESP_LOGE(TAG, "文件验证失败，无法播放: %s", file_path.c_str());
        return false;
    }

    // 先关闭已打开的文件
    CloseSdCardFile();

    sd_file_ = fopen(file_path.c_str(), "rb");
    if (sd_file_ == nullptr) {
        ESP_LOGE(TAG, "Failed to open SD card file: %s", file_path.c_str());
        return false;
    }
    ESP_LOGI(TAG, "Opened SD card file: %s", file_path.c_str());
    return true;
}

// 验证MP3文件完整性
bool Esp32Music::ValidateMP3File(const std::string& file_path)
{
    FILE* test_file = fopen(file_path.c_str(), "rb");
    if (!test_file) {
        ESP_LOGE(TAG, "文件验证失败：无法打开文件 %s", file_path.c_str());
        DisplayFileError(file_path, "文件无法打开");
        return false;
    }

    // 获取文件大小
    fseek(test_file, 0, SEEK_END);
    long file_size = ftell(test_file);
    fseek(test_file, 0, SEEK_SET);

    ESP_LOGI(TAG, "验证文件: %s, 大小: %ld 字节", file_path.c_str(), file_size);

    // 检查文件大小
    if (file_size <= 0) {
        ESP_LOGE(TAG, "文件验证失败：文件为空或大小检测失败");
        fclose(test_file);
        DisplayFileError(file_path, "文件为空");
        return false;
    }

    if (file_size < 1024) {
        ESP_LOGW(TAG, "文件验证警告：文件大小过小 (%ld 字节)，可能不是有效的MP3文件", file_size);
    }

    // 读取文件头部进行验证
    uint8_t header[32];
    size_t read_bytes = fread(header, 1, 32, test_file);
    fclose(test_file);

    if (read_bytes < 16) {
        ESP_LOGE(TAG, "文件验证失败：无法读取文件头部");
        DisplayFileError(file_path, "文件读取失败");
        return false;
    }

    ESP_LOGI(TAG, "文件头部: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
             header[0], header[1], header[2], header[3], header[4], header[5], header[6], header[7],
             header[8], header[9], header[10], header[11], header[12], header[13], header[14], header[15]);

    // 检查是否全是零字节
    bool all_zeros = true;
    for (int i = 0; i < 16; i++) {
        if (header[i] != 0) {
            all_zeros = false;
            break;
        }
    }

    if (all_zeros) {
        ESP_LOGE(TAG, "文件验证失败：文件已损坏或为空（检测到全零字节）");
        DisplayFileError(file_path, "文件已损坏");
        return false;
    }

    // 检查是否是有效的MP3文件格式
    bool is_valid_mp3 = false;
    std::string format_info = "";

    // 检查ID3v2标签
    if (memcmp(header, "ID3", 3) == 0) {
        uint8_t major_version = header[3];
        uint8_t minor_version = header[4];
        format_info = "ID3v2." + std::to_string(major_version) + "." + std::to_string(minor_version) + " 标签";
        ESP_LOGI(TAG, "检测到 %s", format_info.c_str());
        is_valid_mp3 = true;
    }
    // 检查MP3同步字 (0xFF 0xFB, 0xFF 0xFA, 0xFF 0xF3, 0xFF 0xF2)
    else if ((header[0] == 0xFF) && ((header[1] & 0xE0) == 0xE0)) {
        format_info = "MP3同步字";
        ESP_LOGI(TAG, "检测到 %s", format_info.c_str());
        is_valid_mp3 = true;
    }
    // 检查其他可能的音频格式
    else if (memcmp(header, "RIFF", 4) == 0) {
        format_info = "RIFF/WAV格式";
        ESP_LOGW(TAG, "检测到 %s，但期望MP3格式", format_info.c_str());
    }
    else if (memcmp(header, "fLaC", 4) == 0) {
        format_info = "FLAC格式";
        ESP_LOGW(TAG, "检测到 %s，但期望MP3格式", format_info.c_str());
    }
    else {
        format_info = "未知格式";
        ESP_LOGW(TAG, "检测到 %s", format_info.c_str());
    }

    if (!is_valid_mp3) {
        ESP_LOGE(TAG, "文件验证失败：不是有效的MP3文件格式 (检测到: %s)", format_info.c_str());
        DisplayFileError(file_path, "格式不支持: " + format_info);
        return false;
    }

    ESP_LOGI(TAG, "文件验证成功：%s 是有效的MP3文件", file_path.c_str());
    return true;
}

// 显示文件错误信息
void Esp32Music::DisplayFileError(const std::string& file_path, const std::string& error_msg)
{
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    if (display) {
        // 提取文件名
        size_t pos = file_path.find_last_of("/\\");
        std::string filename = (pos != std::string::npos) ? file_path.substr(pos + 1) : file_path;
        
        // 格式化错误信息
        std::string error_display = filename + " - " + error_msg;
        display->SetMusicInfo(error_display.c_str());
        ESP_LOGI(TAG, "显示文件错误: %s", error_display.c_str());
    }
}


// 播放 SD 卡中的音频流
void Esp32Music::PlaySdCardAudioStream() {
    ESP_LOGI(TAG, "Starting SD card audio stream playback");

    struct PlayTaskHandleScope {
        Esp32Music* self;
        explicit PlayTaskHandleScope(Esp32Music* instance) : self(instance) {
            self->play_thread_task_handle_.store((void*)xTaskGetCurrentTaskHandle(), std::memory_order_relaxed);
        }
        ~PlayTaskHandleScope() {
            self->play_thread_task_handle_.store(nullptr, std::memory_order_relaxed);
        }
    } play_task_handle_scope(this);

    // 初始化时间跟踪变量
    current_play_time_ms_ = 0;
    last_frame_time_ms_ = 0;
    total_frames_decoded_ = 0;

    auto codec = Board::GetInstance().GetAudioCodec();
    if (!codec || !codec->output_enabled()) {
        ESP_LOGE(TAG, "Audio codec not available or not enabled");
        is_playing_ = false;
        CloseSdCardFile();
        // 新增：播放无法启动时，清除播放状态
        Application::GetInstance().SetMusicPlaying(false);
        return;
    }

    if (!mp3_decoder_initialized_) {
        ESP_LOGE(TAG, "MP3 decoder not initialized");
        is_playing_ = false;
        CloseSdCardFile();
        // 新增：播放无法启动时，清除播放状态
        Application::GetInstance().SetMusicPlaying(false);
        return;
    }

    // 启用音频输出
    codec->EnableOutput(true);
    ESP_LOGI(TAG, "Audio output enabled for SD card music playback");

    size_t total_played = 0;
    uint8_t *mp3_input_buffer = nullptr;
    int bytes_left = 0;
    uint8_t *read_ptr = nullptr;

    // 分配 MP3 输入缓冲区
    mp3_input_buffer = (uint8_t *)heap_caps_malloc(16384, MALLOC_CAP_SPIRAM);
    if (!mp3_input_buffer) {
        ESP_LOGE(TAG, "Failed to allocate MP3 input buffer");
        is_playing_ = false;
        CloseSdCardFile();
        // 新增：分配失败时，清除播放状态
        Application::GetInstance().SetMusicPlaying(false);
        return;
    }

    // 标记是否已经处理过 ID3 标签
    bool id3_processed = false;
    int consecutive_decode_failures = 0;

    auto switch_to_track = [&](const std::string& track_path) -> bool {
        ESP_LOGI(TAG, "Switching to track: %s", track_path.c_str());

        if (!OpenSdCardFile(track_path)) {
            ESP_LOGW(TAG, "Failed to open track: %s", track_path.c_str());
            return false;
        }

        // 更新当前曲目信息
        current_song_name_ = track_path.substr(track_path.find_last_of("/\\") + 1);
        song_name_displayed_ = false;
        playback_started_ = false;
        stop_listening_requested_ = false;

        // 重置解码/计时状态
        current_play_time_ms_ = 0;
        last_frame_time_ms_ = 0;
        total_frames_decoded_ = 0;
        id3_processed = false;
        consecutive_decode_failures = 0;
        bytes_left = 0;
        read_ptr = mp3_input_buffer;

        // 重置 MP3 解码器状态，避免跨文件残留
        if (mp3_decoder_) {
            MP3FreeDecoder(mp3_decoder_);
            mp3_decoder_ = nullptr;
        }
        mp3_decoder_ = MP3InitDecoder();
        mp3_decoder_initialized_ = (mp3_decoder_ != nullptr);
        if (!mp3_decoder_initialized_) {
            ESP_LOGE(TAG, "Failed to reinitialize MP3 decoder for track switch");
            is_playing_ = false;
            return false;
        }

        return true;
    };

    while (is_playing_) {
        // 用户请求切歌（上一曲/下一曲）：仅在列表播放下生效
        int requested_index = playlist_switch_to_index_.exchange(-1, std::memory_order_relaxed);
        if (requested_index >= 0 && is_playing_.load()) {
            std::string requested_track;
            {
                std::lock_guard<std::mutex> lock(playlist_mutex_);
                if (playlist_active_ &&
                    requested_index < static_cast<int>(playlist_paths_.size()) &&
                    requested_index >= 0) {
                    playlist_index_ = static_cast<size_t>(requested_index);
                    requested_track = playlist_paths_[playlist_index_];
                }
            }

            if (!requested_track.empty()) {
                ESP_LOGI(TAG, "Track switch requested: index=%d, path=%s",
                         requested_index, requested_track.c_str());
                if (switch_to_track(requested_track)) {
                    continue;
                }
                ESP_LOGW(TAG, "Track switch failed, continue current track");
            }
        }

        // 统一检查：对话暂停标记 || 非空闲/非聆听状态
        auto &app = Application::GetInstance();
        DeviceState current_state = app.GetDeviceState();

        // 暂停条件：
        // 1. 有对话暂停标记（用户唤醒导致）
        // 2. 非 idle 且非 listening（connecting/speaking/activating 等状态）
        //
        // 允许播放的状态：idle 或 listening（AI说完话在等待）
        bool should_pause = paused_for_dialog_.load() ||
                           (current_state != kDeviceStateIdle && current_state != kDeviceStateListening);

        if (should_pause) {
            if (!pause_log_emitted_.exchange(true)) {
                ESP_LOGW(TAG, "Playback paused (dialog=%d, state=%d)",
                         paused_for_dialog_.load(), current_state);
            }
            // 进入对话/非可播放状态时，允许在下次恢复播放时再次请求回到待机。
            stop_listening_requested_ = false;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // 恢复播放后清理暂停日志标记
        pause_log_emitted_ = false;

        // 本地音乐播放时希望处于 idle：
        // 1) 避免 listening 阶段边听边放（mic/voice processing 仍在工作）
        // 2) 关闭音频通道，避免服务端后续继续下发 TTS（例如会话结束语）
        if (current_state == kDeviceStateListening) {
            if (!stop_listening_requested_.exchange(true)) {
                app.StopListening();
                app.CloseAudioChannelIfOpened();
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // 若已进入 idle（manual-stop 等路径可能直接到 idle），也确保音频通道关闭一次。
        if (!stop_listening_requested_.exchange(true)) {
            app.CloseAudioChannelIfOpened();
        }

        // 设备状态检查通过，显示当前播放的歌名
        if (!song_name_displayed_ && !current_song_name_.empty()) {
            auto &board = Board::GetInstance();
            auto display = board.GetDisplay();
            if (display) {
                // 格式化歌名显示为《歌名》播放中...
                std::string formatted_song_name = "《" + current_song_name_ + "》播放中...";
                display->SetMusicInfo(formatted_song_name.c_str());

                ESP_LOGI(TAG, "Displaying song name: %s", formatted_song_name.c_str());
                song_name_displayed_ = true;
            }
        }

        // 如果需要更多 MP3 数据，从 SD 卡文件读取
        if (bytes_left < 6144) { // 保持至少 6KB 数据用于解码

            if (bytes_left > 0) {
                memmove(mp3_input_buffer, read_ptr, bytes_left);
            }

            size_t read_size = fread(mp3_input_buffer + bytes_left, 1, 8192 - bytes_left, sd_file_);
            if (read_size == 0) {
                if (feof(sd_file_)) {
                    // 文件读取结束：若启用了播放列表，自动切换到下一首
                    std::string next_track;
                    size_t max_attempts = 0;
                    {
                        std::lock_guard<std::mutex> lock(playlist_mutex_);
                        max_attempts = playlist_active_ ? playlist_paths_.size() : 0;
                    }

                    bool switched = false;
                    if (is_playing_.load() && max_attempts > 0) {
                        for (size_t attempt = 0; attempt < max_attempts && is_playing_.load(); ++attempt) {
                            if (!GetNextPlaylistTrack(next_track)) {
                                break;
                            }

                            ESP_LOGI(TAG, "Track finished, switching to next: %s", next_track.c_str());
                            if (switch_to_track(next_track)) {
                                switched = true;
                                break;
                            }
                            ESP_LOGW(TAG, "Failed to switch to next track: %s, skipping", next_track.c_str());
                        }
                    }

                    if (switched) {
                        continue;
                    }

                    // 文件读取结束，播放结束（无下一首或切换失败）
                    ESP_LOGI(TAG, "SD card playback finished, total played: %d bytes", total_played);
                    break;
                } else {
                    ESP_LOGE(TAG, "Failed to read SD card file");
                    break;
                }
            }
            bytes_left += read_size;
            read_ptr = mp3_input_buffer;

            // 检查并跳过 ID3 标签（仅在开始时处理一次）
            if (!id3_processed && bytes_left >= 10) {
                size_t id3_skip = SkipId3Tag(read_ptr, bytes_left);
                if (id3_skip > 0) {
                    if (id3_skip >= static_cast<size_t>(bytes_left)) {
                        size_t remaining_skip = id3_skip - static_cast<size_t>(bytes_left);
                        ESP_LOGI(TAG, "Skipping ID3 tag: %u bytes (seeking remaining %u bytes in file)",
                                 static_cast<unsigned int>(id3_skip),
                                 static_cast<unsigned int>(remaining_skip));

                        // 结束当前缓冲，把剩余的ID3字节从文件中跳过
                        bytes_left = 0;
                        read_ptr = mp3_input_buffer;

                        if (fseek(sd_file_, static_cast<long>(remaining_skip), SEEK_CUR) != 0) {
                            ESP_LOGE(TAG, "Failed to seek past ID3 tag, aborting playback");
                            break;
                        }
                        // 跳过ID3后立刻读取数据，避免空转检查同步字
                        continue;
                    } else {
                        read_ptr += id3_skip;
                        bytes_left -= static_cast<int>(id3_skip);
                        ESP_LOGI(TAG, "Skipped ID3 tag: %u bytes", static_cast<unsigned int>(id3_skip));
                    }
                }
                id3_processed = true;
            }
        }

        // 尝试找到 MP3 帧同步
        int sync_offset = MP3FindSyncWord(read_ptr, bytes_left);

        if (sync_offset < 0) {
            ESP_LOGW(TAG, "No MP3 sync word found, skipping %d bytes", bytes_left);
            bytes_left = 0;
            continue;
        }

        // 跳过到同步位置
        if (sync_offset > 0) {
            read_ptr += sync_offset;
            bytes_left -= sync_offset;
        }

        // 解码 MP3 帧
        int16_t pcm_buffer[2304];
        int decode_result = MP3Decode(mp3_decoder_, &read_ptr, &bytes_left, pcm_buffer, 0);

        if (decode_result == 0) {
            playback_started_ = true; // 首帧解码成功，允许对话暂停逻辑生效
            consecutive_decode_failures = 0;
            // 解码成功，获取帧信息
            MP3GetLastFrameInfo(mp3_decoder_, &mp3_frame_info_);
            total_frames_decoded_++;

            // 基本的帧信息有效性检查，防止除零错误
            if (mp3_frame_info_.samprate == 0 || mp3_frame_info_.nChans == 0) {
                ESP_LOGW(TAG, "Invalid frame info: rate=%d, channels=%d, skipping",
                         mp3_frame_info_.samprate, mp3_frame_info_.nChans);
                continue;
            }

            // 计算当前帧的持续时间(毫秒)
            int frame_duration_ms = (mp3_frame_info_.outputSamps * 1000) /
                                    (mp3_frame_info_.samprate * mp3_frame_info_.nChans);

            // 更新当前播放时间
            current_play_time_ms_ += frame_duration_ms;

            ESP_LOGD(TAG, "Frame %d: time=%lldms, duration=%dms, rate=%d, ch=%d",
                     total_frames_decoded_, current_play_time_ms_, frame_duration_ms,
                     mp3_frame_info_.samprate, mp3_frame_info_.nChans);

            // 更新歌词显示
            int buffer_latency_ms = 600; // 实测调整值
            UpdateLyricDisplay(current_play_time_ms_ + buffer_latency_ms);

            // 将 PCM 数据发送到 Application 的音频解码队列
            if (mp3_frame_info_.outputSamps > 0) {
                int16_t *final_pcm_data = pcm_buffer;
                int final_sample_count = mp3_frame_info_.outputSamps;
                std::vector<int16_t> mono_buffer;

                // 如果是双通道，转换为单通道混合
                if (mp3_frame_info_.nChans == 2) {
                    // 双通道转单通道：将左右声道混合
                    int stereo_samples = mp3_frame_info_.outputSamps; // 包含左右声道的总样本数
                    int mono_samples = stereo_samples / 2;            // 实际的单声道样本数

                    mono_buffer.resize(mono_samples);

                    for (int i = 0; i < mono_samples; ++i) {
                        // 混合左右声道 (L + R) / 2
                        int left = pcm_buffer[i * 2];      // 左声道
                        int right = pcm_buffer[i * 2 + 1]; // 右声道
                        mono_buffer[i] = (int16_t)((left + right) / 2);
                    }

                    final_pcm_data = mono_buffer.data();
                    final_sample_count = mono_samples;

                    ESP_LOGD(TAG, "Converted stereo to mono: %d -> %d samples",
                             stereo_samples, mono_samples);
                } else if (mp3_frame_info_.nChans == 1) {
                    // 已经是单声道，无需转换
                    ESP_LOGD(TAG, "Already mono audio: %d samples", final_sample_count);
                } else {
                    ESP_LOGW(TAG, "Unsupported channel count: %d, treating as mono",
                             mp3_frame_info_.nChans);
                }

                // 创建 AudioStreamPacket
                AudioStreamPacket packet;
                packet.sample_rate = mp3_frame_info_.samprate;
                packet.frame_duration = 60; // 使用 Application 默认的帧时长
                packet.timestamp = 0;

                // 将 int16_t PCM 数据转换为 uint8_t 字节数组
                size_t pcm_size_bytes = final_sample_count * sizeof(int16_t);
                packet.payload.resize(pcm_size_bytes);
                memcpy(packet.payload.data(), final_pcm_data, pcm_size_bytes);

                ESP_LOGD(TAG, "Sending %d PCM samples (%d bytes, rate=%d, channels=%d->1) to Application",
                         final_sample_count, pcm_size_bytes, mp3_frame_info_.samprate, mp3_frame_info_.nChans);

                // 发送到 Application 的音频解码队列
                app.AddAudioData(std::move(packet));
                total_played += pcm_size_bytes;

                // 打印播放进度
                if (total_played % (32 * 1024) == 0) {
                    ESP_LOGI(TAG, "Played %d bytes from SD card", total_played);
                }
            }
        } else {
            // 解码失败
            ESP_LOGW(TAG, "MP3 decode failed with error: %d", decode_result);
            
            // 连续解码失败计数
            consecutive_decode_failures++;
            
            // 如果连续失败次数过多，可能是文件损坏，停止播放
            if (consecutive_decode_failures > 10) {
                ESP_LOGE(TAG, "Too many consecutive decode failures (%d), stopping playback", consecutive_decode_failures);
                break;
            }

            // 跳过一些字节继续尝试
            if (bytes_left > 1) {
                read_ptr++;
                bytes_left--;
            } else {
                bytes_left = 0;
            }
        }
    }

    // 清理资源
    if (mp3_input_buffer) {
        heap_caps_free(mp3_input_buffer);
        mp3_input_buffer = nullptr;
        ESP_LOGI(TAG, "MP3 input buffer freed");
    }
    
    // 重置MP3解码器状态
    if (mp3_decoder_initialized_ && mp3_decoder_) {
        // libhelix-mp3没有重置函数，需要重新初始化
        MP3FreeDecoder(mp3_decoder_);
        mp3_decoder_ = MP3InitDecoder();
        if (mp3_decoder_) {
            ESP_LOGI(TAG, "MP3 decoder reinitialized after failure");
        } else {
            ESP_LOGE(TAG, "Failed to reinitialize MP3 decoder");
            mp3_decoder_initialized_ = false;
        }
    }
    
    // 清空音频缓冲区
    ClearAudioBuffer();

    // 播放结束时清空歌名和歌词显示
    auto &board = Board::GetInstance();
    auto display = board.GetDisplay();
    if (display) {
        display->SetMusicInfo(""); // 清空歌名显示
        display->SetChatMessage("lyric", ""); // 清空歌词显示
        ESP_LOGI(TAG, "Cleared song name and lyric display on SD card playback end");
    }

    // 播放结束时保持音频输出启用状态，让 Application 管理
    // 不在这里禁用音频输出，避免干扰其他音频功能
    ESP_LOGI(TAG, "SD card audio stream playback finished, total played: %d bytes", total_played);

    // 新增：恢复到原始采样率，保证唤醒/录音链路使用正确采样率
    ResetSampleRate();

    // 新增：上报音乐播放结束（SD卡），允许时钟UI在满足条件后显示
    Application::GetInstance().SetMusicPlaying(false);

    is_playing_ = false;
    CloseSdCardFile();
}

// 关闭 SD 卡文件
void Esp32Music::CloseSdCardFile() {

    if (sd_file_ != nullptr) {
        fclose(sd_file_);
        sd_file_ = nullptr;
        ESP_LOGI(TAG, "SD card file closed");
    } else {
        ESP_LOGI(TAG, "SD card file was already closed");
    }
    
}

void Esp32Music::ClearPlaylist() {
    std::lock_guard<std::mutex> lock(playlist_mutex_);
    playlist_paths_.clear();
    playlist_index_ = 0;
    playlist_loop_ = false;
    playlist_active_ = false;
    playlist_switch_to_index_.store(-1, std::memory_order_relaxed);
}

bool Esp32Music::GetNextPlaylistTrack(std::string& next_path) {
    std::lock_guard<std::mutex> lock(playlist_mutex_);
    if (!playlist_active_ || playlist_paths_.empty()) {
        return false;
    }

    size_t next_index = playlist_index_ + 1;
    if (next_index < playlist_paths_.size()) {
        playlist_index_ = next_index;
        next_path = playlist_paths_[playlist_index_];
        return true;
    }

    if (playlist_loop_) {
        playlist_index_ = 0;
        next_path = playlist_paths_[playlist_index_];
        return true;
    }

    playlist_paths_.clear();
    playlist_index_ = 0;
    playlist_loop_ = false;
    playlist_active_ = false;
    return false;
}

bool Esp32Music::NextTrack() {
    if (!is_playing_.load()) {
        ESP_LOGW(TAG, "NextTrack: not playing");
        return false;
    }

    int target_index = -1;
    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (!playlist_active_ || playlist_paths_.empty()) {
            ESP_LOGW(TAG, "NextTrack: playlist not active");
            return false;
        }

        size_t next_index = playlist_index_ + 1;
        if (next_index < playlist_paths_.size()) {
            target_index = static_cast<int>(next_index);
        } else if (playlist_loop_) {
            target_index = 0;
        } else {
            ESP_LOGW(TAG, "NextTrack: already at last track");
            return false;
        }
    }

    playlist_switch_to_index_.store(target_index, std::memory_order_relaxed);
    ESP_LOGI(TAG, "NextTrack requested, target index=%d", target_index);
    return true;
}

bool Esp32Music::PrevTrack() {
    if (!is_playing_.load()) {
        ESP_LOGW(TAG, "PrevTrack: not playing");
        return false;
    }

    int target_index = -1;
    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        if (!playlist_active_ || playlist_paths_.empty()) {
            ESP_LOGW(TAG, "PrevTrack: playlist not active");
            return false;
        }

        if (playlist_index_ > 0) {
            target_index = static_cast<int>(playlist_index_ - 1);
        } else if (playlist_loop_) {
            target_index = static_cast<int>(playlist_paths_.size() - 1);
        } else {
            ESP_LOGW(TAG, "PrevTrack: already at first track");
            return false;
        }
    }

    playlist_switch_to_index_.store(target_index, std::memory_order_relaxed);
    ESP_LOGI(TAG, "PrevTrack requested, target index=%d", target_index);
    return true;
}

bool Esp32Music::StartSdCardPlayback(const std::string& file_path) {
    if (is_playing_.load()) {
        ESP_LOGW(TAG, "Music is already playing");
        return false;
    }
    playback_started_ = false;
    paused_for_dialog_ = false;
    user_wakeup_pause_ = false;
    pause_log_emitted_ = false;
    stop_listening_requested_ = false;

    // 注意：不在这里控制设备状态
    // 让 AI 自然地完成回复，播放线程会在状态变为 idle 时自动开始
    // 这样避免了与 Protocol 的状态管理冲突

    if (!OpenSdCardFile(file_path)) {
        ESP_LOGE(TAG, "Failed to open SD card file: %s", file_path.c_str());
        // 新增：文件打开失败时清理资源
        CleanupOnPlaybackFailure();
        return false;
    }

    // 保存歌名用于后续显示
    current_song_name_ = file_path.substr(file_path.find_last_of("/\\") + 1);

    // 重置歌名显示标志，确保每次播放都会显示歌名
    song_name_displayed_ = false;

    // 清空之前的歌词数据，因为SD卡音乐没有歌词
    {
        std::lock_guard<std::mutex> lock(lyrics_mutex_);
        lyrics_.clear();
        current_lyric_index_ = -1;
    }

    // 配置线程栈大小以避免栈溢出
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.stack_size = 16384; // 16KB 栈大小
    cfg.prio = 5;          // 中等优先级
    cfg.thread_name = "sd_audio_stream";
    esp_pthread_set_cfg(&cfg);

    // 确保之前的播放线程已经结束
    if (play_thread_.joinable()) {
        ESP_LOGW(TAG, "Previous play thread still running, waiting for it to finish");
        is_playing_ = false;
        play_thread_.join();
        ESP_LOGI(TAG, "Previous play thread finished");
    }

    // 开始播放线程
    is_playing_ = true;
    play_thread_ = std::thread(&Esp32Music::PlaySdCardAudioStream, this);

    // 新增：上报音乐开始播放（SD卡），抑制时钟UI
    Application::GetInstance().SetMusicPlaying(true);

    ESP_LOGI(TAG, "SD card streaming thread started successfully");
    return true;
}

// 播放 SD 卡中的音乐
bool Esp32Music::PlaySdCardMusic(const std::string& file_path) {
    if (is_playing_.load()) {
        ESP_LOGW(TAG, "Music is already playing");
        return false;
    }
    ClearPlaylist();
    return StartSdCardPlayback(file_path);
}

bool Esp32Music::PlaySdCardPlaylist(const std::vector<std::string>& file_paths, bool loop) {
    if (file_paths.empty()) {
        ESP_LOGW(TAG, "Playlist is empty");
        return false;
    }

    // 若当前正在播放，先停止并等待线程退出
    if (is_playing_.load()) {
        Stop();
    }

    {
        std::lock_guard<std::mutex> lock(playlist_mutex_);
        playlist_paths_ = file_paths;
        playlist_index_ = 0;
        playlist_loop_ = loop;
        playlist_active_ = true;
    }

    // 开始播放第一首
    if (!StartSdCardPlayback(file_paths.front())) {
        ClearPlaylist();
        return false;
    }

    ESP_LOGI(TAG, "Playlist started (count=%u, loop=%d)", (unsigned int)file_paths.size(), loop ? 1 : 0);
    return true;
}

// 在Esp32Music类中添加成员函数，用于搜索SD卡中的MP3文件
std::vector<std::string> Esp32Music::SearchSdCardMusic(const std::string& song_name) {
    // 如果索引未初始化，先初始化
    if (!index_initialized_) {
        ESP_LOGI(TAG, "Index not initialized, initializing now...");
        if (!InitializeIndex()) {
            ESP_LOGW(TAG, "Failed to initialize index, falling back to file system search");
            return SearchSdCardMusicLegacy(song_name);
        }
    }
    
    // 使用索引进行快速搜索
    ESP_LOGI(TAG, "Searching using index for: %s", song_name.c_str());
    auto start_time = esp_timer_get_time();
    
    std::vector<std::string> results = index_manager_->Search(song_name);
    
    auto end_time = esp_timer_get_time();
    ESP_LOGI(TAG, "Index search completed in %.2f ms, found %u results", 
              (end_time - start_time) / 1000.0, results.size());
    
    return results;
}

// 保留原有的文件系统搜索作为备用方案
std::vector<std::string> Esp32Music::SearchSdCardMusicLegacy(const std::string& song_name) {
    std::vector<std::string> matches;
    const char* root_path = "/sdcard/"; // SD卡根目录，根据实际挂载点修改

    // 检查是否有打开的文件（避免冲突）
    if (sd_file_ != nullptr) {
        ESP_LOGW(TAG, "SD card file is already open, search may conflict");
        // 可选择返回空列表或继续执行，根据实际需求决定
    }

    // 打开目录
    DIR* dir = opendir(root_path);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open SD card directory: %s", root_path);
        return matches; // 返回空列表，表示SD卡不可用或目录不存在
    }

    // 遍历目录中的文件
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过目录（只处理文件）
        if (entry->d_type != DT_REG) continue;

        // 检查文件扩展名为.mp3（忽略大小写）
        std::string filename = entry->d_name;
        ESP_LOGI(TAG, "Find SD card File: %s", filename.c_str());
        std::string ext = filename.substr(filename.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != "mp3") continue;

        // 模糊匹配歌曲名（包含关键词即视为匹配）
        std::string lower_filename = filename;
        std::string lower_song_name = song_name;
        std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), ::tolower);
        std::transform(lower_song_name.begin(), lower_song_name.end(), lower_song_name.begin(), ::tolower);

        if (lower_filename.find(lower_song_name) != std::string::npos) {
            // 匹配成功，记录完整路径
            matches.push_back(std::string(root_path) + filename);
            ESP_LOGI(TAG, "Found matching SD card music: %s", filename.c_str());
        }
    }

    closedir(dir);
    return matches;
}

// 新增：初始化音乐索引
bool Esp32Music::InitializeIndex() {
    if (index_initialized_) {
        ESP_LOGW(TAG, "Index already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing music index...");
    
    // 等待 SD 卡挂载完成（最多尝试 10 次，每次 200ms）
    auto sd_ready = []() -> bool {
        struct stat st {};
        return (stat("/sdcard/", &st) == 0) && S_ISDIR(st.st_mode);
    };
    const int kMaxAttempts = 10;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        if (sd_ready()) {
            break;
        }
        ESP_LOGW(TAG, "SD card not mounted yet, retrying (%d/%d)", attempt + 1, kMaxAttempts);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    if (!sd_ready()) {
        ESP_LOGE(TAG, "SD card root not available, skip building index");
        return false;
    }
    
    // 创建索引管理器
    index_manager_ = std::make_unique<MusicIndexManager>();
    if (!index_manager_) {
        ESP_LOGE(TAG, "Failed to create index manager");
        return false;
    }
    
    // 构建索引
    if (!index_manager_->BuildIndex()) {
        ESP_LOGE(TAG, "Failed to build music index");
        return false;
    }
    
    index_initialized_ = true;
    
    // 打印内存统计
    index_manager_->PrintMemoryStats();

    // 打印当前 SD 卡音乐的索引表（分批输出避免单次日志过长）
    // const size_t total_entries = index_manager_->GetIndexSize();
    // const size_t kBatchSize = 50;
    // for (size_t start = 0; start < total_entries; start += kBatchSize) {
    //     size_t batch_count = std::min(kBatchSize, total_entries - start);
    //     index_manager_->PrintIndex(start, batch_count);
    // }
    
    ESP_LOGI(TAG, "Music index initialized successfully");
    return true;
}

// 新增：处理播放失败时的资源清理
void Esp32Music::CleanupOnPlaybackFailure() {
    ESP_LOGW(TAG, "Cleaning up resources after playback failure");
    
    // 停止播放状态
    is_playing_ = false;
    is_downloading_ = false;
    
    // 关闭SD卡文件
    CloseSdCardFile();
    
    // 清理播放相关状态
    current_song_name_.clear();
    song_name_displayed_ = false;
    
    // 清空歌词数据
    {
        std::lock_guard<std::mutex> lock(lyrics_mutex_);
        lyrics_.clear();
        current_lyric_index_ = -1;
    }
    
    // 清空音频缓冲区
    ClearAudioBuffer();
    
    // 重置MP3解码器状态
    if (mp3_decoder_initialized_ && mp3_decoder_) {
        // libhelix-mp3没有重置函数，需要重新初始化
        MP3FreeDecoder(mp3_decoder_);
        mp3_decoder_ = MP3InitDecoder();
        if (mp3_decoder_) {
            ESP_LOGI(TAG, "MP3 decoder reinitialized after failure");
        } else {
            ESP_LOGE(TAG, "Failed to reinitialize MP3 decoder");
            mp3_decoder_initialized_ = false;
        }
    }
    
    // 等待并清理线程（使用标志位避免重复join）
    static bool cleanup_in_progress = false;
    if (!cleanup_in_progress) {
        cleanup_in_progress = true;
        
        // 先设置停止标志，让线程自然退出
        is_playing_ = false;
        is_downloading_ = false;
        is_lyric_running_ = false;
        
        if (play_thread_.joinable()) {
            ESP_LOGW(TAG, "Waiting for play thread to finish during cleanup");
            try {
                play_thread_.join();
            } catch (const std::system_error& e) {
                ESP_LOGW(TAG, "Play thread already joined: %s", e.what());
            }
        }
        if (download_thread_.joinable()) {
            ESP_LOGW(TAG, "Waiting for download thread to finish during cleanup");
            try {
                download_thread_.join();
            } catch (const std::system_error& e) {
                ESP_LOGW(TAG, "Download thread already joined: %s", e.what());
            }
        }
        if (lyric_thread_.joinable()) {
            ESP_LOGW(TAG, "Waiting for lyric thread to finish during cleanup");
            try {
                lyric_thread_.join();
            } catch (const std::system_error& e) {
                ESP_LOGW(TAG, "Lyric thread already joined: %s", e.what());
            }
        }
        
        cleanup_in_progress = false;
    } else {
        ESP_LOGW(TAG, "Cleanup already in progress, skipping thread join");
    }
    
    // 上报音乐播放结束
    Application::GetInstance().SetMusicPlaying(false);
    
    // 清空显示
    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    if (display) {
        display->SetMusicInfo("");
        display->SetChatMessage("lyric", "");
        ESP_LOGI(TAG, "Cleared display after playback failure");
    }
    
    // 重置采样率
    ResetSampleRate();
    
    ESP_LOGI(TAG, "Playback failure cleanup completed");
}

// 支持歌手+歌名的搜索方法
std::vector<std::string> Esp32Music::SearchSdCardMusicWithArtist(const std::string& song_name, const std::string& artist) {
    // 如果索引未初始化，先初始化
    if (!index_initialized_) {
        ESP_LOGI(TAG, "Index not initialized, initializing now...");
        if (!InitializeIndex()) {
            ESP_LOGW(TAG, "Failed to initialize index, falling back to file system search");
            return SearchSdCardMusicLegacy(song_name);
        }
    }
    
    // 先按歌曲名搜索候选
    ESP_LOGI(TAG, "Searching using index for song only: '%s'", song_name.c_str());
    auto start_time = esp_timer_get_time();
    
    std::vector<std::string> candidates = index_manager_->Search(song_name);
    
    // 二次过滤：按歌手包含
    std::vector<std::string> results;
    std::string artist_lower = artist;
    std::transform(artist_lower.begin(), artist_lower.end(), artist_lower.begin(), ::tolower);
    for (const auto& path : candidates) {
        std::string path_lower = path;
        std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(), ::tolower);
        if (path_lower.find(artist_lower) != std::string::npos) {
            results.push_back(path);
        }
    }

    // 降级策略：若无歌手匹配，回退到按歌名匹配的第一条
    if (results.empty() && !candidates.empty()) {
        ESP_LOGW(TAG, "No artist-matched result, fallback to first song-only candidate: %s", candidates.front().c_str());
        results.push_back(candidates.front());
    }
    
    auto end_time = esp_timer_get_time();
    ESP_LOGI(TAG, "Index search (song then artist filter with fallback) completed in %.2f ms, returned %u results", 
              (end_time - start_time) / 1000.0, results.size());
    
    return results;
}

// 实现缺失的虚函数
size_t Esp32Music::GetBufferSize() const {
    return buffer_size_;
}

bool Esp32Music::IsDownloading() const {
    return is_downloading_;
}

// 清理音频缓冲区
void Esp32Music::ClearAudioBuffer() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    while (!audio_buffer_.empty()) {
        AudioChunk chunk = audio_buffer_.front();
        audio_buffer_.pop();
        if (chunk.data) {
            heap_caps_free(chunk.data);
        }
    }
    buffer_size_ = 0;
    ESP_LOGI(TAG, "Audio buffer cleared");
}

// 重置采样率到原始值
void Esp32Music::ResetSampleRate() {
    auto codec = Board::GetInstance().GetAudioCodec();
    if (!codec) {
        ESP_LOGW(TAG, "Audio codec not available for sample rate reset");
        return;
    }
    // 采样率已经通过 AudioStreamPacket 中的 sample_rate 字段正确设置
    // 无需额外操作，播放完成后音频服务会自动管理采样率
    ESP_LOGI(TAG, "Sample rate reset completed");
}

// ========== 未使用的虚方法实现（仅保留以满足接口要求）==========

// 从URL下载音乐（当前不使用）
bool Esp32Music::Download(const std::string& song_name) {
    ESP_LOGW(TAG, "Download method not implemented - use SD card music instead");
    return false;
}

// 播放下载的音乐（当前不使用）
bool Esp32Music::Play() {
    ESP_LOGW(TAG, "Play method not implemented - use PlaySdCardMusic instead");
    return false;
}

// 停止播放（当前不使用）
bool Esp32Music::Stop() {
    // 停止 SD 卡音乐播放
    is_playing_ = false;

    // 等待播放线程退出，避免 Stop() 与 fread/fseek/feof 并发导致竞态（关闭文件句柄后仍被使用）
    if (play_thread_.joinable()) {
        void* current_task = (void*)xTaskGetCurrentTaskHandle();
        void* play_task = play_thread_task_handle_.load(std::memory_order_relaxed);
        if (play_task != nullptr && play_task == current_task) {
            ESP_LOGW(TAG, "Stop called from playback task, skip join/cleanup");
            ClearPlaylist();
            return true;
        }

        ESP_LOGI(TAG, "Waiting for SD playback thread to stop...");
        try {
            play_thread_.join();
        } catch (const std::exception& e) {
            // If join fails, do not close the file handle here; let the playback thread clean up.
            ESP_LOGW(TAG, "Exception while joining play_thread: %s", e.what());
            ClearPlaylist();
            return true;
        }
    }

    ClearPlaylist();
    CloseSdCardFile();
    Application::GetInstance().SetMusicPlaying(false);
    ESP_LOGI(TAG, "Stop called");
    return true;
}

// 获取下载结果（当前不使用）
std::string Esp32Music::GetDownloadResult() {
    ESP_LOGW(TAG, "GetDownloadResult method not implemented");
    return "";
}

// 开始流式播放（当前不使用）
bool Esp32Music::StartStreaming(const std::string& music_url) {
    ESP_LOGW(TAG, "StartStreaming method not implemented - use SD card music instead");
    return false;
}

// 停止流式播放（当前不使用）
bool Esp32Music::StopStreaming() {
    ESP_LOGW(TAG, "StopStreaming method not implemented");
    return false;
}

// ========== 私有方法实现 ==========

// ID3 标签跳过
size_t Esp32Music::SkipId3Tag(uint8_t* data, size_t size) {
    // ID3v2 标签结构：
    // 0-2: "ID3"
    // 3-4: 版本号
    // 5: 标志
    // 6-9: 标签大小（syncsafe integer）

    if (size < 10 || data[0] != 'I' || data[1] != 'D' || data[2] != '3') {
        return 0;  // 不是 ID3v2 标签
    }

    // 解析 syncsafe integer（7-bit 编码）
    uint32_t tag_size = ((data[6] & 0x7F) << 21) |
                        ((data[7] & 0x7F) << 14) |
                        ((data[8] & 0x7F) << 7) |
                        (data[9] & 0x7F);

    // ID3v2 标签总大小 = 10 字节头 + tag_size
    size_t id3_size = 10 + tag_size;

    ESP_LOGD(TAG, "Skipped ID3 tag of size: %u bytes", (unsigned int)id3_size);
    return id3_size;
}

// 更新歌词显示
void Esp32Music::UpdateLyricDisplay(int64_t current_time_ms) {
    // 歌词显示逻辑
    // 当前实现：仅记录日志
    // 如果需要实现实时歌词显示，可在此处添加逻辑

    std::lock_guard<std::mutex> lock(lyrics_mutex_);

    if (lyrics_.empty() || current_lyric_index_ < 0) {
        return;  // 没有歌词数据
    }

    // 在实际应用中，可以根据当前时间匹配相应的歌词
    // 这里仅保留为占位符，便于后续扩展

    ESP_LOGD(TAG, "Lyric display update: time=%lldms, current_index=%d",
             current_time_ms, (int)current_lyric_index_.load());
}
