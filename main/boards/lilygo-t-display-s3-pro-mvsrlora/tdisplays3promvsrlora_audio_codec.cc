#include "tdisplays3promvsrlora_audio_codec.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/i2s_tdm.h>
#include <driver/i2s_pdm.h>

#include "config.h"

static const char TAG[] = "Tdisplays3promvsrloraAudioCodec";

Tdisplays3promvsrloraAudioCodec::Tdisplays3promvsrloraAudioCodec(int input_sample_rate, int output_sample_rate,
    gpio_num_t mic_bclk, gpio_num_t mic_ws, gpio_num_t mic_data,
    gpio_num_t spkr_bclk, gpio_num_t spkr_lrclk, gpio_num_t spkr_data,
    bool input_reference) {
    duplex_ = true;                             // 是否双工
    input_reference_ = input_reference;         // 是否使用参考输入，实现回声消除
    input_channels_ = input_reference_ ? 2 : 1; // 输入通道数
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;

    CreateVoiceHardware(mic_bclk, mic_ws, mic_data, spkr_bclk, spkr_lrclk, spkr_data);

    gpio_config_t config_mic_en;
    config_mic_en.pin_bit_mask = BIT64(AUDIO_MIC_ENABLE);
    config_mic_en.mode = GPIO_MODE_OUTPUT;
    config_mic_en.pull_up_en = GPIO_PULLUP_ENABLE;
    config_mic_en.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config_mic_en.intr_type = GPIO_INTR_DISABLE;
#if SOC_GPIO_SUPPORT_PIN_HYS_FILTER
config_mic_en.hys_ctrl_mode = GPIO_HYS_SOFT_ENABLE;
#endif
    gpio_config(&config_mic_en);
    gpio_set_level(AUDIO_MIC_ENABLE, 1);

    gpio_config_t config_spkr_en;
    config_spkr_en.pin_bit_mask = BIT64(AUDIO_SPKR_ENABLE);
    config_spkr_en.mode = GPIO_MODE_OUTPUT;
    config_spkr_en.pull_up_en = GPIO_PULLUP_DISABLE;
    config_spkr_en.pull_down_en = GPIO_PULLDOWN_ENABLE;
    config_spkr_en.intr_type = GPIO_INTR_DISABLE;
#if SOC_GPIO_SUPPORT_PIN_HYS_FILTER
config_spkr_en.hys_ctrl_mode = GPIO_HYS_SOFT_ENABLE;
#endif
    gpio_config(&config_spkr_en);
    gpio_set_level(AUDIO_SPKR_ENABLE, 0);

    ESP_LOGI(TAG, "Tdisplays3promvsrloraAudioCodec initialized");
}

Tdisplays3promvsrloraAudioCodec::~Tdisplays3promvsrloraAudioCodec() {
    audio_codec_delete_codec_if(in_codec_if_);
    audio_codec_delete_ctrl_if(in_ctrl_if_);
    audio_codec_delete_codec_if(out_codec_if_);
    audio_codec_delete_ctrl_if(out_ctrl_if_);
    audio_codec_delete_gpio_if(gpio_if_);
    audio_codec_delete_data_if(data_if_);
}

void Tdisplays3promvsrloraAudioCodec::CreateVoiceHardware(gpio_num_t mic_bclk, gpio_num_t mic_ws, gpio_num_t mic_data,
    gpio_num_t spkr_bclk, gpio_num_t spkr_lrclk, gpio_num_t spkr_data) {
    
    i2s_chan_config_t mic_chan_config = I2S_CHANNEL_DEFAULT_CONFIG(i2s_port_t(0), I2S_ROLE_MASTER);
    mic_chan_config.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    i2s_chan_config_t spkr_chan_config = I2S_CHANNEL_DEFAULT_CONFIG(i2s_port_t(1), I2S_ROLE_MASTER);
    spkr_chan_config.auto_clear = true; // Auto clear the legacy data in the DMA buffer

    ESP_ERROR_CHECK(i2s_new_channel(&mic_chan_config, NULL, &rx_handle_));
    ESP_ERROR_CHECK(i2s_new_channel(&spkr_chan_config, &tx_handle_, NULL));

    i2s_pdm_rx_config_t mic_config = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(input_sample_rate_)),
        /* The data bit-width of PDM mode is fixed to 16 */
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .clk = mic_ws,
            .din = mic_data,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };

    i2s_std_config_t spkr_config = {
        .clk_cfg ={
            .sample_rate_hz = static_cast<uint32_t>(11025),
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            #ifdef   I2S_HW_VERSION_2    
                .ext_clk_freq_hz = 0,
            #endif
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg ={
            .mclk = I2S_GPIO_UNUSED,
            .bclk = spkr_bclk,
            .ws = spkr_lrclk,
            .dout = spkr_data,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle_, &mic_config));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &spkr_config));
    ESP_LOGI(TAG, "Voice hardware created");
}

void Tdisplays3promvsrloraAudioCodec::SetOutputVolume(int volume) {
    volume_ = volume;
    AudioCodec::SetOutputVolume(volume);
}

void Tdisplays3promvsrloraAudioCodec::EnableInput(bool enable) {
    gpio_set_level(AUDIO_MIC_ENABLE, !enable);
    AudioCodec::EnableInput(enable);
}

void Tdisplays3promvsrloraAudioCodec::EnableOutput(bool enable) {
    gpio_set_level(AUDIO_SPKR_ENABLE, enable);
    AudioCodec::EnableOutput(enable);
}

int Tdisplays3promvsrloraAudioCodec::Read(int16_t *dest, int samples){
    if (input_enabled_){
        size_t bytes_read;
        i2s_channel_read(rx_handle_, dest, samples * sizeof(int16_t), &bytes_read, portMAX_DELAY);

        // ESP_LOGI(TAG, "Left: %d\n", dest[0]);
        // ESP_LOGI(TAG,"Right: %d\n", dest[1]);
    }
    return samples;
}

int Tdisplays3promvsrloraAudioCodec::Write(const int16_t *data, int samples){
    if (output_enabled_){
        size_t bytes_read;
        auto output_data = (int16_t *)malloc(samples * sizeof(int16_t));
        for (size_t i = 0; i < samples; i++){
            output_data[i] = (float)data[i] * (float)(volume_ / 100.0);
        }
        i2s_channel_write(tx_handle_, output_data, samples * sizeof(int16_t), &bytes_read, portMAX_DELAY);
        free(output_data);
    }
    return samples;
}
