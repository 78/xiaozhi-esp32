#ifndef _AUDIO_DEVICE_H
#define _AUDIO_DEVICE_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <driver/i2s_std.h>

#include <vector>
#include <string>
#include <functional>

class AudioDevice {
public:
    AudioDevice();
    ~AudioDevice();

    void Start(int input_sample_rate, int output_sample_rate);
    void OnInputData(std::function<void(const int16_t*, int)> callback);
    void OutputData(std::vector<int16_t>& data);
    void SetOutputVolume(int volume);

    int input_sample_rate() const { return input_sample_rate_; }
    int output_sample_rate() const { return output_sample_rate_; }
    bool duplex() const { return duplex_; }
    int output_volume() const { return output_volume_; }
private:
    bool duplex_ = false;
    int input_sample_rate_ = 0;
    int output_sample_rate_ = 0;
    int output_volume_ = 80;
    i2s_chan_handle_t tx_handle_ = nullptr;
    i2s_chan_handle_t rx_handle_ = nullptr;

    TaskHandle_t audio_input_task_ = nullptr;
    
    EventGroupHandle_t event_group_;
    std::function<void(const int16_t*, int)> on_input_data_;

    void CreateDuplexChannels();
    void CreateSimplexChannels();
    void InputTask();
    int Read(int16_t* dest, int samples);
    void Write(const int16_t* data, int samples);
};

#endif // _AUDIO_DEVICE_H
