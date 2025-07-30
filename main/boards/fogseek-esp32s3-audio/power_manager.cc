#include "power_manager.h"
#include <esp_log.h>

static const char *TAG = "PowerManager";

void PowerManager::CheckChargeStatus()
{
    // 获取 CHRG 和 STDBY 的状态
    bool bat_charg_state = gpio_get_level(bat_charg_gpio_) == 0; // 充电中（拉低）
    bool bat_done_state = gpio_get_level(bat_done_gpio_) == 0;   // 充电完成（拉低）

    // 更新充电状态
    if (bat_charg_state != is_charging_)
    {
        is_charging_ = bat_charg_state;
        if (on_charging_status_changed_)
        {
            on_charging_status_changed_(is_charging_);
        }
    }

    // 更新充电完成状态
    if (bat_done_state != is_charge_done_)
    {
        is_charge_done_ = bat_done_state;
        if (on_charge_done_status_changed_)
        {
            on_charge_done_status_changed_(is_charge_done_);
        }
    }
}

void PowerManager::ReadBatteryAdcData()
{
    int adc_value;
    ESP_ERROR_CHECK(adc_oneshot_read(adc_handle_, ADC_CHANNEL_2, &adc_value));

    // 将 ADC 值添加到队列中
    adc_values_.push_back(adc_value);
    if (adc_values_.size() > kBatteryAdcDataCount)
    {
        adc_values_.erase(adc_values_.begin());
    }
    uint32_t average_adc = 0;
    for (auto value : adc_values_)
    {
        average_adc += value;
    }
    average_adc /= adc_values_.size();

    // 定义电池电量区间（根据实际情况调整）
    const struct
    {
        uint16_t adc;
        uint8_t level;
    } levels[] = {
        {1241, 0},
        {1449, 20},
        {1530, 40},
        {1570, 60},
        {1650, 80},
        {1737, 100}};

    // 低于最低值时
    if (average_adc < levels[0].adc)
    {
        battery_level_ = 0;
    }
    // 高于最高值时
    else if (average_adc >= levels[5].adc)
    {
        battery_level_ = 100;
    }
    else
    {
        // 线性插值计算中间值
        for (int i = 0; i < 5; i++)
        {
            if (average_adc >= levels[i].adc && average_adc < levels[i + 1].adc)
            {
                float ratio = static_cast<float>(average_adc - levels[i].adc) / (levels[i + 1].adc - levels[i].adc);
                battery_level_ = levels[i].level + ratio * (levels[i + 1].level - levels[i].level);
                break;
            }
        }
    }

    // Check low battery status
    if (adc_values_.size() >= kBatteryAdcDataCount)
    {
        bool new_low_battery_status = battery_level_ <= kLowBatteryLevel;
        if (new_low_battery_status != is_low_battery_)
        {
            is_low_battery_ = new_low_battery_status;
            if (on_low_battery_status_changed_)
            {
                on_low_battery_status_changed_(is_low_battery_);
            }
        }
    }

    ESP_LOGI(TAG, "ADC value: %d average: %ld level: %ld", adc_value, average_adc, battery_level_);
}

void PowerManager::CheckBatteryStatus()
{
    CheckChargeStatus();

    // 如果电池电量数据不足，则读取电池电量数据
    if (adc_values_.size() < kBatteryAdcDataCount)
    {
        ReadBatteryAdcData();
        return;
    }

    // 每分钟读取一次电池电量数据
    ticks_++;
    if (ticks_ % kBatteryAdcInterval == 0)
    {
        ReadBatteryAdcData();
    }
}

PowerManager::PowerManager(gpio_num_t bat_charg_gpio, gpio_num_t bat_done_gpio)
    : bat_charg_gpio_(bat_charg_gpio), bat_done_gpio_(bat_done_gpio)
{
    // 配置 GPIO 模式为输入模式
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << bat_charg_gpio_) | (1ULL << bat_done_gpio_);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // 创建电池状态检查定时器（每秒触发一次）
    esp_timer_create_args_t timer_args = {
        .callback = [](void *arg)
        {
            PowerManager *self = static_cast<PowerManager *>(arg);
            self->CheckBatteryStatus();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "battery_check_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, 1000000)); // 1秒触发一次

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
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, ADC_CHANNEL_2, &chan_config));
}

PowerManager::~PowerManager()
{
    if (timer_handle_)
    {
        esp_timer_stop(timer_handle_);
        esp_timer_delete(timer_handle_);
    }
    if (adc_handle_)
    {
        adc_oneshot_del_unit(adc_handle_);
    }
}

bool PowerManager::IsCharging()
{
    return is_charging_;
}

bool PowerManager::IsChargeDone()
{
    return is_charge_done_;
}

uint8_t PowerManager::GetBatteryLevel()
{
    return battery_level_;
}

void PowerManager::OnChargingStatusChanged(std::function<void(bool)> callback)
{
    RegisterCallback(on_charging_status_changed_, callback);
}

void PowerManager::OnChargeDoneStatusChanged(std::function<void(bool)> callback)
{
    RegisterCallback(on_charge_done_status_changed_, callback);
}

void PowerManager::OnLowBatteryStatusChanged(std::function<void(bool)> callback)
{
    RegisterCallback(on_low_battery_status_changed_, callback);
}