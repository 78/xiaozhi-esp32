#include "boards/ml307_board.h"
#include "audio_codecs/box_audio_codec.h"
#include "display/ssd1306_display.h"
#include "application.h"
#include "button.h"
#include "led.h"
#include "config.h"

#include <esp_log.h>
#include <esp_spiffs.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

static const char *TAG = "KevinBoxBoard";

class KevinBoxBoard : public Ml307Board {
private:
    adc_oneshot_unit_handle_t adc1_handle_;
    adc_cali_handle_t adc1_cali_handle_;
    i2c_master_bus_handle_t display_i2c_bus_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;

    void MountStorage() {
        // Mount the storage partition
        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/storage",
            .partition_label = "storage",
            .max_files = 5,
            .format_if_mount_failed = true,
        };
        esp_vfs_spiffs_register(&conf);
    }

    void Enable4GModule() {
        // Make GPIO15 HIGH to enable the 4G module
        gpio_config_t ml307_enable_config = {
            .pin_bit_mask = (1ULL << 15) | (1ULL << 18),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&ml307_enable_config);
        gpio_set_level(GPIO_NUM_15, 1);
        gpio_set_level(GPIO_NUM_18, 1);
    }

    void InitializeADC() {
        adc_oneshot_unit_init_cfg_t init_config1 = {};
        init_config1.unit_id = ADC_UNIT_1;
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle_));

        //-------------ADC1 Config---------------//
        adc_oneshot_chan_cfg_t config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle_, ADC_CHANNEL_0, &config));

        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .chan = ADC_CHANNEL_0,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle_));
    }

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            Application::GetInstance().ToggleChatState();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("Volume\n" + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            auto codec = GetAudioCodec();
            codec->SetOutputVolume(100);
            GetDisplay()->ShowNotification("Volume\n100");
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification("Volume\n" + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            auto codec = GetAudioCodec();
            codec->SetOutputVolume(0);
            GetDisplay()->ShowNotification("Volume\n0");
        });
    }

public:
    KevinBoxBoard() : Ml307Board(ML307_TX_PIN, ML307_RX_PIN, 4096),
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
    }

    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing KevinBoxBoard");
        InitializeDisplayI2c();
        InitializeCodecI2c();
        InitializeADC();
        MountStorage();
        Enable4GModule();

        gpio_config_t charging_io = {
            .pin_bit_mask = (1ULL << 2),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&charging_io);

        InitializeButtons();

        Ml307Board::Initialize();
    }

    virtual Led* GetBuiltinLed() override {
        static Led led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(codec_i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        static Ssd1306Display display(display_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        return &display;
    }

    virtual bool GetBatteryVoltage(int &voltage, bool& charging) override {
        ESP_ERROR_CHECK(adc_oneshot_get_calibrated_result(adc1_handle_, adc1_cali_handle_, ADC_CHANNEL_0, &voltage));
        voltage *= 3;
        charging = gpio_get_level(GPIO_NUM_2) == 0;
        ESP_LOGI(TAG, "Battery voltage: %d, Charging: %d", voltage, charging);
        return true;
    }
};

DECLARE_BOARD(KevinBoxBoard);