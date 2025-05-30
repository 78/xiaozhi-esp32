#ifndef _BOX_AUDIO_CODEC_H
#define _BOX_AUDIO_CODEC_H

#include "audio_codec.h"

#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

class AdcPdmAudioCodec : public AudioCodec {
private:
    esp_codec_dev_handle_t output_dev_ = nullptr;
    esp_codec_dev_handle_t input_dev_ = nullptr;
    gpio_num_t pa_ctrl_pin_ = GPIO_NUM_NC;

    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

public:
    AdcPdmAudioCodec(int input_sample_rate, int output_sample_rate,
        uint32_t adc_mic_channel, gpio_num_t pdm_speak_p, gpio_num_t pdm_speak_n, gpio_num_t pa_ctl);
    virtual ~AdcPdmAudioCodec();

    virtual void SetOutputVolume(int volume) override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;
    void Start();
};

#endif // _BOX_AUDIO_CODEC_H
