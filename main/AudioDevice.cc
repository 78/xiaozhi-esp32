#include "AudioDevice.h"
#include <esp_log.h>
#include <cstring>
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_system.h"
#include "esp_check.h"
#include "es8311.h"
#include "driver/i2c.h"
#include "es7210.h"
#define TAG "AudioDevice"



/* Example configurations */
#define EXAMPLE_RECV_BUF_SIZE (2400)
#define EXAMPLE_SAMPLE_RATE (16000)
#define EXAMPLE_MCLK_MULTIPLE (384) // If not using 24-bit data width, 256 should be enough
#define EXAMPLE_MCLK_FREQ_HZ (EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE)
#define EXAMPLE_VOICE_VOLUME 75


#define ES7210_I2C_ADDR             (0x40)
#define ES7210_SAMPLE_RATE          (48000)
#define ES7210_I2S_FORMAT           ES7210_I2S_FMT_I2S
#define ES7210_MCLK_MULTIPLE        (256)
#define ES7210_BIT_WIDTH            ES7210_I2S_BITS_16B
#define ES7210_MIC_BIAS             ES7210_MIC_BIAS_2V87
#define ES7210_MIC_GAIN             ES7210_MIC_GAIN_30DB
#define ES7210_ADC_VOLUME           (0)


/* I2C port and GPIOs */
#define I2C_NUM I2C_NUM_0

#define I2C_SCL_IO (GPIO_NUM_18)
#define I2C_SDA_IO (GPIO_NUM_17)

/* I2S port and GPIOs */
#define I2S_NUM I2S_NUM_0
#define I2S_MCK_IO (GPIO_NUM_16)
#define I2S_BCK_IO (GPIO_NUM_9)
#define I2S_WS_IO (GPIO_NUM_45)
#define I2S_DO_IO (GPIO_NUM_8)
#define I2S_DI_IO (GPIO_NUM_10)
static i2s_chan_handle_t tx_handle = NULL;
static i2s_chan_handle_t rx_handle = NULL;
static es7210_dev_handle_t es7210_handle = NULL;

static void es7210_init(bool is_tdm)
{
    /* Create ES7210 device handle */
    es7210_i2c_config_t es7210_i2c_conf = {
        .i2c_port = I2C_NUM,
        .i2c_addr = ES7210_I2C_ADDR
    };
    es7210_new_codec(&es7210_i2c_conf, &es7210_handle);

    ESP_LOGI(TAG, "Configure ES7210 codec parameters");
    es7210_codec_config_t codec_conf = {
        .sample_rate_hz = ES7210_SAMPLE_RATE,
        .mclk_ratio = ES7210_MCLK_MULTIPLE,
        .i2s_format = ES7210_I2S_FORMAT,
        .bit_width = ES7210_BIT_WIDTH,
        .mic_bias = ES7210_MIC_BIAS,
        .mic_gain = ES7210_MIC_GAIN,
        .flags = {
            .tdm_enable = 1}};
    es7210_config_codec(es7210_handle, &codec_conf);
    es7210_config_volume(es7210_handle, ES7210_ADC_VOLUME);
}

static esp_err_t es8311_codec_init(void)
{
    /* Initialize I2C peripheral */
    // const i2c_config_t es_i2c_cfg = {
    //     .mode = I2C_MODE_MASTER,
    //     .sda_io_num = I2C_SDA_IO,
    //     .scl_io_num = I2C_SCL_IO,
    //     .sda_pullup_en = GPIO_PULLUP_ENABLE,
    //     .scl_pullup_en = GPIO_PULLUP_ENABLE,
    //     .master = {
    //             .clk_speed = 400000,
    //         }
    // };
    // ESP_RETURN_ON_ERROR(i2c_param_config(I2C_NUM, &es_i2c_cfg), TAG, "config i2c failed");
    // ESP_RETURN_ON_ERROR(i2c_driver_install(I2C_NUM, I2C_MODE_MASTER, 0, 0, 0), TAG, "install i2c driver failed");

    /* Initialize es8311 codec */
    es8311_handle_t es_handle = es8311_create(I2C_NUM, ES8311_ADDRRES_0);
    ESP_RETURN_ON_FALSE(es_handle, ESP_FAIL, TAG, "es8311 create failed");
    const es8311_clock_config_t es_clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = EXAMPLE_MCLK_FREQ_HZ,
        .sample_frequency = EXAMPLE_SAMPLE_RATE};

    ESP_ERROR_CHECK(es8311_init(es_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16));
    ESP_RETURN_ON_ERROR(es8311_sample_frequency_config(es_handle, EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE, EXAMPLE_SAMPLE_RATE), TAG, "set es8311 sample frequency failed");
    ESP_RETURN_ON_ERROR(es8311_voice_volume_set(es_handle, EXAMPLE_VOICE_VOLUME, NULL), TAG, "set es8311 volume failed");
    ESP_RETURN_ON_ERROR(es8311_microphone_config(es_handle, false), TAG, "set es8311 microphone failed");
#if CONFIG_EXAMPLE_MODE_ECHO
    ESP_RETURN_ON_ERROR(es8311_microphone_gain_set(es_handle, EXAMPLE_MIC_GAIN), TAG, "set es8311 microphone gain failed");
#endif
    return ESP_OK;
}

static esp_err_t i2s_driver_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // Auto clear the legacy data in the DMA buffer
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(EXAMPLE_SAMPLE_RATE),
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false
        },
        .gpio_cfg = {
            .mclk = I2S_MCK_IO,
            .bclk = I2S_BCK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DO_IO,
            .din = I2S_DI_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = (i2s_mclk_multiple_t)EXAMPLE_MCLK_MULTIPLE;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    return ESP_OK;
}


AudioDevice::AudioDevice() {
}

AudioDevice::~AudioDevice() {
    if (audio_input_task_ != nullptr) {
        vTaskDelete(audio_input_task_);
    }
    if (rx_handle_ != nullptr) {
        ESP_ERROR_CHECK(i2s_channel_disable(rx_handle_));
    }
    if (tx_handle_ != nullptr) {
        ESP_ERROR_CHECK(i2s_channel_disable(tx_handle_));
    }
}

void AudioDevice::Start(int input_sample_rate, int output_sample_rate) {
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;

// #ifdef CONFIG_AUDIO_DEVICE_I2S_SIMPLEX
//         CreateSimplexChannels();
// #else
//         CreateDuplexChannels();
// #endif

//     ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
//     ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    printf("i2s es8311 codec example start\n-----------------------------\n");
    /* Initialize i2s peripheral */
    if (i2s_driver_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s driver init failed");
        abort();
    }
    else
    {
        ESP_LOGI(TAG, "i2s driver init success");
    }
    /* Initialize i2c peripheral and config es8311 codec by i2c */
    if (es8311_codec_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "es8311 codec init failed");
        abort();
    }
    else
    {
        ESP_LOGI(TAG, "es8311 codec init success");
    }
    es7210_init(true);
    esp_rom_gpio_pad_select_gpio(GPIO_NUM_48);
    gpio_set_direction(GPIO_NUM_48, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_48, 1); // 输出高电平

    xTaskCreate([](void* arg) {
        auto audio_device = (AudioDevice*)arg;
        audio_device->InputTask();
    }, "audio_input", 1024*10, this, 20, &audio_input_task_);
}

void AudioDevice::CreateDuplexChannels() {
    duplex_ = true;

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = false,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)CONFIG_AUDIO_DEVICE_I2S_MIC_GPIO_BCLK,
            .ws = (gpio_num_t)CONFIG_AUDIO_DEVICE_I2S_MIC_GPIO_WS,
            .dout = (gpio_num_t)CONFIG_AUDIO_DEVICE_I2S_SPK_GPIO_DOUT,
            .din = (gpio_num_t)CONFIG_AUDIO_DEVICE_I2S_MIC_GPIO_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
    ESP_LOGI(TAG, "Duplex channels created");
}

#ifdef CONFIG_AUDIO_DEVICE_I2S_SIMPLEX
void AudioDevice::CreateSimplexChannels() {
    // Create a new channel for speaker
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240,
        .auto_clear_after_cb = false,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, nullptr));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)CONFIG_AUDIO_DEVICE_I2S_SPK_GPIO_BCLK,
            .ws = (gpio_num_t)CONFIG_AUDIO_DEVICE_I2S_SPK_GPIO_WS,
            .dout = (gpio_num_t)CONFIG_AUDIO_DEVICE_I2S_SPK_GPIO_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));

    // Create a new channel for MIC
    chan_cfg.id = I2S_NUM_1;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &rx_handle_));
    std_cfg.clk_cfg.sample_rate_hz = (uint32_t)input_sample_rate_;
    std_cfg.gpio_cfg.bclk = (gpio_num_t)CONFIG_AUDIO_DEVICE_I2S_MIC_GPIO_BCLK;
    std_cfg.gpio_cfg.ws = (gpio_num_t)CONFIG_AUDIO_DEVICE_I2S_MIC_GPIO_WS;
    std_cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.din = (gpio_num_t)CONFIG_AUDIO_DEVICE_I2S_MIC_GPIO_DIN;
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
    ESP_LOGI(TAG, "Simplex channels created");
}
#endif

void AudioDevice::Write(const int16_t* data, int samples) {
    int32_t buffer[samples];
    for (int i = 0; i < samples; i++) {
        buffer[i] = int32_t(data[i]) << 15;
    }

    size_t bytes_written;
    ESP_ERROR_CHECK(i2s_channel_write(tx_handle, buffer, samples * sizeof(int32_t), &bytes_written, portMAX_DELAY));

}

int AudioDevice::Read(int16_t* dest, int samples) {
    size_t bytes_read;

    int32_t bit32_buffer_[samples];
    if (i2s_channel_read(rx_handle, bit32_buffer_, samples * sizeof(int32_t), &bytes_read, portMAX_DELAY) != ESP_OK) {
        ESP_LOGE(TAG, "Read Failed!");
        return 0;
    }

    samples = bytes_read / sizeof(int32_t);
    for (int i = 0; i < samples; i++) {
        int32_t value = bit32_buffer_[i] >> 12;
        dest[i] = (value > INT16_MAX) ? INT16_MAX : (value < -INT16_MAX) ? -INT16_MAX : (int16_t)value;
    }
    return samples;
}

void AudioDevice::OnInputData(std::function<void(const int16_t*, int)> callback) {
    on_input_data_ = callback;
}

void AudioDevice::OutputData(std::vector<int16_t>& data) {
    Write(data.data(), data.size());
}

void AudioDevice::InputTask() {
    int duration = 10;
    int input_frame_size = input_sample_rate_ / 1000 * duration;
    int16_t input_buffer[input_frame_size];


    // int16_t *input_buffer=(int16_t *)heap_caps_malloc(input_frame_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    while (true) {
        int samples = Read(input_buffer, input_frame_size);
        if (samples > 0) {
            on_input_data_(input_buffer, samples);
        }
    }
}
