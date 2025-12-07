#ifndef ESP32_RADIO_H
#define ESP32_RADIO_H

#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <map>

#include "radio.h"

// AAC Simple Decoder for VOV radio streams
// VOV URLs return audio/aacp format which requires AAC decoder
extern "C" {
#include "esp_audio_simple_dec_default.h"
}

// Audio data chunk structure
struct RadioAudioChunk {
    uint8_t* data;
    size_t size;
    
    RadioAudioChunk() : data(nullptr), size(0) {}
    RadioAudioChunk(uint8_t* d, size_t s) : data(d), size(s) {}
};

// Radio station information structure
struct RadioStation {
    std::string name;        // Radio station name
    std::string url;         // Streaming URL
    std::string description; // Description
    std::string genre;       // Genre
    float volume;            // Volume amplification factor (default 1.0 = 100%)
    
    RadioStation() : volume(1.0f) {}
    RadioStation(const std::string& n, const std::string& u, const std::string& d = "", const std::string& g = "", float v = 1.0f)
        : name(n), url(u), description(d), genre(g), volume(v) {}
};

class Esp32Radio : public Radio {
public:
    // Display mode control
    enum DisplayMode {
        DISPLAY_MODE_SPECTRUM = 0,  // Display spectrum
        DISPLAY_MODE_INFO = 1       // Display station information
    };

private:
    std::string current_station_name_;
    std::string current_station_url_;
    bool station_name_displayed_;
    float current_station_volume_;  // Current station's volume amplification factor
    
    // Predefined radio station list
    std::map<std::string, RadioStation> radio_stations_;
    
    std::atomic<DisplayMode> display_mode_;
    std::atomic<bool> is_playing_;
    std::atomic<bool> is_downloading_;
    std::thread play_thread_;
    std::thread download_thread_;
    
    // Audio buffer
    std::queue<RadioAudioChunk> audio_buffer_;
    std::mutex buffer_mutex_;
    std::condition_variable buffer_cv_;
    size_t buffer_size_;
    static constexpr size_t MAX_BUFFER_SIZE = 256 * 1024;  // 256KB buffer
    static constexpr size_t MIN_BUFFER_SIZE = 32 * 1024;   // 32KB minimum playback buffer
    
    // AAC Simple Decoder for VOV radio streams
    esp_audio_simple_dec_handle_t aac_decoder_;
    esp_audio_simple_dec_info_t aac_info_;
    bool aac_decoder_initialized_;
    bool aac_info_ready_;
    std::vector<uint8_t> aac_out_buffer_;
    
    // Private methods
    void InitializeRadioStations();
    void DownloadRadioStream(const std::string& radio_url);
    void PlayRadioStream();
    void ClearAudioBuffer();
    bool InitializeAacDecoder();
    void CleanupAacDecoder();
    void ResetSampleRate();
    
    // ID3 tag handling
    size_t SkipId3Tag(uint8_t* data, size_t size);

    int16_t* final_pcm_data_fft = nullptr;

public:
    Esp32Radio();
    ~Esp32Radio();

    void Initialize();

    // Play a specific station
    virtual bool PlayStation(const std::string& station_name) override;
    
    // Play a station from a specific URL
    virtual bool PlayUrl(const std::string& radio_url, const std::string& station_name = "") override;
    
    // Stop playback
    virtual bool Stop() override;
    
    // Get the list of stations
    virtual std::vector<std::string> GetStationList() const override;
    
    // Get current playback status
    virtual bool IsPlaying() const override { return is_playing_; }
    virtual std::string GetCurrentStation() const override { return current_station_name_; }
    
    // Buffer status
    virtual size_t GetBufferSize() const override { return buffer_size_; }
    virtual bool IsDownloading() const override { return is_downloading_; }
    virtual int16_t* GetAudioData() override { return final_pcm_data_fft; }
    
    // Display mode control methods
    void SetDisplayMode(DisplayMode mode);
    DisplayMode GetDisplayMode() const { return display_mode_.load(); }
};

#endif // ESP32_RADIO_H