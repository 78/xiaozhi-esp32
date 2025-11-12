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

// MP3解码器支持
extern "C" {
#include "mp3dec.h"
}

// 音频数据块结构
struct RadioAudioChunk {
    uint8_t* data;
    size_t size;
    
    RadioAudioChunk() : data(nullptr), size(0) {}
    RadioAudioChunk(uint8_t* d, size_t s) : data(d), size(s) {}
};

// 电台信息结构
struct RadioStation {
    std::string name;        // 电台名称
    std::string url;         // 流媒体URL
    std::string description; // 描述
    std::string genre;       // 类型
    
    RadioStation() {}
    RadioStation(const std::string& n, const std::string& u, const std::string& d = "", const std::string& g = "")
        : name(n), url(u), description(d), genre(g) {}
};

class Esp32Radio : public Radio {
public:
    // 显示模式控制
    enum DisplayMode {
        DISPLAY_MODE_SPECTRUM = 0,  // 显示频谱
        DISPLAY_MODE_INFO = 1       // 显示电台信息
    };

private:
    std::string current_station_name_;
    std::string current_station_url_;
    bool station_name_displayed_;
    
    // 预定义电台列表
    std::map<std::string, RadioStation> radio_stations_;
    
    std::atomic<DisplayMode> display_mode_;
    std::atomic<bool> is_playing_;
    std::atomic<bool> is_downloading_;
    std::thread play_thread_;
    std::thread download_thread_;
    
    // 音频缓冲区
    std::queue<RadioAudioChunk> audio_buffer_;
    std::mutex buffer_mutex_;
    std::condition_variable buffer_cv_;
    size_t buffer_size_;
    static constexpr size_t MAX_BUFFER_SIZE = 256 * 1024;  // 256KB缓冲区
    static constexpr size_t MIN_BUFFER_SIZE = 32 * 1024;   // 32KB最小播放缓冲
    
    // MP3解码器相关
    HMP3Decoder mp3_decoder_;
    MP3FrameInfo mp3_frame_info_;
    bool mp3_decoder_initialized_;
    
    // 私有方法
    void InitializeRadioStations();
    void DownloadRadioStream(const std::string& radio_url);
    void PlayRadioStream();
    void ClearAudioBuffer();
    bool InitializeMp3Decoder();
    void CleanupMp3Decoder();
    void ResetSampleRate();
    
    // ID3标签处理
    size_t SkipId3Tag(uint8_t* data, size_t size);

    int16_t* final_pcm_data_fft = nullptr;

public:
    Esp32Radio();
    ~Esp32Radio();

    // 播放指定电台
    virtual bool PlayStation(const std::string& station_name) override;
    
    // 播放指定URL的电台
    virtual bool PlayUrl(const std::string& radio_url, const std::string& station_name = "") override;
    
    // 停止播放
    virtual bool Stop() override;
    
    // 获取电台列表
    virtual std::vector<std::string> GetStationList() const override;
    
    // 获取当前播放状态
    virtual bool IsPlaying() const override { return is_playing_; }
    virtual std::string GetCurrentStation() const override { return current_station_name_; }
    
    // 缓冲区状态
    virtual size_t GetBufferSize() const override { return buffer_size_; }
    virtual bool IsDownloading() const override { return is_downloading_; }
    virtual int16_t* GetAudioData() override { return final_pcm_data_fft; }
    
    // 显示模式控制方法
    void SetDisplayMode(DisplayMode mode);
    DisplayMode GetDisplayMode() const { return display_mode_.load(); }
};

#endif // ESP32_RADIO_H