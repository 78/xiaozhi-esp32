#ifndef RADIO_H
#define RADIO_H

#include <string>
#include <vector>

class Radio {
public:
    virtual ~Radio() = default;
    
    // Play the specified station
    virtual bool PlayStation(const std::string& station_name) = 0;
    
    // Play a station from the specified URL
    virtual bool PlayUrl(const std::string& radio_url, const std::string& station_name = "") = 0;
    
    // Stop playback
    virtual bool Stop() = 0;
    
    // Get the list of stations
    virtual std::vector<std::string> GetStationList() const = 0;
    
    // Get the current playback status
    virtual bool IsPlaying() const = 0;
    virtual std::string GetCurrentStation() const = 0;
    
    // Buffer status
    virtual size_t GetBufferSize() const = 0;
    virtual bool IsDownloading() const = 0;
    virtual int16_t* GetAudioData() = 0;
};

#endif // RADIO_H