#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "esp_idf_version.h"
#include "led/circular_strip.h"
#include "sdkconfig.h"

#include "application.h"
#include "button.h"
#include "codecs/es8311_audio_codec.h"
#include "config.h"
#include "sleep_timer.h"
#include "wifi_board.h"

#ifdef IMU_INT_GPIO
#include <esp_sleep.h>

#include "bmi270_api.h"
#include "i2c_bus.h"
#endif  // IMU_INT_GPIO

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define TAG "esp_spot_s3"
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
#define TAG "esp_spot_c5"
#else  // target
#error "Unsupported target"
#endif  // target

#ifdef IMU_INT_GPIO
namespace Bmi270Imu {

static bmi270_handle_t bmi_handle_ = nullptr;

esp_err_t Initialize(i2c_bus_handle_t i2c_bus, uint8_t addr = BMI270_I2C_ADDRESS) {
    if (bmi_handle_) {
        return ESP_OK;
    }

    if (!i2c_bus) {
        ESP_LOGE(TAG, "Invalid I2C bus for BMI270");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = bmi270_sensor_create(i2c_bus, &bmi_handle_, bmi270_config_file,
                                         BMI2_GYRO_CROSS_SENS_ENABLE | BMI2_CRT_RTOSK_ENABLE);
    if (ret != ESP_OK || !bmi_handle_) {
        ESP_LOGE(TAG, "BMI270 create failed: %s", esp_err_to_name(ret));
        return ret == ESP_OK ? ESP_FAIL : ret;
    }
    ESP_LOGI(TAG, "BMI270 initialized");
    return ESP_OK;
}

// Only used for deep sleep wakeup with wrist gesture interrupt
esp_err_t EnableImuIntForWakeup() {
    if (!bmi_handle_) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t sens_list[] = {BMI2_ACCEL, BMI2_WRIST_GESTURE};
    int8_t rslt = bmi270_sensor_enable(sens_list, 2, bmi_handle_);
    if (rslt != BMI2_OK) {
        ESP_LOGE(TAG, "Failed to enable BMI270 sensors: %d", rslt);
        return ESP_FAIL;
    }

    struct bmi2_sens_config config = {.type = BMI2_WRIST_GESTURE};
    rslt = bmi270_get_sensor_config(&config, 1, bmi_handle_);
    if (rslt != BMI2_OK) {
        ESP_LOGE(TAG, "Failed to get wrist gesture config: %d", rslt);
        return ESP_FAIL;
    }
    config.cfg.wrist_gest.wearable_arm = BMI2_ARM_RIGHT;
    rslt = bmi270_set_sensor_config(&config, 1, bmi_handle_);
    if (rslt != BMI2_OK) {
        ESP_LOGE(TAG, "Failed to set wrist gesture config: %d", rslt);
        return ESP_FAIL;
    }

    struct bmi2_int_pin_config pin_config = {};
    pin_config.pin_type = BMI2_INT1;
    pin_config.pin_cfg[0].input_en = BMI2_INT_INPUT_DISABLE;
    pin_config.pin_cfg[0].lvl = BMI2_INT_ACTIVE_HIGH;
    pin_config.pin_cfg[0].od = BMI2_INT_PUSH_PULL;
    pin_config.pin_cfg[0].output_en = BMI2_INT_OUTPUT_ENABLE;
    pin_config.int_latch = BMI2_INT_NON_LATCH;
    rslt = bmi2_set_int_pin_config(&pin_config, bmi_handle_);
    if (rslt != BMI2_OK) {
        ESP_LOGE(TAG, "Failed to set BMI270 INT pin: %d", rslt);
        return ESP_FAIL;
    }

    struct bmi2_sens_int_config int_config = {.type = BMI2_WRIST_GESTURE, .hw_int_pin = BMI2_INT1};
    rslt = bmi270_map_feat_int(&int_config, 1, bmi_handle_);
    if (rslt != BMI2_OK) {
        ESP_LOGE(TAG, "Failed to map BMI270 interrupt: %d", rslt);
        return ESP_FAIL;
    }

    return ESP_OK;
}

}  // namespace Bmi270Imu

#endif  // IMU_INT_GPIO

class EspSpot : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    Button boot_button_;
    Button key_button_;
    adc_oneshot_unit_handle_t adc1_handle_;
    adc_cali_handle_t adc1_cali_handle_;
    bool adc_calibration_lock_ = false;
    bool key_long_pressed_ = false;
    int64_t last_key_press_time = 0;
    SleepTimer* sleep_timer_ = nullptr;
#ifdef IMU_INT_GPIO
    i2c_bus_handle_t shared_i2c_bus_handle_ = nullptr;
    static constexpr int kDeepSleepTimeoutSeconds = 10 * 60; // 10 minutes
    bool imu_ready_ = false;
#endif

#ifdef IMU_INT_GPIO
    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_config_t i2c_bus_cfg = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .sda_pullup_en = true,
            .scl_pullup_en = true,
            .master =
                {
                    .clk_speed = I2C_MASTER_FREQ_HZ,
                },
            .clk_flags = 0,
        };
        shared_i2c_bus_handle_ = i2c_bus_create(I2C_NUM_0, &i2c_bus_cfg);
        if (!shared_i2c_bus_handle_) {
            ESP_LOGE(TAG, "Failed to create shared I2C bus");
            ESP_ERROR_CHECK(ESP_FAIL);
        }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0) && !CONFIG_I2C_BUS_BACKWARD_CONFIG
        i2c_bus_ = i2c_bus_get_internal_bus_handle(shared_i2c_bus_handle_);
#else
#error "ESP-Spot board requires i2c_bus_get_internal_bus_handle() support"
#endif
        if (!i2c_bus_) {
            ESP_LOGE(TAG, "Failed to obtain master bus handle");
            ESP_ERROR_CHECK(ESP_FAIL);
        }

        esp_err_t imu_ret = Bmi270Imu::Initialize(shared_i2c_bus_handle_);
        if (imu_ret != ESP_OK) {
            ESP_LOGW(TAG, "BMI270 initialization failed, deep sleep disabled (%s)", esp_err_to_name(imu_ret));
        } else {
            imu_ready_ = true;
        }
    }
#else
    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }
#endif  // IMU_INT_GPIO

    void InitializeADC() {
        adc_oneshot_unit_init_cfg_t init_config1 = {.unit_id = ADC_UNIT_1};
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle_));

        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN,
            .bitwidth = ADC_WIDTH,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle_, VBAT_ADC_CHANNEL, &chan_config));

#ifdef ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_handle_t handle = nullptr;
        esp_err_t ret = ESP_FAIL;

        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .atten = ADC_ATTEN,
            .bitwidth = ADC_WIDTH,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            adc_calibration_lock_ = true;
            adc1_cali_handle_ = handle;
            ESP_LOGI(TAG, "ADC Curve Fitting calibration succeeded");
        }
#endif  // ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            HandleUserActivity();
            EnterWifiConfigMode();
        });

        key_button_.OnClick([this]() {
            HandleUserActivity();
            auto& app = Application::GetInstance();
            app.ToggleChatState();
            key_long_pressed_ = false;
        });

        key_button_.OnLongPress([this]() {
            HandleUserActivity();
            int64_t now = esp_timer_get_time();
            auto* led = static_cast<CircularStrip*>(this->GetLed());

            if (key_long_pressed_) {
                if ((now - last_key_press_time) < LONG_PRESS_TIMEOUT_US) {
                    ESP_LOGW(TAG, "Key button long pressed the second time within 5s, shutting down...");
                    led->SetSingleColor(0, {0, 0, 0});

                    gpio_hold_dis(MCU_VCC_CTL);
                    gpio_set_level(MCU_VCC_CTL, 0);

                } else {
                    last_key_press_time = now;
                    BlinkGreenFor5s();
                }
                key_long_pressed_ = true;
            } else {
                ESP_LOGW(TAG, "Key button first long press! Waiting second within 5s to shutdown...");
                last_key_press_time = now;
                key_long_pressed_ = true;

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
        gpio_config_t io_pa = {.pin_bit_mask = (1ULL << AUDIO_CODEC_PA_PIN),
                               .mode = GPIO_MODE_OUTPUT,
                               .pull_up_en = GPIO_PULLUP_DISABLE,
                               .pull_down_en = GPIO_PULLDOWN_DISABLE,
                               .intr_type = GPIO_INTR_DISABLE};
        gpio_config(&io_pa);
        gpio_set_level(AUDIO_CODEC_PA_PIN, 0);

        gpio_config_t io_conf_1 = {.pin_bit_mask = (1ULL << MCU_VCC_CTL),
                                   .mode = GPIO_MODE_OUTPUT,
                                   .pull_up_en = GPIO_PULLUP_DISABLE,
                                   .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                   .intr_type = GPIO_INTR_DISABLE};
        gpio_config(&io_conf_1);

        gpio_config_t io_conf_2 = {.pin_bit_mask = (1ULL << PERP_VCC_CTL),
                                   .mode = GPIO_MODE_OUTPUT,
                                   .pull_up_en = GPIO_PULLUP_DISABLE,
                                   .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                   .intr_type = GPIO_INTR_DISABLE};
        gpio_config(&io_conf_2);

#ifdef IMU_INT_GPIO
        gpio_config_t io_conf_imu_int = {
            .pin_bit_mask = (1ULL << IMU_INT_GPIO),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_NEGEDGE,
        };
        gpio_config(&io_conf_imu_int);
        gpio_install_isr_service(0);
#endif  // IMU_INT_GPIO
    }

    void HandleUserActivity() {
        if (sleep_timer_) {
            sleep_timer_->WakeUp();
        }
    }

#ifdef IMU_INT_GPIO
    void InitializePowerSaveTimer() {
        if (!imu_ready_) {
            ESP_LOGW(TAG, "IMU not ready, skip deep sleep timer");
            return;
        }
        if (sleep_timer_) {
            return;
        }
        sleep_timer_ = new SleepTimer(-1, kDeepSleepTimeoutSeconds);
        sleep_timer_->OnEnterDeepSleepMode([this]() { EnterDeepSleep(); });
        sleep_timer_->SetEnabled(true);
        ESP_LOGI(TAG, "Deep sleep timer enabled, timeout=%ds", kDeepSleepTimeoutSeconds);
    }

    void EnterDeepSleep() {
        if (!imu_ready_) {
            ESP_LOGW(TAG, "Skip deep sleep because IMU is not ready");
            return;
        }

        auto* led = static_cast<CircularStrip*>(GetLed());
        if (led) {
            led->SetSingleColor(0, {0, 0, 0});
        }

        if (Bmi270Imu::EnableImuIntForWakeup() != ESP_OK) {
            ESP_LOGE(TAG, "IMU wakeup configuration failed, abort deep sleep");
            return;
        }

        const uint64_t wakeup_mask = (1ULL << KEY_BUTTON_GPIO) | (1ULL << IMU_INT_GPIO);
        ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup(wakeup_mask, ESP_EXT1_WAKEUP_ANY_HIGH));
        ESP_LOGI(TAG, "Entering deep sleep, waiting for key or wrist gesture");
        esp_deep_sleep_start();
    }
#endif  // IMU_INT_GPIO

    void BlinkGreenFor5s() {
        auto* led = static_cast<CircularStrip*>(GetLed());
        if (!led) {
            return;
        }

        led->Blink({50, 25, 0}, 100);

        esp_timer_create_args_t timer_args = {.callback =
                                                  [](void* arg) {
                                                      auto* self = static_cast<EspSpot*>(arg);
                                                      auto* led = static_cast<CircularStrip*>(self->GetLed());
                                                      if (led) {
                                                          led->SetSingleColor(0, {0, 0, 0});
                                                      }
                                                  },
                                              .arg = this,
                                              .dispatch_method = ESP_TIMER_TASK,
                                              .name = "green_blink_timer"};

        esp_timer_handle_t green_blink_timer = nullptr;
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &green_blink_timer));
        ESP_ERROR_CHECK(esp_timer_start_once(green_blink_timer, LONG_PRESS_TIMEOUT_US));
    }

public:
    EspSpot() : boot_button_(BOOT_BUTTON_GPIO), key_button_(KEY_BUTTON_GPIO, true) {
        InitializePowerCtl();
        InitializeADC();
        InitializeI2c();
        InitializeButtons();
#ifdef IMU_INT_GPIO
        InitializePowerSaveTimer();
#endif  // IMU_INT_GPIO
    }

    virtual Led* GetLed() override {
        static CircularStrip led(LED_GPIO, 1);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
                                            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_PA_PIN,
                                            AUDIO_CODEC_ES8311_ADDR, false);
        return &audio_codec;
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        if (sleep_timer_) {
            sleep_timer_->SetEnabled(level == PowerSaveLevel::LOW_POWER);
        }
        WifiBoard::SetPowerSaveLevel(level);
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (!adc1_handle_) {
            InitializeADC();
        }

        int raw_value = 0;
        int voltage = 0;

        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle_, VBAT_ADC_CHANNEL, &raw_value));

        if (adc_calibration_lock_) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle_, raw_value, &voltage));
            voltage = voltage * 3 / 2;  // compensate for voltage divider
            ESP_LOGI(TAG, "Calibrated voltage: %d mV", voltage);
        } else {
            ESP_LOGI(TAG, "Raw ADC value: %d", raw_value);
            voltage = raw_value;
        }

        voltage = voltage < EMPTY_BATTERY_VOLTAGE ? EMPTY_BATTERY_VOLTAGE : voltage;
        voltage = voltage > FULL_BATTERY_VOLTAGE ? FULL_BATTERY_VOLTAGE : voltage;

        // Calculate battery level percentage
        level = (voltage - EMPTY_BATTERY_VOLTAGE) * 100 / (FULL_BATTERY_VOLTAGE - EMPTY_BATTERY_VOLTAGE);

        // ESP-Spot does not support charging detection, so we use MCU_VCC_CTL to determine charging status
        charging = gpio_get_level(MCU_VCC_CTL);
        discharging = !charging;
        ESP_LOGI(TAG, "Battery Level: %d%%, Charging: %s", level, charging ? "Yes" : "No");
        return true;
    }
};

DECLARE_BOARD(EspSpot);
