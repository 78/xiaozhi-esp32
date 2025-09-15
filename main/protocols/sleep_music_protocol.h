#ifndef _SLEEP_MUSIC_PROTOCOL_H_
#define _SLEEP_MUSIC_PROTOCOL_H_

#include <web_socket.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <memory>

#define SLEEP_MUSIC_PROTOCOL_CONNECTED_EVENT (1 << 0)

class SleepMusicProtocol {
public:
    static SleepMusicProtocol& GetInstance();
    
    bool OpenAudioChannel();
    void CloseAudioChannel();
    bool IsAudioChannelOpened() const;

private:
    SleepMusicProtocol();
    ~SleepMusicProtocol();
    
    EventGroupHandle_t event_group_handle_;
    std::unique_ptr<WebSocket> websocket_;
    bool is_connected_ = false;
    
    // 睡眠音乐服务器配置
    static constexpr int SAMPLE_RATE = 24000;  // 24kHz
    static constexpr int CHANNELS = 2;         // 立体声
    static constexpr int FRAME_DURATION_MS = 60; // 60ms帧时长
    
    void OnAudioDataReceived(const char* data, size_t len);
};

#endif
