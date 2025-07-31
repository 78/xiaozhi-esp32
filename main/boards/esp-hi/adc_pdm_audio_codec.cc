#include "adc_pdm_audio_codec.h"

#include <esp_log.h>
#include <driver/i2c.h>
#include <driver/i2c_master.h>
#include <driver/i2s_tdm.h>
#include "adc_mic.h"
#include "driver/i2s_pdm.h"
#include "soc/gpio_sig_map.h"
#include "soc/io_mux_reg.h"
#include "hal/rtc_io_hal.h"
#include "hal/gpio_ll.h"
#include "settings.h"

static const char TAG[] = "AdcPdmAudioCodec";

#define BSP_I2S_GPIO_CFG(_dout)       \
    {                          \
        .clk = GPIO_NUM_NC,    \
        .dout = _dout,  \
        .invert_flags = {      \
            .clk_inv = false, \
        },                     \
    }

/**
 * @brief Mono Duplex I2S configuration structure
 *
 * This configuration is used by default in bsp_audio_init()
 */
#define BSP_I2S_DUPLEX_MONO_CFG(_sample_rate, _dout)                                                         \
    {                                                                                                 \
        .clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG(_sample_rate),                                          \
        .slot_cfg = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), \
        .gpio_cfg = BSP_I2S_GPIO_CFG(_dout),                                                                 \
    }

AdcPdmAudioCodec::AdcPdmAudioCodec(int input_sample_rate, int output_sample_rate,
    uint32_t adc_mic_channel, gpio_num_t pdm_speak_p,gpio_num_t pdm_speak_n, gpio_num_t pa_ctl) {

    input_reference_ = false;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;

    uint8_t adc_channel[1] = {0};
    adc_channel[0] = adc_mic_channel;

    audio_codec_adc_cfg_t cfg = {
        .handle = NULL,
        .max_store_buf_size = 1024 * 2,
        .conv_frame_size = 1024,
        .unit_id = ADC_UNIT_1,
        .adc_channel_list = adc_channel,
        .adc_channel_num = sizeof(adc_channel) / sizeof(adc_channel[0]),
        .sample_rate_hz = (uint32_t)input_sample_rate,
    };
    const audio_codec_data_if_t *adc_if = audio_codec_new_adc_data(&cfg);

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .data_if = adc_if,
    };
    input_dev_ = esp_codec_dev_new(&codec_dev_cfg);
    if (!input_dev_) {
        ESP_LOGE(TAG, "Failed to create codec device");
        return;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, NULL));

    i2s_pdm_tx_config_t pdm_cfg_default = BSP_I2S_DUPLEX_MONO_CFG((uint32_t)output_sample_rate, pdm_speak_p);
    pdm_cfg_default.clk_cfg.up_sample_fs = output_sample_rate / 100;
    pdm_cfg_default.slot_cfg.sd_scale = I2S_PDM_SIG_SCALING_MUL_4;
    pdm_cfg_default.slot_cfg.hp_scale = I2S_PDM_SIG_SCALING_MUL_4;
    pdm_cfg_default.slot_cfg.lp_scale = I2S_PDM_SIG_SCALING_MUL_4;
    pdm_cfg_default.slot_cfg.sinc_scale = I2S_PDM_SIG_SCALING_MUL_4;
    const i2s_pdm_tx_config_t *p_i2s_cfg = &pdm_cfg_default;

    ESP_ERROR_CHECK(i2s_channel_init_pdm_tx_mode(tx_handle_, p_i2s_cfg));

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = NULL,
        .tx_handle = tx_handle_,
    };

    const audio_codec_data_if_t *i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);

    codec_dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT;
    codec_dev_cfg.codec_if = NULL;
    codec_dev_cfg.data_if = i2s_data_if;
    output_dev_ = esp_codec_dev_new(&codec_dev_cfg);

    output_volume_ = 100;
    if(pa_ctl != GPIO_NUM_NC) {
        pa_ctrl_pin_ = pa_ctl;
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << pa_ctrl_pin_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
    }
    gpio_set_drive_capability(pdm_speak_p, GPIO_DRIVE_CAP_0);

    if(pdm_speak_n != GPIO_NUM_NC){
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pdm_speak_n], PIN_FUNC_GPIO);
        gpio_set_direction(pdm_speak_n, GPIO_MODE_OUTPUT);
        esp_rom_gpio_connect_out_signal(pdm_speak_n, I2SO_SD_OUT_IDX, 1, 0); //反转输出 SD OUT 信号
        gpio_set_drive_capability(pdm_speak_n, GPIO_DRIVE_CAP_0);
    }
    ESP_LOGI(TAG, "AdcPdmAudioCodec initialized");
}

AdcPdmAudioCodec::~AdcPdmAudioCodec() {
    ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    esp_codec_dev_delete(output_dev_);
    ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    esp_codec_dev_delete(input_dev_);
}

void AdcPdmAudioCodec::SetOutputVolume(int volume) {
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
    AudioCodec::SetOutputVolume(volume);
}

void AdcPdmAudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    if (enable) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
            .sample_rate = (uint32_t)input_sample_rate_,
            .mclk_multiple = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(input_dev_, &fs));
    } else {
        // ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
        return;
    }
    AudioCodec::EnableInput(enable);
}

void AdcPdmAudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    if (enable) {
        // Play 16bit 1 channel
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = 0,
        };
        ESP_ERROR_CHECK(esp_codec_dev_open(output_dev_, &fs));
        ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, output_volume_));
        if(pa_ctrl_pin_ != GPIO_NUM_NC){
            gpio_set_level(pa_ctrl_pin_, 1);
        }

    } else {
        if(pa_ctrl_pin_ != GPIO_NUM_NC){
            gpio_set_level(pa_ctrl_pin_, 0);
        }
        ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    }
    AudioCodec::EnableOutput(enable);
}

int AdcPdmAudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_read(input_dev_, (void*)dest, samples * sizeof(int16_t)));
    }
    return samples;
}
int AdcPdmAudioCodec::Write(const int16_t* data, int samples) {
    if (output_enabled_) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_codec_dev_write(output_dev_, (void*)data, samples * sizeof(int16_t)));
    }
    return samples;
}

void AdcPdmAudioCodec::Start() {
    Settings settings("audio", false);
    output_volume_ = settings.GetInt("output_volume", output_volume_);
    if (output_volume_ <= 0) {
        ESP_LOGW(TAG, "Output volume value (%d) is too small, setting to default (10)", output_volume_);
        output_volume_ = 10;
    }

    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));

    EnableInput(true);
    EnableOutput(true);
    ESP_LOGI(TAG, "Audio codec started");
}
