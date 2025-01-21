#include "fft_dsp_processor..h"
#include <esp_log.h>
#include <malloc.h>

#define PROCESSOR_RUNNING 0x01

static const char *TAG = "FFTDspProcessor";

FFTDspProcessor::FFTDspProcessor()
    : audio_buffer(nullptr), wind_buffer(nullptr)
{
}

void FFTDspProcessor::Initialize()
{
    // Init esp-dsp library to use fft functionality
    esp_err_t ret = dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Not possible to initialize FFT esp-dsp from library!");
        return;
    }
    inputQueue = xQueueCreate(10, sizeof(std::vector<int16_t> *));
    xTaskCreate([](void *arg)
                {
        auto this_ = (FFTDspProcessor*)arg;
        this_->FFTDspProcessorTask();
        vTaskDelete(NULL); }, "fft_dsp_communication", 4096 * 2, this, 1, NULL);
}

FFTDspProcessor::~FFTDspProcessor()
{
    if (audio_buffer)
        free(audio_buffer);
    if (wind_buffer)
        free(wind_buffer);
}

void FFTDspProcessor::Input(const std::vector<int16_t> &data)
{
    // ESP_LOGI(TAG, "FFT audio size: %d", data.size());
    std::vector<int16_t> *dataPtr = new std::vector<int16_t>(data);
    if (xQueueSend(inputQueue, &dataPtr, portMAX_DELAY) != pdTRUE)
    {
        delete dataPtr;
        ESP_LOGE(TAG, "Failed to send data to queue");
    }
}

void FFTDspProcessor::OnOutput(std::function<void(std::vector<float> &&data)> callback)
{
    output_callback_ = callback;
}

void FFTDspProcessor::FFTDspProcessorTask()
{
    // ESP_LOGI(TAG, "FFT communication task started");

    while (true)
    {
        std::vector<int16_t> *dataPtr;
        if (xQueueReceive(inputQueue, &dataPtr, portMAX_DELAY) == pdTRUE)
        {
            std::vector<int16_t> &data = *dataPtr;

            if (data.size() != audio_chunksize)
            {
                if (audio_buffer)
                    free(audio_buffer);
                if (wind_buffer)
                    free(wind_buffer);
                audio_chunksize = data.size();
                // Allocate audio buffer and check for result
                audio_buffer = (float *)memalign(16, (audio_chunksize + 16) * sizeof(float) * I2S_CHANNEL_NUM);
                // Allocate buffer for window
                wind_buffer = (float *)memalign(16, (audio_chunksize + 16) * sizeof(float));
                // Generate window and convert it to int16_t
                dsps_wind_hann_f32(wind_buffer, audio_chunksize);
            }
            auto raw_buffer = data.data();
            for (size_t i = 0; i < audio_chunksize; i++)
            {
                audio_buffer[2 * i + 0] = raw_buffer[i] * wind_buffer[i];
                audio_buffer[2 * i + 1] = 0;
            }

            // Call FFT bit reverse
            dsps_fft2r_fc32(audio_buffer, audio_chunksize);
            dsps_bit_rev_fc32(audio_buffer, audio_chunksize);
            // Convert spectrum from two input channels to two
            // spectrums for two channels.
            dsps_cplx2reC_fc32(audio_buffer, audio_chunksize);

            // The output data array presented as moving average for input in dB
            for (int i = 0; i < audio_chunksize; i++)
            {
                float spectrum_sqr = audio_buffer[i * 2 + 0] * audio_buffer[i * 2 + 0] + audio_buffer[i * 2 + 1] * audio_buffer[i * 2 + 1];
                float spectrum_dB = 10 * log10f(0.1 + spectrum_sqr);
                result_data[i] = spectrum_dB;
            }
            if (output_callback_)
            {
                output_callback_(std::vector<float>(result_data, result_data + audio_chunksize));
            }
            delete dataPtr; // 处理完数据后删除数据指针
        }
    }
}
