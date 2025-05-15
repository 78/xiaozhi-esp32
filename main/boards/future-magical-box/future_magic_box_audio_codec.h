#ifndef COSTOM_AUDIO_CODEC_H
#define COSTOM_AUDIO_CODEC_H

#include "audio_codecs/box_audio_codec.h"
#include "esp_io_expander.h"
#include "config.h"

class MagicBoxAudioCodec : public BoxAudioCodec
{
private:
    bool is_exp_gpio_ = false; // 标记是否为扩展GPIO
    int pa_pin_;               // 扩展GPIO引脚存储
    esp_io_expander_handle_t io_expander_;
    bool IsExpansionGPIO(int pin);

public:
MagicBoxAudioCodec(void *i2c_master_handle, esp_io_expander_handle_t io_expander, int input_sample_rate, int output_sample_rate,
                     gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
                     int pa_pin, uint8_t es8311_addr, uint8_t es7210_addr, bool input_reference);
    virtual ~MagicBoxAudioCodec();

    virtual void EnableOutput(bool enable) override;
};

#endif // COSTOM_AUDIO_CODEC_H
