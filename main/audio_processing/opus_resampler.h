#ifndef OPUS_RESAMPLER_H
#define OPUS_RESAMPLER_H

#include <cstdint>
#include "esp_ae_rate_cvt.h"

class OpusResampler {
public:
    OpusResampler();
    ~OpusResampler();

    void Configure(int input_sample_rate, int output_sample_rate);
    void Process(const int16_t *input, int input_samples, int16_t *output);
    int GetOutputSamples(int input_samples) const;

    int input_sample_rate() const { return input_sample_rate_; }
    int output_sample_rate() const { return output_sample_rate_; }

private:
     esp_ae_rate_cvt_handle_t esp_rate_;
    int input_sample_rate_;
    int output_sample_rate_;
};

#endif


