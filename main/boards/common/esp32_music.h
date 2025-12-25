#ifndef ESP32_MUSIC_H
#define ESP32_MUSIC_H

#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <memory>

#include <dirent.h> //用于目录遍历
#include "device_state_event.h"

#include "music.h"
#include "music_index_manager.h"

// MP3解码器支持
extern "C" {
#include "mp3dec.h"
}

// 音频数据块结构
struct AudioChunk {
    uint8_t* data;
    size_t size;
    
    AudioChunk() : data(nullptr), size(0) {}
    AudioChunk(uint8_t* d, size_t s) : data(d), size(s) {}
};

class Esp32Music : public Music {
private:
    std::string last_downloaded_data_;
    std::string current_music_url_;
    std::string current_song_name_;
    bool song_name_displayed_;
    
    // 歌词相关
    std::string current_lyric_url_;
    std::vector<std::pair<int, std::string>> lyrics_;  // 时间戳和歌词文本
    std::mutex lyrics_mutex_;  // 保护lyrics_数组的互斥锁
    std::atomic<int> current_lyric_index_;
    std::thread lyric_thread_;
    std::atomic<bool> is_lyric_running_;
    std::atomic<bool> is_playing_;
    std::atomic<bool> is_downloading_;
    std::thread play_thread_;
    std::thread download_thread_;
    int64_t current_play_time_ms_;  // 当前播放时间(毫秒)
    int64_t last_frame_time_ms_;    // 上一帧的时间戳
    int total_frames_decoded_;      // 已解码的帧数

    // 音频缓冲区
    std::queue<AudioChunk> audio_buffer_;
    std::mutex buffer_mutex_;
    std::condition_variable buffer_cv_;
    size_t buffer_size_;
    static constexpr size_t MAX_BUFFER_SIZE = 256 * 1024;  // 256KB缓冲区（降低以减少brownout风险）
    static constexpr size_t MIN_BUFFER_SIZE = 32 * 1024;   // 32KB最小播放缓冲（降低以减少brownout风险）
    
    // MP3解码器相关
    HMP3Decoder mp3_decoder_;
    MP3FrameInfo mp3_frame_info_;
    bool mp3_decoder_initialized_;
    
    // 私有方法
    void DownloadAudioStream(const std::string& music_url);
    void PlayAudioStream();
    void ClearAudioBuffer();
    bool InitializeMp3Decoder();
    void CleanupMp3Decoder();
    void ResetSampleRate();  // 重置采样率到原始值
    
    // 歌词相关私有方法
    bool DownloadLyrics(const std::string& lyric_url);
    bool ParseLyrics(const std::string& lyric_content);
    void LyricDisplayThread();
    void UpdateLyricDisplay(int64_t current_time_ms);
    
    // ID3标签处理
    size_t SkipId3Tag(uint8_t* data, size_t size);

    // 新增：SD 卡文件句柄
    FILE* sd_file_ = nullptr;
    std::mutex sd_card_mutex_;  // 新增：SD卡操作互斥锁

    // 新增：SD 卡音乐搜索和播放相关私有方法
    bool OpenSdCardFile(const std::string& file_path);
    void PlaySdCardAudioStream();
    void CloseSdCardFile();

    // 文件验证
    bool ValidateMP3File(const std::string& file_path);
    void DisplayFileError(const std::string& file_path, const std::string& error_msg);

    // 新增：音乐索引管理器
    std::unique_ptr<MusicIndexManager> index_manager_;
    bool index_initialized_;
    std::atomic<bool> paused_for_dialog_{false};   // 唤醒/对话时暂停标记
    std::atomic<bool> pause_log_emitted_{false};   // 避免重复日志
    int listening_toggle_retries_ = 0;             // 聆听态切回待机的尝试次数
    std::atomic<bool> playback_started_{false};    // 首帧解码完成后才允许对话暂停
    std::atomic<bool> user_wakeup_pause_{false};   // 用户主动唤醒导致的暂停（区别于AI自己的状态切换）
    std::atomic<bool> stop_listening_requested_{false}; // 播放开始时请求切回待机（仅一次）

    // 新增：索引管理方法
    bool InitializeIndex();
    std::vector<std::string> SearchSdCardMusicLegacy(const std::string& song_name);

    // 新增：处理播放失败时的资源清理
    void CleanupOnPlaybackFailure();

public:
    Esp32Music();
    ~Esp32Music();

    // 实现Music接口
    virtual bool Download(const std::string& song_name) override;
    virtual bool Play() override;
    virtual bool Stop() override;
    virtual std::string GetDownloadResult() override;
    
    // 新增流式播放相关方法
    virtual bool StartStreaming(const std::string& music_url) override;
    virtual bool StopStreaming() override;  // 停止流式播放
    virtual size_t GetBufferSize() const override;
    virtual bool IsDownloading() const override;

     virtual bool PlaySdCardMusic(const std::string& file_path) override;
     virtual std::vector<std::string> SearchSdCardMusic(const std::string& song_name) override;
    
    // 新增：支持歌手+歌名的搜索方法
    virtual std::vector<std::string> SearchSdCardMusicWithArtist(const std::string& song_name, const std::string& artist = "") override;
};

#endif // ESP32_MUSIC_H
