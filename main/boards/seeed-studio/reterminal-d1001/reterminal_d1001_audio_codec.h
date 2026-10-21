#ifndef XIAOZHI_BOARDS_SEEED_STUDIO_RETERMINAL_D1001_AUDIO_CODEC_H_
#define XIAOZHI_BOARDS_SEEED_STUDIO_RETERMINAL_D1001_AUDIO_CODEC_H_

#include "audio/audio_codec.h"

#include <driver/i2c_master.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

#include <functional>
#include <mutex>
#include <vector>

/**
 * D1001 audio codec: ES8311 playback on I2S0 TX (standard I2S) and ES7210
 * capture on I2S1 RX (TDM 4-slot). The two codecs do not share an I2S
 * topology on this board, so BoxAudioCodec cannot be reused; this class
 * mirrors its esp_codec_dev usage but creates the TX and RX channels
 * independently, following the Seeed D1001 BSP configuration.
 *
 * The NS4150B power amplifier is controlled by a PCA9535 expander pin. The
 * board installs a callback here so the codec can power the PA only after
 * silent DMA frames are queued (pop-free enable order).
 */
class ReTerminalD1001AudioCodec : public AudioCodec {
public:
    ReTerminalD1001AudioCodec(i2c_master_bus_handle_t i2c_bus, int input_sample_rate,
                              int output_sample_rate);
    virtual ~ReTerminalD1001AudioCodec();

    void SetPowerAmpCallback(std::function<void(bool)> callback) {
        pa_callback_ = std::move(callback);
    }

    virtual void SetOutputVolume(int volume) override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;

private:
    const audio_codec_data_if_t* tx_data_if_ = nullptr;
    const audio_codec_data_if_t* rx_data_if_ = nullptr;
    const audio_codec_ctrl_if_t* out_ctrl_if_ = nullptr;
    const audio_codec_if_t* out_codec_if_ = nullptr;
    const audio_codec_ctrl_if_t* in_ctrl_if_ = nullptr;
    const audio_codec_if_t* in_codec_if_ = nullptr;
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;

    esp_codec_dev_handle_t output_dev_ = nullptr;
    esp_codec_dev_handle_t input_dev_ = nullptr;
    std::function<void(bool)> pa_callback_;
    std::mutex data_if_mutex_;
    std::vector<int16_t> rx_buf_;

    void CreateChannels();
    void PrefillSilence();

    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;
};

#endif  // XIAOZHI_BOARDS_SEEED_STUDIO_RETERMINAL_D1001_AUDIO_CODEC_H_
