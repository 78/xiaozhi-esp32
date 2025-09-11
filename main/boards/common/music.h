#ifndef MUSIC_H
#define MUSIC_H

#include <string>

class Music {
public:
    virtual ~Music() = default;  // 添加虚析构函数
    
    virtual bool Download(const std::string& song_name, const std::string& artist_name = "") = 0;
    virtual std::string GetDownloadResult() = 0;
    
    // 新增流式播放相关方法
    virtual bool StartStreaming(const std::string& music_url) = 0;
    virtual bool StopStreaming() = 0;  // 停止流式播放
    virtual size_t GetBufferSize() const = 0;
    virtual bool IsDownloading() const = 0;
    virtual bool IsPlaying() const = 0;
    virtual bool IsPaused() const = 0;
    virtual int16_t* GetAudioData() = 0;
    
    // MCP工具需要的方法
    virtual bool PlaySong() = 0;
    virtual bool SetVolume(int volume) = 0;
    virtual bool StopSong() = 0;
    virtual bool PauseSong() = 0;
    virtual bool ResumeSong() = 0;
};

#endif // MUSIC_H 