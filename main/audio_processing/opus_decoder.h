#ifndef _OPUS_DECODER_WRAPPER_H_
#define _OPUS_DECODER_WRAPPER_H_

#include <functional>
#include <vector>
#include <cstdint>
#include "esp_audio_dec.h"


class OpusDecoderWrapper {
public:
    OpusDecoderWrapper(int sample_rate, int channels, int duration_ms = 60);
    ~OpusDecoderWrapper();

    bool Decode(std::vector<uint8_t>&& opus, std::vector<int16_t>& pcm);
    void ResetState();

private:
    esp_audio_dec_handle_t audio_dec_ = nullptr;
    int frame_size_;
};

#endif // _OPUS_DECODER_WRAPPER_H_
