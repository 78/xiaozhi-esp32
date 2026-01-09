#pragma once
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_timer.h>

class PowerManager {
  private:
    static constexpr uint32_t BATTERY_LEVEL_MIN = 2048;
    static constexpr uint32_t BATTERY_LEVEL_MAX = 2330;
    static constexpr size_t ADC_VALUES_COUNT = 10;
    static constexpr size_t CHARGING_COUNT = 5;

    esp_timer_handle_t timer_handle_ = nullptr;
    gpio_num_t adc_pin_ = GPIO_NUM_NC;
    uint16_t adc_values_[ADC_VALUES_COUNT];
    size_t adc_values_index_ = 0;
    size_t adc_values_count_ = 0;
    uint8_t battery_level_ = 100;
    gpio_num_t charging_pin_ = GPIO_NUM_NC;
    bool is_charging_ = false;
    int unchanging_count = 0;

    adc_oneshot_unit_handle_t adc_handle_;
    adc_channel_t adc_channel_;

    void CheckBatteryStatus() {
        if (charging_pin_ != GPIO_NUM_NC) {
            bool new_charging_status = gpio_get_level(charging_pin_) == 1;
            if (new_charging_status) {
                unchanging_count = 0;
            } else {
                unchanging_count++;
                if (is_charging_ && unchanging_count < 5) {
                    new_charging_status = true;
                }
            }
            is_charging_ = new_charging_status;
        }

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

        ESP_LOGI("PowerManager", "ADC值: %d 平均值: %ld 电量: %u%%", adc_value, average_adc, battery_level_);
    }

    void CalculateBatteryLevel(uint32_t average_adc) {
        if (average_adc <= BATTERY_LEVEL_MIN) {
            battery_level_ = 0;
        } else if (average_adc >= BATTERY_LEVEL_MAX) {
            battery_level_ = 100;
        } else {
            float ratio = static_cast<float>(average_adc - BATTERY_LEVEL_MIN) / (BATTERY_LEVEL_MAX - BATTERY_LEVEL_MIN);
            battery_level_ = ratio * 100;
        }
    }

  public:
    PowerManager(gpio_num_t adc_pin_, gpio_num_t charging_pin) : adc_pin_(adc_pin_), charging_pin_(charging_pin) {
        // 初始化充电引脚
        if (charging_pin_ != GPIO_NUM_NC) {
            gpio_config_t io_conf = {};
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pin_bit_mask = (1ULL << charging_pin_);
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            gpio_config(&io_conf);
        }

        esp_timer_create_args_t timer_args = {
            .callback =
                [](void *arg) {
                    PowerManager *self = static_cast<PowerManager *>(arg);
                    self->CheckBatteryStatus();
                },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "battery_check_timer",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000)); // 1秒

        InitializeAdc();
    }

    void InitializeAdc() {
        adc_unit_t adc_unit;
        ESP_ERROR_CHECK(adc_oneshot_io_to_channel(adc_pin_, &adc_unit, &adc_channel_));

        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = adc_unit,
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

    bool IsCharging() {
        // 如果电量已经满了，则不再显示充电中
        if (battery_level_ == 100) {
            return false;
        }
        return is_charging_;
    }

    bool IsDischarging() {
        // 没有区分充电和放电，所以直接返回相反状态
        return !is_charging_;
    }

    uint8_t GetBatteryLevel() { return battery_level_; }
};
