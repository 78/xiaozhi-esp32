#pragma once
#include <vector>
#include <functional>

#include <esp_timer.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>


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
    bool adc_handle_owned_ = false;  // 标记ADC句柄是否由本类创建
    adc_cali_handle_t adc_cali_handle_ = nullptr;  // ADC校准句柄

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
        int adc_raw = 0;
        int voltage_mv = 0;  // ADC校准后的电压（mV）
        
        // 多次采样取平均，提高稳定性
        uint32_t adc_sum = 0;
        const int sample_count = 10;
        for (int i = 0; i < sample_count; i++) {
            int temp_raw = 0;
            ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, ADC_CHANNEL_5, &temp_raw));
            adc_sum += temp_raw;
            vTaskDelay(pdMS_TO_TICKS(10));  // 每次采样间隔10ms
        }
        adc_raw = adc_sum / sample_count;
        
        // 使用ADC校准获取准确电压
        if (adc_cali_handle_) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle_, adc_raw, &voltage_mv));
        } else {
            // 如果没有校准，使用线性计算
            voltage_mv = (int)(adc_raw * 3300.0f / 4095.0f);
        }
        
        // 根据分压比计算实际电池电压
        // 电路分压比: R21/(R20+R21) = 100K/300K = 1/3
        // 实际电池电压 = ADC测量电压 × 3
        int battery_voltage_mv = voltage_mv * 3;
        
        // 将电压值添加到队列中用于平滑
        adc_values_.push_back(battery_voltage_mv);
        if (adc_values_.size() > kBatteryAdcDataCount) {
            adc_values_.erase(adc_values_.begin());
        }
        
        uint32_t average_voltage = 0;
        for (auto value : adc_values_) {
            average_voltage += value;
        }
        average_voltage /= adc_values_.size();

        // 定义电池电量区间（基于实际电池电压，单位mV）
        const struct {
            uint16_t voltage_mv;  // 电池电压（mV）
            uint8_t level;        // 电量百分比
        } levels[] = {
            {3500, 0},    // 3.5V
            {3640, 20},   // 3.64V
            {3760, 40},   // 3.76V
            {3880, 60},   // 3.88V
            {4000, 80},   // 4.0V
            {4200, 100}   // 4.2V
        };

        // 低于最低值时
        if (average_voltage < levels[0].voltage_mv) {
            battery_level_ = 0;
        }
        // 高于最高值时
        else if (average_voltage >= levels[5].voltage_mv) {
            battery_level_ = 100;
        } else {
            // 线性插值计算中间值
            for (int i = 0; i < 5; i++) {
                if (average_voltage >= levels[i].voltage_mv && average_voltage < levels[i+1].voltage_mv) {
                    float ratio = static_cast<float>(average_voltage - levels[i].voltage_mv) / 
                                  (levels[i+1].voltage_mv - levels[i].voltage_mv);
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

        ESP_LOGI("PowerManager", "ADC raw: %d, ADC voltage: %dmV, Battery: %ldmV (%.2fV), level: %ld%%", 
                 adc_raw, voltage_mv, average_voltage, average_voltage/1000.0f, battery_level_);
    }

public:
    // 构造函数：使用外部ADC句柄（用于复用已存在的ADC）
    PowerManager(gpio_num_t pin, adc_oneshot_unit_handle_t* external_adc_handle = nullptr) 
        : charging_pin_(pin), adc_handle_owned_(false) {
        if(charging_pin_ != GPIO_NUM_NC){
            // 初始化充电引脚
            gpio_config_t io_conf = {};
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pin_bit_mask = (1ULL << charging_pin_);
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; 
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     
            gpio_config(&io_conf);
        }
        
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

        // 初始化或复用 ADC
        if (external_adc_handle != nullptr && *external_adc_handle != nullptr) {
            // 复用外部ADC句柄
            adc_handle_ = *external_adc_handle;
            adc_handle_owned_ = false;
        } else {
            // 创建新的ADC句柄
            adc_oneshot_unit_init_cfg_t init_config = {
                .unit_id = ADC_UNIT_1,  // GPIO6 对应 ADC1
                .ulp_mode = ADC_ULP_MODE_DISABLE,
            };
            ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));
            adc_handle_owned_ = true;
        }
        
        // 配置ADC通道
        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, ADC_CHANNEL_5, &chan_config));  // GPIO6 = ADC1_CHANNEL_5
        
        // 初始化ADC校准
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = ADC_UNIT_1,
            .chan = ADC_CHANNEL_5,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle_);
        if (ret == ESP_OK) {
            ESP_LOGI("PowerManager", "ADC calibration initialized successfully");
        } else {
            ESP_LOGW("PowerManager", "ADC calibration failed, using linear calculation");
            adc_cali_handle_ = nullptr;
        }
    }

    ~PowerManager() {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }
        // 删除ADC校准句柄
        if (adc_cali_handle_) {
            adc_cali_delete_scheme_curve_fitting(adc_cali_handle_);
        }
        // 只有当ADC句柄是本类创建的时候才删除
        if (adc_handle_ && adc_handle_owned_) {
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
