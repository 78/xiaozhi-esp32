#pragma once
#include <vector>
#include <functional>

#include <esp_timer.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include "adc_battery_estimation.h"

#define JIUCHUAN_ADC_UNIT (ADC_UNIT_1)
#define JIUCHUAN_ADC_BITWIDTH (ADC_BITWIDTH_12)
#define JIUCHUAN_ADC_ATTEN (ADC_ATTEN_DB_12)
#define JIUCHUAN_ADC_CHANNEL (ADC_CHANNEL_3)
#define JIUCHUAN_RESISTOR_UPPER (200000)
#define JIUCHUAN_RESISTOR_LOWER (100000)

class PowerManager {
private:
    esp_timer_handle_t timer_handle_;
    std::function<void(bool)> on_charging_status_changed_;
    std::function<void(bool)> on_low_battery_status_changed_;

    gpio_num_t charging_pin_ = GPIO_NUM_NC;
    std::vector<uint16_t> adc_values_;
    uint32_t battery_level_ = 0;
    bool is_charging_ = false;
    bool is_low_battery_ = false;
    int ticks_ = 0;
    const int kBatteryAdcInterval = 60;
    const int kBatteryAdcDataCount = 3;
    const int kLowBatteryLevel = 20;

    adc_battery_estimation_handle_t adc_battery_estimation_handle;

    void CheckBatteryStatus() {
        // Get charging status
        bool new_charging_status = gpio_get_level(charging_pin_) == 1;
        if (new_charging_status != is_charging_) {
            is_charging_ = new_charging_status;
            if (on_charging_status_changed_) {
                on_charging_status_changed_(is_charging_);
            }
            ReadBatteryAdcData();
            return;
        }

        // 如果电池电量数据不足，则读取电池电量数据
        if (adc_values_.size() < kBatteryAdcDataCount) {
            ReadBatteryAdcData();
            return;
        }

        // 如果电池电量数据充足，则每 kBatteryAdcInterval 个 tick 读取一次电池电量数据
        ticks_++;
        if (ticks_ % kBatteryAdcInterval == 0) {
            ReadBatteryAdcData();
        }
    }

    void ReadBatteryAdcData() {
        float battery_capacity_temp = 0;
        adc_battery_estimation_get_capacity(adc_battery_estimation_handle, &battery_capacity_temp);
        ESP_LOGI("PowerManager", "Battery level: %.1f%%", battery_capacity_temp);
        battery_level_ = battery_capacity_temp;
    }

public:
    PowerManager(gpio_num_t pin) : charging_pin_(pin) {
        // 初始化充电引脚
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << charging_pin_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; 
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     
        gpio_config(&io_conf);

        // 创建电池电量检查定时器
        esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) {
                PowerManager* self = static_cast<PowerManager*>(arg);
                self->CheckBatteryStatus();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "battery_check_timer",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000));

        // 初始化 ADC
        static const battery_point_t battery_ponint_table[]={
            { 4.2 ,  100},
            { 4.06 ,  80},
            { 3.82 ,  60},
            { 3.58 ,  40},
            { 3.34 ,  20},
            { 3.1 ,  0}
        };

        adc_battery_estimation_t config = {
            .internal = {
                .adc_unit = JIUCHUAN_ADC_UNIT,
                .adc_bitwidth = JIUCHUAN_ADC_BITWIDTH,
                .adc_atten = JIUCHUAN_ADC_ATTEN,
            },
            .adc_channel = JIUCHUAN_ADC_CHANNEL,
            .upper_resistor = JIUCHUAN_RESISTOR_UPPER,
            .lower_resistor = JIUCHUAN_RESISTOR_LOWER,
            .battery_points = battery_ponint_table,
            .battery_points_count = sizeof(battery_ponint_table) / sizeof(battery_ponint_table[0])
        };

        adc_battery_estimation_handle = adc_battery_estimation_create(&config);
    }

    ~PowerManager() {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
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

    uint8_t GetBatteryLevel() {
        return battery_level_;
    }

    void OnLowBatteryStatusChanged(std::function<void(bool)> callback) {
        on_low_battery_status_changed_ = callback;
    }

    void OnChargingStatusChanged(std::function<void(bool)> callback) {
        on_charging_status_changed_ = callback;
    }
};