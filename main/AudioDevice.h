#ifndef _AUDIO_DEVICE_H
#define _AUDIO_DEVICE_H

#include <freertos/FreeRTOS.h>
#include <driver/i2s_std.h>

#include <vector>
#include <string>
#include <functional>

class AudioDevice {
public:
    AudioDevice();
    virtual ~AudioDevice();
    virtual void Initialize();

    void OnInputData(std::function<void(std::vector<int16_t>&& data)> callback);
    void OutputData(std::vector<int16_t>& data);
    virtual void SetOutputVolume(int volume);
    virtual void EnableInput(bool enable);
    virtual void EnableOutput(bool enable);

    inline bool duplex() const { return duplex_; }
    inline bool input_reference() const { return input_reference_; }
    inline int input_sample_rate() const { return input_sample_rate_; }
    inline int output_sample_rate() const { return output_sample_rate_; }
    inline int input_channels() const { return input_channels_; }
    inline int output_channels() const { return output_channels_; }
    inline int output_volume() const { return output_volume_; }

private:
    TaskHandle_t audio_input_task_ = nullptr;
    std::function<void(std::vector<int16_t>&& data)> on_input_data_; 

    void InputTask();
    void CreateSimplexChannels();

protected:
    bool duplex_ = false;
    bool input_reference_ = false;
    bool input_enabled_ = false;
    bool output_enabled_ = false;
    int input_sample_rate_ = 0;
    int output_sample_rate_ = 0;
    int input_channels_ = 1;
    int output_channels_ = 1;
    int output_volume_ = 70;
    i2s_chan_handle_t tx_handle_ = nullptr;
    i2s_chan_handle_t rx_handle_ = nullptr;

    virtual void CreateDuplexChannels();
    virtual int Read(int16_t* dest, int samples);
    virtual int Write(const int16_t* data, int samples);
};

#endif // _AUDIO_DEVICE_H
