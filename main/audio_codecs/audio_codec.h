#ifndef _AUDIO_CODEC_H
#define _AUDIO_CODEC_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <driver/i2s_std.h>

#include <vector>
#include <string>
#include <functional>
#include <list>
#include <mutex>
#include <condition_variable>

#include "board.h"

#define AUDIO_EVENT_OUTPUT_DONE (1 << 0)

class AudioCodec {
public:
    AudioCodec();
    virtual ~AudioCodec();
    
    virtual void SetOutputVolume(int volume);
    virtual void EnableInput(bool enable);
    virtual void EnableOutput(bool enable);

    void Start();
    void OnInputData(std::function<void(std::vector<int16_t>&& data)> callback);
    void OutputData(std::vector<int16_t>& data);
    void WaitForOutputDone();
    void ClearOutputQueue();

    inline bool duplex() const { return duplex_; }
    inline bool input_reference() const { return input_reference_; }
    inline int input_sample_rate() const { return input_sample_rate_; }
    inline int output_sample_rate() const { return output_sample_rate_; }
    inline int input_channels() const { return input_channels_; }
    inline int output_channels() const { return output_channels_; }
    inline int output_volume() const { return output_volume_; }

private:
    TaskHandle_t audio_input_task_ = nullptr;
    TaskHandle_t audio_output_task_ = nullptr;
    std::function<void(std::vector<int16_t>&& data)> on_input_data_; 
    std::list<std::vector<int16_t>> audio_output_queue_;
    std::mutex audio_output_queue_mutex_;
    std::condition_variable audio_output_queue_cv_;
    EventGroupHandle_t audio_event_group_ = nullptr;
    IRAM_ATTR static bool on_sent(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);

    void InputTask();
    void OutputTask();

protected:
    i2s_chan_handle_t tx_handle_ = nullptr;
    i2s_chan_handle_t rx_handle_ = nullptr;

    bool duplex_ = false;
    bool input_reference_ = false;
    bool input_enabled_ = false;
    bool output_enabled_ = false;
    int input_sample_rate_ = 0;
    int output_sample_rate_ = 0;
    int input_channels_ = 1;
    int output_channels_ = 1;
    int output_volume_ = 70;

    virtual int Read(int16_t* dest, int samples) = 0;
    virtual int Write(const int16_t* data, int samples) = 0;
};

#endif // _AUDIO_CODEC_H
