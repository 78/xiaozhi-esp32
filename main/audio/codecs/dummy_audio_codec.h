#ifndef _DUMMY_AUDIO_CODEC_H
#define _DUMMY_AUDIO_CODEC_H

#include "audio_codec.h"

class DummyAudioCodec : public AudioCodec {
private:
    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

public:
    DummyAudioCodec(int input_sample_rate, int output_sample_rate);
    virtual ~DummyAudioCodec();
};

#endif // _DUMMY_AUDIO_CODEC_H