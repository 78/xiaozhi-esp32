#ifndef _TCIRCLES3_AUDIO_CODEC_H
#define _TCIRCLES3_AUDIO_CODEC_H

#include "audio_codec.h"

#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

class Tcircles3AudioCodec : public AudioCodec {
private:
    const audio_codec_data_if_t *data_if_ = nullptr;
    const audio_codec_ctrl_if_t *out_ctrl_if_ = nullptr;
    const audio_codec_if_t *out_codec_if_ = nullptr;
    const audio_codec_ctrl_if_t *in_ctrl_if_ = nullptr;
    const audio_codec_if_t *in_codec_if_ = nullptr;
    const audio_codec_gpio_if_t *gpio_if_ = nullptr;

    uint32_t volume_ = 70;

    void CreateVoiceHardware(gpio_num_t mic_bclk, gpio_num_t mic_ws, gpio_num_t mic_data,gpio_num_t spkr_bclk, gpio_num_t spkr_lrclk, gpio_num_t spkr_data);

    virtual int Read(int16_t *dest, int samples) override;
    virtual int Write(const int16_t *data, int samples) override;

public:
    Tcircles3AudioCodec(int input_sample_rate, int output_sample_rate,
        gpio_num_t mic_bclk, gpio_num_t mic_ws, gpio_num_t mic_data,
        gpio_num_t spkr_bclk, gpio_num_t spkr_lrclk, gpio_num_t spkr_data,
        bool input_reference);
    virtual ~Tcircles3AudioCodec();

    virtual void SetOutputVolume(int volume) override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;
};

#endif // _BOX_AUDIO_CODEC_H
