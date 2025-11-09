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
#include <shared_mutex>
#include <esp_heap_caps.h>

#include "music.h"

// Forward declarations
class Application;

// MP3 decoder support
extern "C" {
#include "mp3dec.h"
}

// Forward declaration for heap_caps_free
extern "C" void heap_caps_free(void* ptr);

// Audio data chunk structure - using smart pointers for memory management
struct AudioChunk {
    std::unique_ptr<uint8_t[], void(*)(void*)> data;
    size_t size;
    
    AudioChunk() : data(nullptr, heap_caps_free), size(0) {}
    AudioChunk(uint8_t* d, size_t s) : data(d, heap_caps_free), size(s) {}
    
    // Move constructor and assignment
    AudioChunk(AudioChunk&& other) noexcept 
        : data(std::move(other.data)), size(other.size) {
        other.size = 0;
    }
    
    AudioChunk& operator=(AudioChunk&& other) noexcept {
        if (this != &other) {
            data = std::move(other.data);
            size = other.size;
            other.size = 0;
        }
        return *this;
    }
    
    // Delete copy constructor and assignment
    AudioChunk(const AudioChunk&) = delete;
    AudioChunk& operator=(const AudioChunk&) = delete;
};

// Audio chunk object pool
class AudioChunkPool {
private:
    std::queue<std::unique_ptr<uint8_t[], void(*)(void*)>> free_chunks_;
    std::mutex pool_mutex_;
    static constexpr size_t CHUNK_SIZE = 8192;
    static constexpr size_t MAX_POOL_SIZE = 32;

public:
    AudioChunkPool() = default;
    ~AudioChunkPool() = default;
    
    std::unique_ptr<uint8_t[], void(*)(void*)> acquire(size_t size);
    void release(std::unique_ptr<uint8_t[], void(*)(void*)> chunk);
    void clear();
};

class Esp32Music : public Music {
public:
    // Constants
    static constexpr size_t MAX_BUFFER_SIZE = 256 * 1024;  // 256KB
    static constexpr size_t MIN_BUFFER_SIZE = 32 * 1024;   // 32KB
    static constexpr size_t CHUNK_SIZE = 8192;              // 8KB
    static constexpr size_t MP3_BUFFER_SIZE = 8192;         // 8KB
    static constexpr size_t MP3_DECODE_THRESHOLD = 4096;    // 4KB
    static constexpr size_t MAX_PCM_SAMPLES = 2304;         // Max MP3 frame samples
    static constexpr int BUFFER_LATENCY_MS = 600;           // Audio buffer latency
    
    // Display mode control
    enum DisplayMode {
        DISPLAY_MODE_SPECTRUM = 0,  // Default spectrum display
        DISPLAY_MODE_LYRICS = 1     // Display lyrics
    };
    
    // Player internal state - simplified version
    enum class PlayerState {
        IDLE,
        ACTIVE,  // Downloading or Playing
        ERROR
    };

private:
    // Application instance reference
    Application& app_;
    std::string last_downloaded_data_;
    std::string current_music_url_;
    std::string current_song_name_;
    bool song_name_displayed_;
    
    // Lyrics related
    std::string current_lyric_url_;
    std::vector<std::pair<int, std::string>> lyrics_;  // Timestamp and lyric text
    mutable std::shared_mutex lyrics_mutex_;  // Reader-writer lock for lyrics
    std::atomic<int> current_lyric_index_;
    std::thread lyric_thread_;
    std::atomic<bool> is_lyric_running_;
    
    std::atomic<DisplayMode> display_mode_;
    std::atomic<bool> is_playing_;
    std::atomic<bool> is_downloading_;
    std::atomic<PlayerState> player_state_;
    std::thread play_thread_;
    std::thread download_thread_;
    int64_t current_play_time_ms_;  // Current playback time (milliseconds)
    int total_frames_decoded_;      // Number of decoded frames

    // Audio buffer
    std::queue<AudioChunk> audio_buffer_;
    std::mutex buffer_mutex_;
    std::condition_variable buffer_cv_;
    std::atomic<size_t> buffer_size_;
    
    // Audio chunk object pool
    AudioChunkPool chunk_pool_;
    
    // MP3 decoder related
    HMP3Decoder mp3_decoder_;
    MP3FrameInfo mp3_frame_info_;
    bool mp3_decoder_initialized_;
    
    // Core streaming methods
    void DownloadAudioStream(const std::string& music_url);
    void PlayAudioStream();
    void ClearAudioBuffer();
    
    // MP3 decoder methods
    bool InitializeMp3Decoder();
    void CleanupMp3Decoder();
    size_t SkipId3Tag(uint8_t* data, size_t size);
    void ResetSampleRate();
    
    // Lyrics methods
    bool DownloadLyrics(const std::string& lyric_url);
    bool ParseLyrics(const std::string& lyric_content);
    void LyricDisplayThread();
    void UpdateLyricDisplay(int64_t current_time_ms);
    
    // Download helper methods
    bool FetchMusicMetadata(const std::string& url, int& status_code);
    bool HandleMusicStatus(const std::string& status);
    bool ProcessAudioUrl(const std::string& audio_url, const std::string& song_name);
    void ProcessLyricUrl(const std::string& lyric_url, const std::string& song_name);
    
    // Thread and state management
    void StopThreadSafely(std::thread& thread, std::atomic<bool>& flag, 
                         const char* thread_name, int timeout_ms);
    void SetStreamingState();
    void SetIdleStateAfterMusic();

    int16_t* final_pcm_data_fft = nullptr;

public:
    Esp32Music();
    ~Esp32Music();

    virtual bool Download(const std::string& song_id) override;
  
    virtual std::string GetDownloadResult() override;
    
    // New methods
    virtual bool StartStreaming(const std::string& music_url) override;
    virtual bool StopStreaming() override;  // Stop streaming playback
    virtual bool IsPlaying() const override { return is_playing_; }
    virtual size_t GetBufferSize() const override { return buffer_size_; }
    virtual bool IsDownloading() const override { return is_downloading_; }
    virtual int16_t* GetAudioData() override { return final_pcm_data_fft; }
    
    // Display mode control methods
    void SetDisplayMode(DisplayMode mode);
    DisplayMode GetDisplayMode() const { return display_mode_.load(); }
};

#endif // ESP32_MUSIC_H
