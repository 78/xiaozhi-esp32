#include "Ml307Board.h"
#include "BoxAudioDevice.h"

#include <esp_log.h>
#include <esp_spiffs.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>

static const char *TAG = "KevinBoxBoard";

class KevinBoxBoard : public Ml307Board {
private:
    adc_oneshot_unit_handle_t adc1_handle_;
    adc_cali_handle_t adc1_cali_handle_;

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

    virtual void InitializeADC() {
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
public:
    virtual void Initialize() override {
        ESP_LOGI(TAG, "Initializing KevinBoxBoard");
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

        Ml307Board::Initialize();
    }

    virtual AudioDevice* CreateAudioDevice() override {
        return new BoxAudioDevice();
    }

    virtual bool GetBatteryVoltage(int &voltage, bool& charging) override {
        ESP_ERROR_CHECK(adc_oneshot_get_calibrated_result(adc1_handle_, adc1_cali_handle_, ADC_CHANNEL_0, &voltage));
        charging = gpio_get_level(GPIO_NUM_2) == 0;
        ESP_LOGI(TAG, "Battery voltage: %d, Charging: %d", voltage, charging);
        return true;
    }
};

DECLARE_BOARD(KevinBoxBoard);
