#ifndef AUDIO_SAMPLE_PEAK_H_
#define AUDIO_SAMPLE_PEAK_H_

#include <cstddef>
#include <cstdint>

inline uint32_t AudioSamplePeak(const int16_t* samples, size_t count) {
    uint32_t peak = 0;
    for (size_t i = 0; i < count; ++i) {
        const int32_t value = samples[i];
        const uint32_t amplitude = value < 0 ? -value : value;
        if (amplitude > peak) {
            peak = amplitude;
        }
    }
    return peak;
}

#endif  // AUDIO_SAMPLE_PEAK_H_
