#ifndef FFT_DSP_PROCESSOR_H
#define FFT_DSP_PROCESSOR_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include "freertos/queue.h"

#include "esp_dsp.h"
#include <math.h>

#include <string>
#include <vector>
#include <functional>

// Input buffer size
#define BUFFER_PROCESS_SIZE 512
// Amount of audio channels
#define I2S_CHANNEL_NUM (2)

class FFTDspProcessor
{
public:
    FFTDspProcessor();
    ~FFTDspProcessor();

    void Initialize();
    void Input(const std::vector<int16_t> &data);
    void OnOutput(std::function<void(std::vector<float> &&data)> callback);

private:
    QueueHandle_t inputQueue; // RTOS 队列句柄
    int audio_chunksize = BUFFER_PROCESS_SIZE;
    __attribute__((aligned(16))) int16_t audio_buffer[BUFFER_PROCESS_SIZE * I2S_CHANNEL_NUM];
    __attribute__((aligned(16))) int16_t wind_buffer[BUFFER_PROCESS_SIZE * I2S_CHANNEL_NUM];
    __attribute__((aligned(16))) float result_data[BUFFER_PROCESS_SIZE];
    std::vector<int16_t> input_buffer_;
    std::function<void(std::vector<float> &&data)> output_callback_;

    void FFTDspProcessorTask();
};

#endif
