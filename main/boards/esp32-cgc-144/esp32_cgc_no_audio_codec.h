#ifndef _NO_AUDIO_CODEC_H
#define _NO_AUDIO_CODEC_H

#include "audio_codecs/audio_codec.h"

#include <driver/gpio.h>
#include <driver/i2s_pdm.h>

class ESP32CGC_NoAudioCodec : public AudioCodec {
private:
    virtual int Write(const int16_t* data, int samples) override;
    virtual int Read(int16_t* dest, int samples) override;

public:
    virtual ~ESP32CGC_NoAudioCodec();
};

class ESP32CGC_NoAudioCodecDuplex : public ESP32CGC_NoAudioCodec {
public:
    ESP32CGC_NoAudioCodecDuplex(int input_sample_rate, int output_sample_rate, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din);
};

class ESP32CGC_ATK_NoAudioCodecDuplex : public ESP32CGC_NoAudioCodec {
public:
    ESP32CGC_ATK_NoAudioCodecDuplex(int input_sample_rate, int output_sample_rate, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din);
};

class ESP32CGC_NoAudioCodecSimplex : public ESP32CGC_NoAudioCodec {
public:
    ESP32CGC_NoAudioCodecSimplex(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din);
    ESP32CGC_NoAudioCodecSimplex(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, i2s_std_slot_mask_t spk_slot_mask, gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din, i2s_std_slot_mask_t mic_slot_mask);
};

class ESP32CGC_NoAudioCodecSimplexPdm : public ESP32CGC_NoAudioCodec {
public:
    ESP32CGC_NoAudioCodecSimplexPdm(int input_sample_rate, int output_sample_rate, gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout, gpio_num_t mic_sck,  gpio_num_t mic_din);
};

#endif // _NO_AUDIO_CODEC_H
