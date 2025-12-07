#ifndef ESP32_MUSIC_H
#define ESP32_MUSIC_H

#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>

#include "music.h"

// MP3 decoder support
extern "C" {
#include "mp3dec.h"
}

// Audio data chunk structure
struct AudioChunk {
    uint8_t* data;
    size_t size;
    
    AudioChunk() : data(nullptr), size(0) {}
    AudioChunk(uint8_t* d, size_t s) : data(d), size(s) {}
};

class Esp32Music : public Music {
public:
    // Display mode control - moved to public section
    enum DisplayMode {
        DISPLAY_MODE_SPECTRUM = 0,  // Default: display spectrum
        DISPLAY_MODE_LYRICS = 1     // Display lyrics
    };

private:
    std::string last_downloaded_data_;
    std::string current_music_url_;
	std::string artist_name_;   
    std::string title_name_;
    std::string current_song_name_;
    bool song_name_displayed_;
	bool full_info_displayed_;
	bool fft_started_ = false;
    
    // Lyrics-related
    std::string current_lyric_url_;
    std::vector<std::pair<int, std::string>> lyrics_;  // Timestamp and lyric text
    std::mutex lyrics_mutex_;  // Mutex to protect the lyrics_ array
    std::atomic<int> current_lyric_index_;
    std::thread lyric_thread_;
    std::atomic<bool> is_lyric_running_;
    
    std::atomic<DisplayMode> display_mode_;
    std::atomic<bool> is_playing_;
    std::atomic<bool> is_downloading_;
    std::thread play_thread_;
    std::thread download_thread_;
    int64_t current_play_time_ms_;  // Current playback time (milliseconds)
    int64_t last_frame_time_ms_;    // Timestamp of the last frame
    int total_frames_decoded_;      // Total number of decoded frames

    // Audio buffer
    std::queue<AudioChunk> audio_buffer_;
    std::mutex buffer_mutex_;
    std::condition_variable buffer_cv_;
    size_t buffer_size_;
    static constexpr size_t MAX_BUFFER_SIZE = 256 * 1024;  // 256KB buffer (reduced to minimize brownout risk)
    static constexpr size_t MIN_BUFFER_SIZE = 32 * 1024;   // 32KB minimum playback buffer (reduced to minimize brownout risk)
    
    // MP3 decoder-related
    HMP3Decoder mp3_decoder_;
    MP3FrameInfo mp3_frame_info_;
    bool mp3_decoder_initialized_;
    
    // Private methods
    void DownloadAudioStream(const std::string& music_url);
    void PlayAudioStream();
    void ClearAudioBuffer();
    bool InitializeMp3Decoder();
    void CleanupMp3Decoder();
    void ResetSampleRate();  // Reset sample rate to the original value
    
    // Lyrics-related private methods
    bool DownloadLyrics(const std::string& lyric_url);
    bool ParseLyrics(const std::string& lyric_content);
    void LyricDisplayThread();
    void UpdateLyricDisplay(int64_t current_time_ms);
    
    // ID3 tag handling
    size_t SkipId3Tag(uint8_t* data, size_t size);

    int16_t* final_pcm_data_fft = nullptr;

public:
    Esp32Music();
    ~Esp32Music();

    void Initialize();

    virtual bool Download(const std::string& song_name, const std::string& artist_name) override;
  
    virtual std::string GetDownloadResult() override;
    
    // New methods
    virtual bool StartStreaming(const std::string& music_url) override;
    virtual bool StopStreaming() override;  // Stop streaming playback
    virtual size_t GetBufferSize() const override { return buffer_size_; }
    virtual bool IsDownloading() const override { return is_downloading_; }
    virtual int16_t* GetAudioData() override { return final_pcm_data_fft; }
    
    // Display mode control methods
    void SetDisplayMode(DisplayMode mode);
    DisplayMode GetDisplayMode() const { return display_mode_.load(); }
    std::string GetCheckMusicServerUrl();
};

#endif // ESP32_MUSIC_H
