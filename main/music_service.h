#ifndef MUSIC_SERVICE_H
#define MUSIC_SERVICE_H

#include <string>
#include <vector>
#include <queue>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <cJSON.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include <memory>
#include <atomic>

#define AUDIO_QUEUE_SIZE 20
#define AUDIO_BUFFER_SIZE 4096
#define HTTP_RESPONSE_BUFFER_SIZE 8192

extern bool g_music_active;
extern bool g_audio_output_managed_by_music;
extern bool g_music_interrupt_requested;

#ifdef __cplusplus
extern "C" {
#endif
    bool CheckMusicActiveStatus();
    bool IsAudioOutputManagedByMusic();
    void RequestMusicInterrupt();
    bool IsMusicInterruptRequested();
    void ClearMusicInterruptRequest();
#ifdef __cplusplus
}
#endif

#define FILTER_BUFFER_SIZE 16

class MusicService {
public:
    MusicService();
    ~MusicService();

    bool Initialize();
    bool SearchMusic(const std::string& keyword);
    bool PlaySong(const std::string& keyword);
    bool Stop();
    bool IsPlaying() const;
    std::string GetCurrentSongInfo() const;
    bool IsMusicActive();
    void CancelSearch();
    void AbortHttpRequest();

    bool SendHttpRequest(
        const char* url,
        esp_http_client_method_t method,
        const char* post_data,
        size_t post_len,
        char* response_buffer,
        size_t buffer_size,
        int* response_len,
        const char* headers = "application/json",
        bool light_logging = false
    );

private:
    bool StartStreaming(const std::string& url);
    void StreamingTask();
    static void AudioPlayerTask(void* arg);
    bool ProcessMp3Data(const uint8_t* mp3_data, int mp3_size, bool is_end_of_stream);
    bool InitMp3Decoder();
    void CleanupMp3Decoder();

    bool FetchLyrics(int song_id);
    bool ParseLyrics(const char* lrc_text);
    bool ConvertTimestampToMilliseconds(const std::string& timestamp, int* milliseconds);
    void UpdateLyrics();
    void DisplayLyric(const std::string& lyric);

    bool is_playing_;
    int song_id_;
    std::string keyword_;
    std::string current_song_name_;
    std::string current_artist_;
    std::string url_to_play_;
    TaskHandle_t streaming_task_handle_;
    QueueHandle_t audio_queue_;
    int sample_rate_;
    int bits_per_sample_;
    int channels_;
    int actual_codec_sample_rate_;
    void* mp3_decoder_;
    bool is_decoder_initialized_;
    uint8_t* decode_input_buffer_;
    uint8_t* decode_output_buffer_;
    int decode_input_buffer_size_;
    int decode_output_buffer_size_;
    std::vector<std::pair<int, std::string>> lyrics_;
    int current_lyric_index_;
    uint32_t playback_start_time_;
    bool has_lyrics_;
    std::atomic<bool> should_continue_search_;
    std::atomic<bool> http_client_close_requested_;
    esp_http_client_handle_t active_http_client_;
};

#endif