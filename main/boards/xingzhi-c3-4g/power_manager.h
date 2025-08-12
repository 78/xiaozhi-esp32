#pragma once
#include <vector>
#include <functional>

#include <esp_timer.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include "sdkconfig.h"
#include "button.h"
#include "board.h"
#include "application.h"
#include <esp_sleep.h>

class PowerManager {
private:
    esp_timer_handle_t timer_handle_;
    esp_timer_handle_t power_timer_handle_;
    std::function<void(bool)> on_charging_status_changed_;
    std::function<void(bool)> on_low_battery_status_changed_;

    gpio_num_t charging_pin_ = GPIO_NUM_NC;
    std::vector<uint16_t> adc_values_;
    uint32_t battery_level_ = 0;
    bool is_charging_ = false;
    bool is_low_battery_ = false;
    int ticks_ = 0;
    adc_oneshot_unit_handle_t adc_handle_;
    const int kBatteryAdcInterval = 60; 
    const int kBatteryAdcDataCount = 3;
    const int kLowBatteryLevel = 20;

    bool pressed = false; // 是否按下按键
    int PowerControl_ticks_ = 0; // 开机时长
    int press_ticks_ = 0; // 按下时的tick
    int press_interval_ticks_ = 0; // 按下到松开的时间间隔
    bool is_first_boot = true; // 新增一个变量用于标记是否为首次开机
    uint8_t PowerDec_level_ = 0; // 电源键电平
    const int power_off_ticks_ = 10; // 按键按下10/5秒后关机;

    void PowrSwitch() {
        PowerDec_level_ = gpio_get_level(Power_Dec);

        if (PowerDec_level_ == 1) {
            is_first_boot = false;
        } 

        if (!is_first_boot) {
            PowerControl_ticks_++;
            if (PowerDec_level_ == 0 && pressed == false) {
                press_ticks_ = PowerControl_ticks_;
                pressed = true;
            }
            if (press_ticks_ != 0 && PowerControl_ticks_ - press_ticks_ == power_off_ticks_) {
                if (timer_handle_) {
                    esp_timer_stop(timer_handle_);
                    esp_timer_delete(timer_handle_);
                }
                ESP_LOGI("powercontrol", "shut down...");
                esp_deep_sleep_start();
            }
            if (PowerDec_level_ == 1 && press_ticks_!= 0) {
                PowerDec_level_ = gpio_get_level(Power_Dec);
                if (PowerDec_level_ == 1) {
                    press_interval_ticks_ = PowerControl_ticks_ - press_ticks_;
                    pressed = false;
                    press_ticks_ = 0;
                }
            }
            if (press_interval_ticks_ != 0) {
                if (press_interval_ticks_ < power_off_ticks_) {
                    ESP_LOGI("powercontrol", "Rebooting...");
                    esp_restart();
                }
            }
        }
    }

    void CheckBatteryStatus() {
        int adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, POWER_USBIN_ADC_CHANNEL, &adc_value));
        bool new_charging_status = (1500 < adc_value && adc_value < 4000 );
        // ESP_LOGE("powercontrol", "ADC value: %d", adc_value);

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
        int adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, POWER_BATTERY_ADC_CHANNEL, &adc_value));

        // 将 ADC 值添加到队列中
        adc_values_.push_back(adc_value);
        if (adc_values_.size() > kBatteryAdcDataCount) {
            adc_values_.erase(adc_values_.begin());
        }
        uint32_t average_adc = 0;
        for (auto value : adc_values_) {
            average_adc += value;
        }
        average_adc /= adc_values_.size();

        // 定义电池电量区间
        const struct {
            uint16_t adc;
            uint8_t level;
        } levels[] = {
            {2360, 0},
            {2440, 20},
            {2572, 40},
            {2678, 60},
            {2784, 80},
            {2880, 100}
            };

        // 低于最低值时
        if (average_adc < levels[0].adc) {
            battery_level_ = 0;
        }
        // 高于最高值时
        else if (average_adc >= levels[5].adc) {
            battery_level_ = 100;
        } else {
            // 线性插值计算中间值
            for (int i = 0; i < 5; i++) {
                if (average_adc >= levels[i].adc && average_adc < levels[i+1].adc) {
                    float ratio = static_cast<float>(average_adc - levels[i].adc) / (levels[i+1].adc - levels[i].adc);
                    battery_level_ = levels[i].level + ratio * (levels[i+1].level - levels[i].level);
                    break;
                }
            }
        }

        // Check low battery status
        if (adc_values_.size() >= kBatteryAdcDataCount) {
            bool new_low_battery_status = battery_level_ <= kLowBatteryLevel;
            if (new_low_battery_status != is_low_battery_) {
                is_low_battery_ = new_low_battery_status;
                if (on_low_battery_status_changed_) {
                    on_low_battery_status_changed_(is_low_battery_);
                }
            }
        }
        ESP_LOGI("PowerManager", "ADC value: %d average: %ld level: %ld", adc_value, average_adc, battery_level_);
    }

public:
    PowerManager(gpio_num_t pin) : charging_pin_(pin) {
        gpio_config_t powerdecgpio_conf = {};
        powerdecgpio_conf.intr_type = GPIO_INTR_DISABLE;
        powerdecgpio_conf.mode = GPIO_MODE_INPUT;
        powerdecgpio_conf.pin_bit_mask = (1ULL << Power_Dec);
        powerdecgpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; 
        powerdecgpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;     
        gpio_config(&powerdecgpio_conf); // 初始化电源键引脚

        // 初始化电源控制引脚
        gpio_config_t powercontgpio_conf = {};
        powercontgpio_conf.intr_type = GPIO_INTR_DISABLE;
        powercontgpio_conf.mode = GPIO_MODE_OUTPUT;
        powercontgpio_conf.pin_bit_mask = (1ULL << Power_Control); 
        powercontgpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE; 
        powercontgpio_conf.pull_up_en = GPIO_PULLUP_DISABLE;     
        gpio_config(&powercontgpio_conf);
        vTaskDelay(pdMS_TO_TICKS(5));
        gpio_set_level(Power_Control, 1);
        ESP_LOGI("powercontrol", "turnded on ...");

        // 创建电源控制检查定时器
        esp_timer_create_args_t power_timer_args = {
            .callback = [](void* arg) {
                PowerManager* self = static_cast<PowerManager*>(arg);
                self->PowrSwitch();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "power_cotrol_timer",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&power_timer_args, &power_timer_handle_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(power_timer_handle_, 200000));

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

        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = POWER_CBS_ADC_UNIT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));

        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, POWER_BATTERY_ADC_CHANNEL, &chan_config)); // 电池电量检测引脚 GPIO0
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, POWER_USBIN_ADC_CHANNEL, &chan_config)); // usb是否插入引脚 GPIO1
    }

    ~PowerManager() {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }
        if (power_timer_handle_) {
            esp_timer_stop(power_timer_handle_);
            esp_timer_delete(power_timer_handle_);
        }
        if (adc_handle_ != nullptr) {
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
