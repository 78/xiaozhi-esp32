#include "wifi_board.h"
#include "audio_codecs/es8311_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "iot/thing_manager.h"
#include "sdkconfig.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include <driver/gpio.h>
#include "esp_timer.h"
#include "led/circular_strip.h"

#define TAG "esp_spot_s3"

bool button_released_ = false;
bool shutdown_ready_ = false;
esp_timer_handle_t shutdown_timer;

class EspSpotS3Bot : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    Button key_button_;
    adc_oneshot_unit_handle_t adc1_handle;
    adc_cali_handle_t adc1_cali_handle;
    bool do_calibration = false;
    bool key_long_pressed = false;
    int64_t last_key_press_time = 0;
    static const int64_t LONG_PRESS_TIMEOUT_US = 5 * 1000000ULL;

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
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
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeADC() {
        adc_oneshot_unit_init_cfg_t init_config1 = {
            .unit_id = ADC_UNIT_1
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN,
            .bitwidth = ADC_WIDTH,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, VBAT_ADC_CHANNEL, &chan_config));

        adc_cali_handle_t handle = NULL;
        esp_err_t ret = ESP_FAIL;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN,
            .bitwidth = ADC_WIDTH,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            do_calibration = true;
            adc1_cali_handle = handle;
            ESP_LOGI(TAG, "ADC Curve Fitting calibration succeeded");
        }
#endif // ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            ResetWifiConfiguration();
        });

        key_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            app.ToggleChatState();
            key_long_pressed = false;
        });

        key_button_.OnLongPress([this]() {
            int64_t now = esp_timer_get_time();
            auto* led = static_cast<CircularStrip*>(this->GetLed());

            if (key_long_pressed) {
                if ((now - last_key_press_time) < LONG_PRESS_TIMEOUT_US) {
                    ESP_LOGW(TAG, "Key button long pressed the second time within 5s, shutting down...");
                    led->SetSingleColor(0, {0, 0, 0});

                    gpio_hold_dis(MCU_VCC_CTL);
                    gpio_set_level(MCU_VCC_CTL, 0);

                } else {
                    last_key_press_time = now;
                    BlinkGreenFor5s();
                }
                key_long_pressed = true;
            } else {
                ESP_LOGW(TAG, "Key button first long press! Waiting second within 5s to shutdown...");
                last_key_press_time = now;
                key_long_pressed = true;

                BlinkGreenFor5s();
            }
        });
    }

    void InitializePowerCtl() {
        InitializeGPIO();

        gpio_set_level(MCU_VCC_CTL, 1);
        gpio_hold_en(MCU_VCC_CTL);

        gpio_set_level(PERP_VCC_CTL, 1);
        gpio_hold_en(PERP_VCC_CTL);
    }

    void InitializeGPIO() {
        gpio_config_t io_pa = {
            .pin_bit_mask = (1ULL << AUDIO_CODEC_PA_PIN),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_pa);
        gpio_set_level(AUDIO_CODEC_PA_PIN, 0);

        gpio_config_t io_conf_1 = {
            .pin_bit_mask = (1ULL << MCU_VCC_CTL),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf_1);

        gpio_config_t io_conf_2 = {
            .pin_bit_mask = (1ULL << PERP_VCC_CTL),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf_2);
    }

    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Battery"));
    }


    void BlinkGreenFor5s() {
        auto* led = static_cast<CircularStrip*>(GetLed());
        if (!led) {
            return;
        }

        led->Blink({50, 25, 0}, 100);

        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                auto* self = static_cast<EspSpotS3Bot*>(arg);
                auto* led = static_cast<CircularStrip*>(self->GetLed());
                if (led) {
                    led->SetSingleColor(0, {0, 0, 0});
                }
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "blinkGreenFor5s_timer"
        };

        esp_timer_handle_t blink_timer = nullptr;
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &blink_timer));
        ESP_ERROR_CHECK(esp_timer_start_once(blink_timer, LONG_PRESS_TIMEOUT_US));
    }

public:
    EspSpotS3Bot() : boot_button_(BOOT_BUTTON_GPIO), key_button_(KEY_BUTTON_GPIO, true) {
        InitializePowerCtl();
        InitializeADC();
        InitializeI2c();
        InitializeButtons();
        InitializeIot();
    }

    virtual Led* GetLed() override {
        static CircularStrip led(LED_PIN, 1);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
         static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR, false);
        return &audio_codec;
    }

    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging) {
        if (!adc1_handle) {
            InitializeADC();
        }

        int raw_value = 0;
        int voltage = 0;

        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, VBAT_ADC_CHANNEL, &raw_value));

        if (do_calibration) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, raw_value, &voltage));
            voltage = voltage * 3 / 2; // compensate for voltage divider
            ESP_LOGI(TAG, "Calibrated voltage: %d mV", voltage);
        } else {
            ESP_LOGI(TAG, "Raw ADC value: %d", raw_value);
            voltage = raw_value;
        }

        voltage = voltage < EMPTY_BATTERY_VOLTAGE ? EMPTY_BATTERY_VOLTAGE : voltage;
        voltage = voltage > FULL_BATTERY_VOLTAGE ? FULL_BATTERY_VOLTAGE : voltage;

        // 计算电量百分比
        level = (voltage - EMPTY_BATTERY_VOLTAGE) * 100 / (FULL_BATTERY_VOLTAGE - EMPTY_BATTERY_VOLTAGE);

        charging = gpio_get_level(MCU_VCC_CTL);
        ESP_LOGI(TAG, "Battery Level: %d%%, Charging: %s", level, charging ? "Yes" : "No");
        return true;
    }
};

DECLARE_BOARD(EspSpotS3Bot);
