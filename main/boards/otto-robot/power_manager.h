#ifndef __POWER_MANAGER_H__
#define __POWER_MANAGER_H__

#include <driver/gpio.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>
#include <esp_timer.h>

#if CONFIG_IDF_TARGET_ESP32S3
#include <driver/usb_serial_jtag.h>
#endif

class PowerManager {
private:
    // 电池分压为两个 100k 电阻，电池电压 = ADC 引脚电压 x 2
    static constexpr float VOLTAGE_DIVIDER_RATIO = 2.0f;

    // 锂电池 OCV-SOC 曲线（mV -> %），比原来的两点线性映射更接近真实放电曲线
    static constexpr struct {
        uint32_t millivolts;
        uint8_t level;
    } BATTERY_LEVELS[] = {
        {3300, 0}, {3500, 15}, {3700, 40}, {3850, 60}, {4000, 80}, {4200, 100},
    };
    static constexpr size_t BATTERY_LEVELS_COUNT = 6;

    // 10 秒慢平均作为充电检测基准，最近 2 个样本作为快平均
    static constexpr size_t ADC_VALUES_COUNT = 10;
    static constexpr size_t FAST_AVG_COUNT = 2;

    // 充电/拔出判定阈值与确认时间。充电器插入/拔出时电池端电压会有明显台阶，
    // 连续 2 个采样周期（约 2 秒）确认一次，兼顾实时性和抗抖动。
    static constexpr int CHARGE_DELTA_MV = 25;
    static constexpr int DISCHARGE_DELTA_MV = 25;
    static constexpr int CHARGE_CONFIRM_TICKS = 2;
    static constexpr int DISCHARGE_CONFIRM_TICKS = 2;
    static constexpr int RESUME_GRACE_TICKS = 3;

    esp_timer_handle_t timer_handle_ = nullptr;
    gpio_num_t charging_pin_;
    adc_unit_t adc_unit_;
    adc_channel_t adc_channel_;

    // 保存电池电压（mV），不是原始 ADC 值
    uint32_t adc_values_[ADC_VALUES_COUNT] = {};
    size_t adc_values_index_ = 0;
    size_t adc_values_count_ = 0;

    uint8_t battery_level_ = 100;
    bool is_charging_ = false;
    bool usb_was_connected_ = false;
    int charge_confirm_ticks_ = 0;
    int discharge_confirm_ticks_ = 0;
    int log_tick_ = 0;

    inline static bool battery_update_paused_ = false;
    inline static int resume_grace_ticks_ = 0;

    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    adc_cali_handle_t adc_cali_handle_ = nullptr;

    uint32_t RawAdcToBatteryMillivolts(int raw_adc) {
        int calibrated_mv = 0;
        if (adc_cali_handle_ != nullptr &&
            adc_cali_raw_to_voltage(adc_cali_handle_, raw_adc, &calibrated_mv) == ESP_OK) {
            return static_cast<uint32_t>(calibrated_mv * VOLTAGE_DIVIDER_RATIO);
        }
        // 校准不可用时退化为按 3.3V 满量程估算
        return static_cast<uint32_t>((static_cast<float>(raw_adc) * 3300.0f / 4095.0f) *
                                     VOLTAGE_DIVIDER_RATIO);
    }

    uint32_t AverageMillivolts(size_t count) {
        if (count > adc_values_count_) {
            count = adc_values_count_;
        }
        if (count == 0) {
            return 0;
        }
        uint64_t sum = 0;
        for (size_t i = 0; i < count; i++) {
            size_t index = (adc_values_index_ + ADC_VALUES_COUNT - 1 - i) % ADC_VALUES_COUNT;
            sum += adc_values_[index];
        }
        return static_cast<uint32_t>(sum / count);
    }

    uint32_t FastAverageMillivolts() { return AverageMillivolts(FAST_AVG_COUNT); }

    uint32_t SlowAverageMillivolts() { return AverageMillivolts(adc_values_count_); }

    bool UsbHostConnected() {
#if CONFIG_IDF_TARGET_ESP32S3
        return usb_serial_jtag_is_connected();
#else
        return false;
#endif
    }

    void SetCharging(bool charging) {
        if (is_charging_ == charging) {
            return;
        }
        is_charging_ = charging;
        ESP_LOGI("PowerManager", "充电状态: %s", charging ? "CHARGING" : "DISCHARGING");
    }

    void CalculateBatteryLevel(uint32_t millivolts) {
        if (millivolts == 0) {
            return;
        }
        if (millivolts <= BATTERY_LEVELS[0].millivolts) {
            battery_level_ = 0;
            return;
        }
        if (millivolts >= BATTERY_LEVELS[BATTERY_LEVELS_COUNT - 1].millivolts) {
            battery_level_ = 100;
            return;
        }
        for (size_t i = 0; i < BATTERY_LEVELS_COUNT - 1; i++) {
            if (millivolts >= BATTERY_LEVELS[i].millivolts &&
                millivolts < BATTERY_LEVELS[i + 1].millivolts) {
                float ratio = static_cast<float>(millivolts - BATTERY_LEVELS[i].millivolts) /
                              (BATTERY_LEVELS[i + 1].millivolts - BATTERY_LEVELS[i].millivolts);
                battery_level_ = BATTERY_LEVELS[i].level +
                                 ratio * (BATTERY_LEVELS[i + 1].level - BATTERY_LEVELS[i].level);
                return;
            }
        }
    }

    void ReadBatteryAdcData() {
        int raw_adc = 0;
        if (adc_oneshot_read(adc_handle_, adc_channel_, &raw_adc) != ESP_OK) {
            return;
        }
        adc_values_[adc_values_index_] = RawAdcToBatteryMillivolts(raw_adc);
        adc_values_index_ = (adc_values_index_ + 1) % ADC_VALUES_COUNT;
        if (adc_values_count_ < ADC_VALUES_COUNT) {
            adc_values_count_++;
        }
    }

    void CheckBatteryStatus() {
        if (battery_update_paused_) {
            return;
        }

        ReadBatteryAdcData();

        uint32_t fast_mv = FastAverageMillivolts();
        uint32_t slow_mv = SlowAverageMillivolts();
        int delta_mv = static_cast<int>(fast_mv) - static_cast<int>(slow_mv);
        CalculateBatteryLevel(fast_mv);

        if (charging_pin_ != GPIO_NUM_NC) {
            charge_confirm_ticks_ = 0;
            discharge_confirm_ticks_ = 0;
            SetCharging(gpio_get_level(charging_pin_) == 0);
        } else if (UsbHostConnected()) {
            // USB 枚举成功说明当前接的是主机/电脑，VBUS 一定存在
            usb_was_connected_ = true;
            charge_confirm_ticks_ = 0;
            discharge_confirm_ticks_ = 0;
            SetCharging(true);
        } else if (usb_was_connected_) {
            // 从 USB 主机拔掉，VBUS 已经消失，立即恢复放电状态
            usb_was_connected_ = false;
            charge_confirm_ticks_ = 0;
            discharge_confirm_ticks_ = 0;
            SetCharging(false);
        } else if (resume_grace_ticks_ > 0) {
            // 舵机动作刚结束时电压会回弹，先跳过几秒，避免误判成插充电器
            resume_grace_ticks_--;
            charge_confirm_ticks_ = 0;
            discharge_confirm_ticks_ = 0;
        } else if (!is_charging_) {
            if (delta_mv >= CHARGE_DELTA_MV) {
                charge_confirm_ticks_++;
                if (charge_confirm_ticks_ >= CHARGE_CONFIRM_TICKS) {
                    charge_confirm_ticks_ = 0;
                    SetCharging(true);
                }
            } else {
                charge_confirm_ticks_ = 0;
            }
        } else {
            if (delta_mv <= -DISCHARGE_DELTA_MV) {
                discharge_confirm_ticks_++;
                if (discharge_confirm_ticks_ >= DISCHARGE_CONFIRM_TICKS) {
                    discharge_confirm_ticks_ = 0;
                    SetCharging(false);
                }
            } else {
                discharge_confirm_ticks_ = 0;
            }
        }

        if (++log_tick_ >= 10) {
            log_tick_ = 0;
            ESP_LOGD("PowerManager",
                     "电池: fast=%lumV slow=%lumV delta=%dmV level=%u%% charging=%d", fast_mv,
                     slow_mv, delta_mv, battery_level_, is_charging_);
        }
    }

public:
    PowerManager(gpio_num_t charging_pin, adc_unit_t adc_unit = ADC_UNIT_2,
                 adc_channel_t adc_channel = ADC_CHANNEL_3)
        : charging_pin_(charging_pin), adc_unit_(adc_unit), adc_channel_(adc_channel) {
        if (charging_pin_ != GPIO_NUM_NC) {
            gpio_config_t io_conf = {};
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pin_bit_mask = (1ULL << charging_pin_);
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            gpio_config(&io_conf);
            ESP_LOGI("PowerManager", "充电检测引脚配置完成: GPIO%d", charging_pin_);
        } else {
            ESP_LOGI("PowerManager", "充电检测引脚未配置，使用 USB + 电池电压趋势检测充电状态");
        }

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
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000));

        InitializeAdc();
    }

    void InitializeAdc() {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = adc_unit_,
            .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle_));

        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, adc_channel_, &chan_config));

        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = adc_unit_,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle_);
        if (ret == ESP_OK) {
            ESP_LOGI("PowerManager", "ADC 校准初始化成功");
        } else {
            ESP_LOGW("PowerManager", "ADC 校准初始化失败，将使用粗略估算");
            adc_cali_handle_ = nullptr;
        }
    }

    ~PowerManager() {
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }
        if (adc_cali_handle_) {
            adc_cali_delete_scheme_curve_fitting(adc_cali_handle_);
        }
        if (adc_handle_) {
            adc_oneshot_del_unit(adc_handle_);
        }
    }

    bool IsCharging() { return is_charging_; }

    uint8_t GetBatteryLevel() { return battery_level_; }

    // 暂停/恢复电量更新（用于动作执行时屏蔽更新）
    static void PauseBatteryUpdate() { battery_update_paused_ = true; }

    static void ResumeBatteryUpdate() {
        battery_update_paused_ = false;
        resume_grace_ticks_ = RESUME_GRACE_TICKS;
    }
};

#endif  // __POWER_MANAGER_H__
