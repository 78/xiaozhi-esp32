#ifndef _AUDIO_DEVICE_H
#define _AUDIO_DEVICE_H

#include "opus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/i2s_std.h"

#include <vector>
#include <string>
#include <functional>

enum AudioPacketType {
    kAudioPacketTypeUnkonwn = 0,
    kAudioPacketTypeStart,
    kAudioPacketTypeStop,
    kAudioPacketTypeData,
    kAudioPacketTypeSentenceStart,
    kAudioPacketTypeSentenceEnd
};

struct AudioPacket {
    AudioPacketType type = kAudioPacketTypeUnkonwn;
    std::string text;
    std::vector<uint8_t> opus;
    std::vector<int16_t> pcm;
    uint32_t timestamp;
};

class AudioDevice {
public:
    AudioDevice();
    ~AudioDevice();

    void Start(int input_sample_rate, int output_sample_rate);
    int Read(int16_t* dest, int samples);
    void Write(const int16_t* data, int samples);
    void QueueAudioPacket(AudioPacket* packet);
    void OnStateChanged(std::function<void()> callback);
    void Break();

    int input_sample_rate() const { return input_sample_rate_; }
    int output_sample_rate() const { return output_sample_rate_; }
    bool duplex() const { return duplex_; }
    bool playing() const { return playing_; }
    uint32_t last_timestamp() const { return last_timestamp_; }

private:
    bool playing_ = false;
    bool breaked_ = false;
    bool duplex_ = false;
    int input_sample_rate_ = 0;
    int output_sample_rate_ = 0;
    uint32_t last_timestamp_ = 0;

    i2s_chan_handle_t tx_handle_ = nullptr;
    i2s_chan_handle_t rx_handle_ = nullptr;

    QueueHandle_t audio_play_queue_ = nullptr;
    TaskHandle_t audio_play_task_ = nullptr;
    
    EventGroupHandle_t event_group_;
    std::function<void()> on_state_changed_;

    void CreateDuplexChannels();
    void CreateSimplexChannels();
    void AudioPlayTask();
};

#endif // _AUDIO_DEVICE_H
