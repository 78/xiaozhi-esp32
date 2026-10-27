#ifndef _CARDPUTER_AUDIO_CODEC_H
#define _CARDPUTER_AUDIO_CODEC_H

#include "audio_codec.h"

#include <driver/gpio.h>
#include <driver/i2s_pdm.h>
#include <driver/i2s_std.h>
#include <atomic>
#include <mutex>

// Half-duplex codec for the original M5Stack Cardputer.
//
// The SPM1423 PDM mic clock and the NS4168 amplifier LRCK are both wired to
// GPIO43, so the mic and speaker can never run at the same time. Both use
// I2S0 (the only port with PDM RX on ESP32-S3); the channel is torn down and
// recreated in the other mode whenever the active direction changes.
//
// Speaker wins: while audio is being played (and for a short hold after the
// last write so the DMA buffers drain), Read() returns silence instead of
// touching the hardware. Once playback goes quiet the mic takes the pin back.
class CardputerAudioCodec : public AudioCodec {
public:
    CardputerAudioCodec(int input_sample_rate, int output_sample_rate,
        gpio_num_t spk_bclk, gpio_num_t spk_ws, gpio_num_t spk_dout,
        gpio_num_t mic_clk, gpio_num_t mic_din, float mic_gain);
    virtual ~CardputerAudioCodec();

    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;

private:
    enum class Mode { kIdle, kSpeaker, kMic };

    gpio_num_t spk_bclk_;
    gpio_num_t spk_ws_;
    gpio_num_t spk_dout_;
    gpio_num_t mic_clk_;
    gpio_num_t mic_din_;

    std::mutex mutex_;
    Mode mode_ = Mode::kIdle;
    i2s_chan_handle_t chan_ = nullptr;
    std::atomic<int> writers_waiting_{0};
    std::atomic<int64_t> last_write_us_{0};

    void SwitchMode(Mode mode);
    void ReleaseChannel();
    void StartSpeaker();
    void StartMic();
    bool SpeakerBusy() const;

    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;
};

#endif // _CARDPUTER_AUDIO_CODEC_H
