#ifndef MUSIC_H
#define MUSIC_H

#include <string>

class Music {
public:
    virtual ~Music() = default;  // Add a virtual destructor
    
    virtual bool Download(const std::string& song_name, const std::string& artist_name = "") = 0;
    virtual std::string GetDownloadResult() = 0;
    
    // Add methods related to streaming playback
    virtual bool StartStreaming(const std::string& music_url) = 0;
    virtual bool StopStreaming() = 0;  // Stop streaming playback
    virtual size_t GetBufferSize() const = 0;
    virtual bool IsDownloading() const = 0;
    virtual int16_t* GetAudioData() = 0;
};

#endif // MUSIC_H 