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
        int adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, ADC_CHANNEL_3, &adc_value));
        
        // 验证ADC值在合理范围内
        if (adc_value < 1200 || adc_value > 1800) {
            ESP_LOGW("PowerManager", "Invalid ADC reading: %d (expected 1200-1800)", adc_value);
            return;
        }
        
        // 将 ADC 值添加到队列中
        adc_values_.push_back(adc_value);
        if (adc_values_.size() > kBatteryAdcDataCount) {
            adc_values_.erase(adc_values_.begin());
        }
        ESP_LOGD("PowerManager", "New ADC reading: %d, queue size: %d", adc_value, adc_values_.size());
        uint32_t average_adc = 0;
        for (auto value : adc_values_) {
            average_adc += value;
        }
        average_adc /= adc_values_.size();


        /*
        电量 (%)	电压 (V)	分压后电压 (V)
        0%	          3.1	        1.033
        20%	          3.34	        1.113
        40%	          3.58	        1.193
        60%	          3.82	        1.273
        80%	          4.06	        1.353
        100%	      4.2	        1.400

        电量 (%)	分压后电压 (V)	ADC值（理论）	实际范围（±5%误差）
        0%	        1.033	        ​1284​​	            1220~1348
        20%	        1.113	        ​1384​​	            1315~1453
        40%	        1.193	        ​1483​​	            1409~1557
        60%	        1.273	        ​1583​​	            1504~1662
        80%	        1.353	        ​1682​​	            1598~1766
        100%	    1.400	        ​1745​​	            1658~1832
        -------------------------------------------------------
        电量 (%)	电压 (V)	分压后电压 (V)
        0%	        3.1	            1.033
        20%	        3.28	        1.093
        40%	        3.46	        1.153
        60%	        3.64	        1.213
        80%	        3.82	        1.273
        100%	    4.1	            1.367

        0%	    1.033	​​1284​​	1220~1348
        20%	    1.093	​​1358​​	1290~1426
        40%	    1.153	​​1431​​	1360~1502
        60%	    1.213	​​1505​​	1430~1580
        80%	    1.273	​​1583​​	1504~1662
        100%	1.367	​​1700​​	1615~1785
        */

        // 定义电池电量区间
        const struct {
            uint16_t adc;
            uint8_t level;
        } levels[] = {
            { 1284 ,  0},
            { 1358 ,  20},
            { 1431 ,  40},
            { 1505 ,  60},
            { 1583 ,  80},
            { 1700 ,  100}
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
                    ESP_LOGD("PowerManager", "Battery level calc: ADC=%u between %u(%u%%) and %u(%u%%) => %u%%",
                            (unsigned int)average_adc, (unsigned int)levels[i].adc, (unsigned int)levels[i].level, 
                            (unsigned int)levels[i+1].adc, (unsigned int)levels[i+1].level, (unsigned int)battery_level_);
                    break;
                }
            }
        }

        // Check low battery status
        if (adc_values_.size() >= kBatteryAdcDataCount) {
            bool new_low_battery_status = battery_level_ <= kLowBatteryLevel;
            if (new_low_battery_status != is_low_battery_) {
                ESP_LOGI("PowerManager", "Low battery status changed: %u -> %u (level: %u%%)", 
                        (unsigned int)is_low_battery_, (unsigned int)new_low_battery_status, (unsigned int)battery_level_);
                
                // 只有在电量确实低于阈值时才触发回调
                if (new_low_battery_status && battery_level_ <= kLowBatteryLevel) {
                    is_low_battery_ = true;
                    if (on_low_battery_status_changed_) {
                        ESP_LOGI("PowerManager", "Triggering low battery callback");
                        on_low_battery_status_changed_(true);
                    }
                } else if (!new_low_battery_status && battery_level_ > kLowBatteryLevel) {
                    is_low_battery_ = false;
                    if (on_low_battery_status_changed_) {
                        ESP_LOGI("PowerManager", "Triggering battery recovered callback");
                        on_low_battery_status_changed_(false);
                    }
                }
            }
        }

        ESP_LOGI("PowerManager", "ADC value: %d average: %ld level: %ld", adc_value, average_adc, battery_level_);
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
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));
        
        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, ADC_CHANNEL_3, &chan_config));
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