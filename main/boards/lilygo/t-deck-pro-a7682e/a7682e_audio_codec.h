#ifndef _T_DECK_PRO_A7682E_AUDIO_CODEC_H_
#define _T_DECK_PRO_A7682E_AUDIO_CODEC_H_

#include "a7682e_modem.h"
#include "audio_codec.h"

#include <mutex>

class A7682eAudioCodec : public AudioCodec {
public:
    explicit A7682eAudioCodec(A7682eModem& modem);
    ~A7682eAudioCodec() override;

    void SetOutputVolume(int volume) override;
    void EnableInput(bool enable) override;
    void EnableOutput(bool enable) override;
    void OutputData(std::vector<int16_t>& data) override;
    void Start() override;

protected:
    int Read(int16_t* dest, int samples) override;
    int Write(const int16_t* data, int samples) override;

private:
    A7682eModem& modem_;
    std::mutex data_if_mutex_;
};

#endif  // _T_DECK_PRO_A7682E_AUDIO_CODEC_H_
