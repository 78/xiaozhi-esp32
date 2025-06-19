#pragma once
#include <vector>
#include <functional>

#include <esp_timer.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>


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

    adc_oneshot_unit_handle_t adc_handle_;

    void CheckBatteryStatus() {
        // 使用GPIO_NUM_5检测充电状态（CHSTA信号）
        int gpio_level = gpio_get_level(charging_pin_);
        
        // 添加详细调试日志
        ESP_LOGI("PowerManager", "GPIO[5] (CHSTA) 充电状态检测: %d", gpio_level);
        
        // GPIO_NUM_5的CHSTA信号：低电平表示正在充电，高电平表示未充电
        bool new_charging_status = gpio_level == 0;  // 低电平表示充电中
        
        if (new_charging_status != is_charging_) {
            ESP_LOGI("PowerManager", "充电状态发生变化: %s -> %s", 
                    is_charging_ ? "充电中" : "未充电", 
                    new_charging_status ? "充电中" : "未充电");
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
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, ADC_CHANNEL_0, &adc_value));
        
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
            {1970, 0},
            {2062, 20},
            {2154, 40},
            {2246, 60},
            {2338, 80},
            {2430, 100}
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
        // 初始化充电状态检测引脚
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pin_bit_mask = (1ULL << charging_pin_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; 
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;     // 启用内部上拉，解决浮空问题
        gpio_config(&io_conf);
        
        ESP_LOGI("PowerManager", "初始化充电状态检测引脚 GPIO[%d] (CHSTA)，启用内部上拉电阻", charging_pin_);

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
        
        // 初始化后立即读取GPIO状态用于调试
        vTaskDelay(pdMS_TO_TICKS(100));  // 等待GPIO稳定
        int gpio_initial = gpio_get_level(charging_pin_);
        ESP_LOGI("PowerManager", "初始充电状态 - GPIO[%d] (CHSTA): %d", charging_pin_, gpio_initial);
        
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000));

        // 初始化 ADC
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));
        
        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, ADC_CHANNEL_0, &chan_config));
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
        // 直接返回真实的充电状态，不受电量百分比影响
        // 即使电量100%，插上充电器时仍应显示充电图标
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
