#pragma once
#include <vector>

#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>


class PowerManager {
private:
    gpio_num_t charging_pin_ = GPIO_NUM_NC;
    std::vector<uint16_t> adc_values_;
    uint32_t battery_level_ = 0;
    int ticks_ = 0;
    const int kBatteryCheckInterval = 60;
    const int kBatteryAdcDataCount = 3;

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
    }

    uint16_t ReadBatteryAdcData() {
        adc_oneshot_unit_handle_t adc_handle;
        adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        // 初始化 ADC 单元
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        // 配置 ADC 通道
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &chan_config));
        int adc_value;
        // 读取 ADC 值
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &adc_value));
        adc_oneshot_del_unit(adc_handle);
        return adc_value;
    }

    bool IsBatteryLevelSteady() {
        return adc_values_.size() >= kBatteryAdcDataCount;
    }

    uint8_t ReadBatteryLevel(bool update_immediately = false) {
        ticks_++;
        if (!update_immediately && adc_values_.size() >= kBatteryAdcDataCount) {
            // 每隔一段时间检查一次电量
            if (ticks_ % kBatteryCheckInterval != 0) {
                return battery_level_;
            }
        }

        uint16_t adc_value = ReadBatteryAdcData();
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
            {1900, 0},   // 小于1900时为0%
            {2000, 20},  // 1970起点为20%
            {2100, 40},  // 2100为40%
            {2200, 60},  // 2200为60%
            {2300, 80},  // 2300为80%
            {2400, 100}  // 2400及以上为100%
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
        return battery_level_;
    }

    bool IsCharging() {
        int charging_level = gpio_get_level(charging_pin_);
        return (charging_level == 1);
    }
};
    