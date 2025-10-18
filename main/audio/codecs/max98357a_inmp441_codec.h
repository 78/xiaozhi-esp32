#ifndef _MAX98357A_INMP441_CODEC_H
#define _MAX98357A_INMP441_CODEC_H

#include "audio_codec.h"
#include <driver/gpio.h>

class Max98357aInmp441Codec : public AudioCodec {
private:
    gpio_num_t sd_mode_pin_;
    
    void CreateDuplexChannels(gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din);
    
    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

public:
    Max98357aInmp441Codec(int input_sample_rate, int output_sample_rate,
        gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
        gpio_num_t sd_mode_pin = GPIO_NUM_NC);
    virtual ~Max98357aInmp441Codec();

    virtual void SetOutputVolume(int volume) override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;
};

#endif // _MAX98357A_INMP441_CODEC_H
