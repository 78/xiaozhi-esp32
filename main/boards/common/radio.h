#ifndef RADIO_H
#define RADIO_H

#include <string>
#include <vector>

class Radio {
public:
    virtual ~Radio() = default;
    
    // 播放指定电台
    virtual bool PlayStation(const std::string& station_name) = 0;
    
    // 播放指定URL的电台
    virtual bool PlayUrl(const std::string& radio_url, const std::string& station_name = "") = 0;
    
    // 停止播放
    virtual bool Stop() = 0;
    
    // 获取电台列表
    virtual std::vector<std::string> GetStationList() const = 0;
    
    // 获取当前播放状态
    virtual bool IsPlaying() const = 0;
    virtual std::string GetCurrentStation() const = 0;
    
    // 缓冲区状态
    virtual size_t GetBufferSize() const = 0;
    virtual bool IsDownloading() const = 0;
    virtual int16_t* GetAudioData() = 0;
};

#endif // RADIO_H