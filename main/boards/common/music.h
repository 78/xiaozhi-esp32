#ifndef MUSIC_H
#define MUSIC_H

#include <string>

class Music {
public:
    virtual ~Music() = default;
    
    virtual bool Download(const std::string& song_id) = 0;
    virtual std::string GetDownloadResult() = 0;
    virtual bool StartStreaming(const std::string& music_url) = 0;
    virtual bool StopStreaming() = 0;
    virtual bool IsPlaying() const = 0;
    virtual size_t GetBufferSize() const = 0;
    virtual bool IsDownloading() const = 0;
    virtual int16_t* GetAudioData() = 0;
};

#endif // MUSIC_H

