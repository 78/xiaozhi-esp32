#ifndef _OPUS_ENCODER_WRAPPER_H_
#define _OPUS_ENCODER_WRAPPER_H_

#include <functional>
#include <vector>
#include <memory>
#include <cstdint>

#include "esp_opus_enc.h"

#define MAX_OPUS_PACKET_SIZE 1500


class OpusEncoderWrapper {
public:
    OpusEncoderWrapper(int sample_rate, int channels, int duration_ms = 60);
    ~OpusEncoderWrapper();

    void SetDtx(bool enable);
    void SetComplexity(int complexity);
    void Encode(std::vector<int16_t>&& pcm, std::function<void(std::vector<uint8_t>&& opus)> handler);
    bool IsBufferEmpty() const { return in_buffer_.empty(); }
    void ResetState();

private:
    esp_audio_enc_handle_t opus_enc_ = nullptr;
    int frame_size_;
    int out_byte_size_;
    std::vector<int16_t> in_buffer_;
};

#endif // _OPUS_ENCODER_H_
