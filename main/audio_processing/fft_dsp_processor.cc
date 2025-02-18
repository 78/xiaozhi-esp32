#include "fft_dsp_processor.h"
#include <esp_log.h>
#include <malloc.h>

#define PROCESSOR_RUNNING 0x01

static const char *TAG = "FFTDspProcessor";

FFTDspProcessor::FFTDspProcessor()
{
}

void FFTDspProcessor::Initialize()
{
    // Init esp-dsp library to use fft functionality
    ESP_LOGI(TAG, "FFT Initialize");
    esp_err_t ret = dsps_fft2r_init_sc16(NULL, CONFIG_DSP_MAX_FFT_SIZE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Not possible to initialize FFT esp-dsp from library!");
        return;
    }
    dsps_wind_blackman_harris_f32(result_data, audio_chunksize);
    for (int i = 0; i < audio_chunksize; i++)
    {
        wind_buffer[i * 2 + 0] = (int16_t)(result_data[i] * 32767);
        wind_buffer[i * 2 + 1] = wind_buffer[i * 2 + 0];
    }
    inputQueue = xQueueCreate(10, sizeof(std::vector<int16_t> *));
    xTaskCreate([](void *arg)
                {
        auto this_ = (FFTDspProcessor*)arg;
        this_->FFTDspProcessorTask();
        vTaskDelete(NULL); }, "fft_dsp_communication", 4096 * 1, this, 1, NULL);
}

FFTDspProcessor::~FFTDspProcessor()
{
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
    ESP_LOGI(TAG, "FFT communication task started");

    while (true)
    {
        std::vector<int16_t> *dataPtr;
        if (xQueueReceive(inputQueue, &dataPtr, portMAX_DELAY) == pdTRUE)
        {
            std::vector<int16_t> &data = *dataPtr;

            auto raw_buffer = data.data();
            auto raw_buffer_size = data.size();
            for (size_t i = 0; i < audio_chunksize; i++)
            {
                if (i < raw_buffer_size)
                {
                    audio_buffer[2 * i + 0] = raw_buffer[i];
                    audio_buffer[2 * i + 1] = audio_buffer[2 * i + 0];
                }
                else
                {
                    audio_buffer[2 * i + 0] = 0;
                    audio_buffer[2 * i + 1] = audio_buffer[2 * i + 0];
                }
            }
            dsps_mul_s16_ansi(audio_buffer, wind_buffer, audio_buffer, audio_chunksize * 2, 1, 1, 1, 15);

            // Call FFT bit reverse
            dsps_fft2r_sc16_ae32(audio_buffer, audio_chunksize);
            dsps_bit_rev_sc16_ansi(audio_buffer, audio_chunksize);
            // Convert spectrum from two input channels to two
            // spectrums for two channels.
            dsps_cplx2reC_sc16(audio_buffer, audio_chunksize);

            for (int i = 0; i < audio_chunksize; i++)
            {
#if (false)
                result_data[i] = sqrt((audio_buffer[i * 2 + 0] * audio_buffer[i * 2 + 0] + audio_buffer[i * 2 + 1] * audio_buffer[i * 2 + 1]));
#else
                result_data[i] = 10 * log10f((audio_buffer[i * 2 + 0] * audio_buffer[i * 2 + 0] + audio_buffer[i * 2 + 1] * audio_buffer[i * 2 + 1]) / audio_chunksize);
#endif
            }
            if (output_callback_)
            {
                output_callback_(std::vector<float>(result_data, result_data + audio_chunksize));
            }
            delete dataPtr; // 处理完数据后删除数据指针
        }
    }
}
