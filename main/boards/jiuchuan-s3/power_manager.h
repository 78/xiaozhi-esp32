#pragma once
#include <vector>
#include <functional>

#include <esp_timer.h>
#include <driver/gpio.h>
#include "adc_battery_estimation.h"
#include "power_controller.h"
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#define JIUCHUAN_ADC_UNIT (ADC_UNIT_1)
#define JIUCHUAN_ADC_BITWIDTH (ADC_BITWIDTH_12)
#define JIUCHUAN_ADC_ATTEN (ADC_ATTEN_DB_12)
#define JIUCHUAN_ADC_CHANNEL (ADC_CHANNEL_3)
#define JIUCHUAN_RESISTOR_UPPER (200000)
#define JIUCHUAN_RESISTOR_LOWER (100000)

#undef TAG
#define TAG "PowerManager"
class PowerManager {
private:
    esp_timer_handle_t timer_handle_;
    std::function<void(bool)> on_charging_status_changed_;
    std::function<void(bool)> on_low_battery_status_changed_;
    gpio_num_t charging_pin_ = GPIO_NUM_NC;
    std::vector<uint16_t> adc_values_;
    int32_t battery_level_ = 100;
    bool is_charging_ = false;
    bool is_low_battery_ = false;
    bool is_empty_battery_ = false;
    int ticks_ = 0;
    const int kBatteryAdcInterval = 60;
    const int kBatteryAdcDataCount = 3;
    const int kLowBatteryLevel = 20;

    adc_battery_estimation_handle_t adc_battery_estimation_handle;
    PowerController* power_controller_;
    
    

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
        if(battery_capacity_temp > -10 && battery_capacity_temp <= 0){
            battery_level_ = 0;
        }else{
            battery_level_ = battery_capacity_temp;
        }
    }

public:
    PowerManager(gpio_num_t pin) : charging_pin_(pin) {
        power_controller_ = &PowerController::Instance();
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

        static const battery_point_t battery_ponint_table[]={
            { 4.2 ,  100},
            { 4.06 ,  80},
            { 3.82 ,  60},
            { 3.58 ,  40},
            { 3.34 ,  20},
            { 3.1 ,  0},
            { 3.0 ,  -10}
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
        
        RegisterAllCallbacks();
    }

    ~PowerManager() {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }
        if (adc_battery_estimation_handle) {
            adc_battery_estimation_destroy(adc_battery_estimation_handle);
        }
    }

    bool IsCharging() {
        // 如果电量已经满了，则不再显示充电中
        if (battery_level_ == 100) {
            //ESP_LOGI(TAG, "电量已满，不再显示充电中");
            return false;
        }
        return is_charging_;
    }

    bool IsDischarging() {
        // 没有区分充电和放电，所以直接返回相反状态
        return !is_charging_;
    }

    int32_t GetBatteryLevel() {
        return battery_level_;
    }

    void RegisterAllCallbacks() {
        //注册电源状态变更回调函数（优化版）
        power_controller_->OnStateChange([this](PowerState newState) {
            switch(newState) {
                case PowerState::SHUTDOWN: {

                    ESP_LOGD(TAG, "关机");
                    
                //取消 PWR_EN 使能
                    /* 防止关机后误唤醒 */
                    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(PWR_BUTTON_GPIO, 0));
                    ESP_ERROR_CHECK(rtc_gpio_pulldown_en(PWR_BUTTON_GPIO)); // 内部下拉
                    ESP_ERROR_CHECK(rtc_gpio_pullup_dis(PWR_BUTTON_GPIO));
                    /* 关闭电源使能 */
                    rtc_gpio_set_level(PWR_EN_GPIO, 0);
                    rtc_gpio_hold_dis(PWR_EN_GPIO);
                    
                    // 确保所有外设已关闭
                    vTaskDelay(200 / portTICK_PERIOD_MS);
                    ESP_LOGI(TAG, "Initiating deep sleep");

                    esp_deep_sleep_start();
                    break;
                }   
                default:
                    ESP_LOGD(TAG, "State changed to %d", static_cast<int>(newState));
                    break;
            }
        });  
    }
    void SetPowerState(PowerState newState) {
        power_controller_->SetState(newState);
    }

    void OnLowBatteryStatusChanged(std::function<void(bool)> callback) {
        on_low_battery_status_changed_ = callback;
    }

    void OnChargingStatusChanged(std::function<void(bool)> callback) {
        on_charging_status_changed_ = callback;
    }
};