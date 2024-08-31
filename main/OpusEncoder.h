#ifndef _OPUS_ENCODER_H_
#define _OPUS_ENCODER_H_

#include <functional>
#include <string>
#include "opus.h"

class OpusEncoder {
public:
    OpusEncoder();
    ~OpusEncoder();

    void Configure(int sample_rate, int channels, int duration_ms = 60);
    void Encode(const void* pcm, size_t pcm_len, std::function<void(const void*, size_t)> handler);
    bool IsBufferEmpty() const { return in_buffer_.empty(); }

private:
    struct OpusEncoder* audio_enc_ = nullptr;
    int frame_size_;
    int out_size_;
    uint8_t* out_buffer_ = nullptr;
    std::string in_buffer_;
};

#endif // _OPUS_ENCODER_H_
