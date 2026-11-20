#ifndef SPEAKER_DSP_H
#define SPEAKER_DSP_H

// Speaker tuning for small speakers on a MAX98357A-style I2S amp (header-only).
//
// Stock firmware only scales the samples by volume^2. A small speaker then crackles on loud bass
// and on peaks. This does, per sample, in this order:
//   1. high-pass filter (removes bass a small speaker cannot play and only distorts)
//   2. makeup gain (makes the voice louder)
//   3. peak limiter + soft clip (peaks never go above the ceiling, so no hard clipping)
//   4. volume: output peak = volume% * SPK_MAX_AMP of full scale (linear)
//
// Tunables (the build workflow writes speaker_dsp_config.h with these):
//   SPK_HIGHPASS_HZ  high-pass corner in Hz
//   SPK_MAKEUP_DB    makeup gain in dB
//   SPK_MAX_AMP      output peak at volume 100, as a fraction of full scale (0.15 = 15%)

#include <math.h>
#include <stdint.h>

#if __has_include("speaker_dsp_config.h")
#include "speaker_dsp_config.h"
#endif

#ifndef SPK_HIGHPASS_HZ
#define SPK_HIGHPASS_HZ 250.0f
#endif
#ifndef SPK_MAKEUP_DB
#define SPK_MAKEUP_DB 6.0f
#endif
#ifndef SPK_MAX_AMP
#define SPK_MAX_AMP 0.15f
#endif

class SpeakerDsp {
public:
    // in: mono 16-bit samples. out: 32-bit samples for the I2S slot. volume: 0-100.
    void Process(const int16_t* in, int32_t* out, int samples, int sample_rate, int volume) {
        if (sample_rate != rate_) {
            Configure(sample_rate);
        }
        const float v = volume <= 0 ? 0.0f : (volume >= 100 ? 1.0f : volume / 100.0f);
        const float scale = v * SPK_MAX_AMP * 2147483647.0f;

        for (int i = 0; i < samples; i++) {
            const float x = static_cast<float>(in[i]) * (1.0f / 32768.0f);

            // 1. high-pass (2nd order, transposed direct form II)
            float y = b0_ * x + z1_;
            z1_ = b1_ * x - a1_ * y + z2_;
            z2_ = b2_ * x - a2_ * y;
            if (fabsf(z1_) < 1e-20f) z1_ = 0.0f;  // avoid denormals in silence
            if (fabsf(z2_) < 1e-20f) z2_ = 0.0f;

            // 2. makeup gain
            y *= makeup_;

            // 3. peak limiter: gain drops fast when a peak comes, recovers slowly
            const float peak = fabsf(y);
            const float want = peak > kCeiling ? kCeiling / peak : 1.0f;
            if (want < gain_) {
                gain_ += (want - gain_) * attack_;
            } else {
                gain_ += (1.0f - gain_) * release_;
            }
            y = SoftClip(y * gain_);

            // 4. volume
            out[i] = static_cast<int32_t>(y * scale);
        }
    }

    float limiter_gain() const { return gain_; }

private:
    static constexpr float kCeiling = 0.9f;

    int rate_ = 0;
    float b0_ = 1, b1_ = 0, b2_ = 0, a1_ = 0, a2_ = 0;
    float z1_ = 0, z2_ = 0;
    float makeup_ = 1, gain_ = 1, attack_ = 1, release_ = 0.001f;

    static float SoftClip(float y) {
        const float knee = 0.9f;
        const float a = fabsf(y);
        if (a <= knee) {
            return y;
        }
        const float s = knee + (1.0f - knee) * tanhf((a - knee) / (1.0f - knee));
        return y < 0 ? -s : s;
    }

    void Configure(int sample_rate) {
        rate_ = sample_rate;
        const double fs = sample_rate > 0 ? sample_rate : 16000;
        double fc = SPK_HIGHPASS_HZ;
        if (fc < 20.0) fc = 20.0;
        if (fc > fs * 0.2) fc = fs * 0.2;
        const double w0 = 2.0 * M_PI * fc / fs;
        const double q = 0.70710678;  // Butterworth
        const double alpha = sin(w0) / (2.0 * q);
        const double c = cos(w0);
        const double a0 = 1.0 + alpha;
        b0_ = static_cast<float>(((1.0 + c) / 2.0) / a0);
        b1_ = static_cast<float>((-(1.0 + c)) / a0);
        b2_ = b0_;
        a1_ = static_cast<float>((-2.0 * c) / a0);
        a2_ = static_cast<float>((1.0 - alpha) / a0);
        z1_ = z2_ = 0.0f;

        makeup_ = static_cast<float>(pow(10.0, SPK_MAKEUP_DB / 20.0));
        attack_ = static_cast<float>(1.0 - exp(-1.0 / (0.0003 * fs)));   // about 0.3 ms
        release_ = static_cast<float>(1.0 - exp(-1.0 / (0.150 * fs)));   // about 150 ms
        gain_ = 1.0f;
    }
};

#endif  // SPEAKER_DSP_H
