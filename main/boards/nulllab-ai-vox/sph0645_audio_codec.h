#pragma once

#ifndef _SPH0645_AUDIO_CODEC_H_
#define _SPH0645_AUDIO_CODEC_H_

#include "audio_codecs/audio_codec.h"

class Sph0645AudioCodec : public AudioCodec {
 public:
  Sph0645AudioCodec(int input_sample_rate, int output_sample_rate,
                    gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout,
                    gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din);
  virtual ~Sph0645AudioCodec();

 private:
  int Write(const int16_t* data, int samples) override;
  int Read(int16_t* dest, int samples) override;
};

#endif