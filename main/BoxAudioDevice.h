#ifndef _BOX_AUDIO_DEVICE_H
#define _BOX_AUDIO_DEVICE_H

#include "AudioDevice.h"
#include <driver/i2c_master.h>
#include <driver/i2s_tdm.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>


class BoxAudioDevice : public AudioDevice {
public:
    BoxAudioDevice();
    virtual ~BoxAudioDevice();
    virtual void Initialize() override;
    virtual void SetOutputVolume(int volume) override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;

private:
    i2c_master_bus_handle_t i2c_master_handle_ = nullptr;

    const audio_codec_data_if_t* data_if_ = nullptr;
    const audio_codec_ctrl_if_t* out_ctrl_if_ = nullptr;
    const audio_codec_if_t* out_codec_if_ = nullptr;
    const audio_codec_ctrl_if_t* in_ctrl_if_ = nullptr;
    const audio_codec_if_t* in_codec_if_ = nullptr;
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;

    esp_codec_dev_handle_t output_dev_ = nullptr;
    esp_codec_dev_handle_t input_dev_ = nullptr;

    void CreateDuplexChannels() override;
    int Read(int16_t* dest, int samples) override;
    int Write(const int16_t* data, int samples) override;
};

#endif // _BOX_AUDIO_DEVICE_H
