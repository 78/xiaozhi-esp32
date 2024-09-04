#ifndef _OPUS_ENCODER_H_
#define _OPUS_ENCODER_H_

#include <functional>
#include <string>
#include <vector>
#include <memory>

#include "lwip/sockets.h"
#include "opus.h"


class OpusEncoder {
public:
    OpusEncoder();
    ~OpusEncoder();

    void Configure(int sample_rate, int channels, int duration_ms = 60);
    void SetComplexity(int complexity);
    void Encode(const iovec pcm, std::function<void(const iovec opus)> handler);
    bool IsBufferEmpty() const { return in_buffer_.empty(); }
    void ResetState();

private:
    struct OpusEncoder* audio_enc_ = nullptr;
    int frame_size_;
    std::vector<uint8_t> out_buffer_;
    std::vector<int16_t> in_buffer_;
};

#endif // _OPUS_ENCODER_H_
