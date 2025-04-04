#ifndef __POWER_MANAGER_H__
#define __POWER_MANAGER_H__

#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_timer.h>

class PowerManager {
private:
    // 电池电量区间-分压电阻为2个100k
    static constexpr struct {
        uint16_t adc;
        uint8_t level;
    } BATTERY_LEVELS[] = {
        {1861, 0},   // 3.00V
        {2140, 5},   // 3.45V
        {2283, 10},  // 3.68V
        {2320, 20},  // 3.74V
        {2339, 30},  // 3.77V
        {2351, 40},  // 3.79V
        {2370, 50},  // 3.82V
        {2401, 60},  // 3.87V
        {2432, 70},  // 3.92V
        {2469, 80},  // 3.98V
        {2519, 90},  // 4.06V
        {2605, 100}  // 4.20V
    };
    static constexpr size_t BATTERY_LEVELS_COUNT = 12;
    static constexpr size_t ADC_VALUES_COUNT = 10;

    esp_timer_handle_t timer_handle_ = nullptr;
    gpio_num_t charging_pin_;
    adc_unit_t adc_unit_;
    adc_channel_t adc_channel_;
    uint16_t adc_values_[ADC_VALUES_COUNT];
    size_t adc_values_index_ = 0;
    size_t adc_values_count_ = 0;
    uint8_t battery_level_ = 100;
    bool is_charging_ = false;

    adc_oneshot_unit_handle_t adc_handle_;

    void CheckBatteryStatus() {
        is_charging_ = gpio_get_level(charging_pin_) == 0;
        ReadBatteryAdcData();
    }

    void ReadBatteryAdcData() {
        int adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, adc_channel_, &adc_value));

        adc_values_[adc_values_index_] = adc_value;
        adc_values_index_ = (adc_values_index_ + 1) % ADC_VALUES_COUNT;
        if (adc_values_count_ < ADC_VALUES_COUNT) {
            adc_values_count_++;
        }

        uint32_t average_adc = 0;
        for (size_t i = 0; i < adc_values_count_; i++) {
            average_adc += adc_values_[i];
        }
        average_adc /= adc_values_count_;

        CalculateBatteryLevel(average_adc);

        // ESP_LOGI("PowerManager", "ADC值: %d 平均值: %ld 电量: %u%%", adc_value, average_adc,
        //          battery_level_);
    }

    void CalculateBatteryLevel(uint32_t average_adc) {
        if (average_adc < BATTERY_LEVELS[0].adc) {
            battery_level_ = 0;
        } else if (average_adc >= BATTERY_LEVELS[BATTERY_LEVELS_COUNT - 1].adc) {
            battery_level_ = 100;
        } else {
            for (size_t i = 0; i < BATTERY_LEVELS_COUNT - 1; i++) {
                if (average_adc >= BATTERY_LEVELS[i].adc &&
                    average_adc < BATTERY_LEVELS[i + 1].adc) {
                    float ratio = static_cast<float>(average_adc - BATTERY_LEVELS[i].adc) /
                                  (BATTERY_LEVELS[i + 1].adc - BATTERY_LEVELS[i].adc);
                    battery_level_ =
                        BATTERY_LEVELS[i].level +
                        ratio * (BATTERY_LEVELS[i + 1].level - BATTERY_LEVELS[i].level);
                    break;
                }
            }
        }
    }

public:
    PowerManager(gpio_num_t charging_pin, adc_unit_t adc_unit = ADC_UNIT_2,
                 adc_channel_t adc_channel = ADC_CHANNEL_3)
        : charging_pin_(charging_pin), adc_unit_(adc_unit), adc_channel_(adc_channel) {
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << charging_pin_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);

        esp_timer_create_args_t timer_args = {
            .callback =
                [](void* arg) {
                    PowerManager* self = static_cast<PowerManager*>(arg);
                    self->CheckBatteryStatus();
                },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "battery_check_timer",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000));  // 1秒

        InitializeAdc();
    }

    void InitializeAdc() {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = adc_unit_,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));

        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };

        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, adc_channel_, &chan_config));
    }

    ~PowerManager() {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }
        if (adc_handle_) {
            adc_oneshot_del_unit(adc_handle_);
        }
    }

    bool IsCharging() { return is_charging_; }

    uint8_t GetBatteryLevel() { return battery_level_; }
};
#endif  // __POWER_MANAGER_H__