/**
 * @file power_manager.h
 * @brief 九川开发板电源管理模块
 *
 * 功能：
 * 1. 双 ADC 通道监控（GPIO4=电池电压，GPIO5=USB电压）
 * 2. 电池电量估算（基于 OCV-SOC 模型）
 * 3. 充电状态检测（通过 VBUS 电压判断）
 * 4. 电池充满检测（通过 LGS4056HEP DONE引脚）
 * 5. 低电量警告
 * 6. 关机流程管理
 *
 * 硬件连接：
 * - GPIO4 (ADC1_CH3): 电池电压检测（通过 200kΩ/100kΩ 分压）
 * - GPIO5 (ADC1_CH4): USB电压检测（通过 200kΩ/100kΩ 分压）
 * - GPIO15: 电源使能控制
 * - GPIO3: 电源按键检测
 * - GPIO16: LGS4056HEP DONE引脚（电池充满检测，低电平=充满）
 *
 * @author Jiuchuan Dev Team
 * @date 2025
 */

#pragma once

#include <vector>
#include <functional>

#include <esp_timer.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "adc_battery_estimation.h"

// ============================================================================
// 硬件配置常量
// ============================================================================

// ADC 基础配置
#define JIUCHUAN_ADC_UNIT           (ADC_UNIT_1)
#define JIUCHUAN_ADC_BITWIDTH       (ADC_BITWIDTH_12)      // 12位精度 (0-4095)
#define JIUCHUAN_ADC_ATTEN          (ADC_ATTEN_DB_12)      // 12dB衰减 (0-3.3V)

// 电池电压检测 (VBAT) - GPIO4 -> ADC1_CH3
#define JIUCHUAN_BATTERY_ADC_CHANNEL        (ADC_CHANNEL_3)     // GPIO4
#define JIUCHUAN_BATTERY_RESISTOR_UPPER     (200000)            // 上臂电阻 200kΩ
#define JIUCHUAN_BATTERY_RESISTOR_LOWER     (100000)            // 下臂电阻 100kΩ
#define JIUCHUAN_BATTERY_VOLTAGE_CALIBRATION (1.0f)             // 校准系数

// USB 电压检测 (VBUS) - GPIO5 -> ADC1_CH4
#define JIUCHUAN_VBUS_ADC_CHANNEL           (ADC_CHANNEL_4)     // GPIO5
#define JIUCHUAN_VBUS_RESISTOR_UPPER        (200000)            // 上臂电阻 200kΩ
#define JIUCHUAN_VBUS_RESISTOR_LOWER        (100000)            // 下臂电阻 100kΩ
#define JIUCHUAN_VBUS_CHARGING_THRESHOLD_MV (1000)              // 充电检测阈值 1.0V

// 运行参数配置
#define JIUCHUAN_ADC_SAMPLE_COUNT           (5)                 // ADC采样次数
#define JIUCHUAN_ADC_SAMPLE_INTERVAL_MS     (10)                // 采样间隔 10ms
#define JIUCHUAN_BATTERY_CHECK_INTERVAL_MS  (2000)              // 定时器周期 2秒（优化：从6秒改为2秒）
#define JIUCHUAN_BATTERY_READ_INTERVAL      (3)                 // 每6秒读取电池 (2*3=6s)
#define JIUCHUAN_LOW_BATTERY_LEVEL          (20)                // 低电量阈值 20%

#undef TAG
#define TAG "PowerManager"

// ============================================================================
// PowerManager 类定义
// ============================================================================

/**
 * @class PowerManager
 * @brief 电源管理类
 * 
 * 提供电池监控、充电检测、电源管理等功能
 */
class PowerManager {
private:
    // ------------------------------------------------------------------------
    // 私有成员变量
    // ------------------------------------------------------------------------
    
    // 硬件句柄
    esp_timer_handle_t timer_handle_;                           // 定时器句柄
    adc_oneshot_unit_handle_t adc_handle_;                      // 共享 ADC 句柄
    adc_cali_handle_t adc_cali_handle_;                         // ADC 校准句柄
    adc_battery_estimation_handle_t adc_battery_estimation_handle_; // 电量估算句柄
    
    // 状态变量
    gpio_num_t charging_pin_;                                   // 充电检测引脚
    gpio_num_t battery_full_pin_;                               // 电池充满检测引脚(DONE)
    int32_t battery_level_;                                     // 电池电量 (0-100%)
    bool is_charging_;                                          // 充电状态
    bool is_battery_full_;                                      // 电池充满状态
    volatile bool battery_full_event_;                          // 中断事件标志（volatile确保可见性）
    bool is_low_battery_;                                       // 低电量标志
    bool is_empty_battery_;                                     // 电量耗尽标志
    int ticks_;                                                 // 定时器计数
    
    // 回调函数
    std::function<void(bool)> on_charging_status_changed_;      // 充电状态变化回调
    std::function<void(bool)> on_low_battery_status_changed_;   // 低电量状态变化回调
    
    // 配置常量
    const int kBatteryReadInterval = JIUCHUAN_BATTERY_READ_INTERVAL;
    const int kLowBatteryLevel = JIUCHUAN_LOW_BATTERY_LEVEL;
    
    // ------------------------------------------------------------------------
    // 私有辅助方法
    // ------------------------------------------------------------------------

    /**
     * @brief DONE引脚中断处理函数（静态方法）
     *
     * 仅在下降沿触发（高→低），表示电池刚充满
     * - 充电中：DONE = 高电平
     * - 充满时：DONE = 低电平（触发此中断）
     * - 拔掉充电器：DONE = 高电平（不触发中断，避免误触）
     *
     * 注意：在ISR中只设置标志位，实际处理在定时器回调中进行
     */
    static void IRAM_ATTR DonePinIsrHandler(void* arg) {
        PowerManager* self = static_cast<PowerManager*>(arg);
        // 设置事件标志，通知主任务处理
        self->battery_full_event_ = true;
    }

    /**
     * @brief 更新电池充满状态
     *
     * 读取DONE引脚当前状态并更新is_battery_full_
     */
    void UpdateBatteryFullStatus() {
        if (battery_full_pin_ != GPIO_NUM_NC) {
            int done_level = gpio_get_level(battery_full_pin_);
            bool new_battery_full = (done_level == 0);  // 低电平表示充满

            // 如果充满状态发生变化，输出日志
            if (new_battery_full != is_battery_full_) {
                is_battery_full_ = new_battery_full;
                ESP_LOGI(TAG, "电池充满状态变化: %s (DONE引脚=%d)",
                        is_battery_full_ ? "已充满" : "未充满", done_level);
            }
        }
    }

    /**
     * @brief 检查电池状态（定时器回调）
     *
     * 每 2 秒调用一次：
     * - 检查中断事件标志 → 立即处理充满事件
     * - 检测充电状态变化 → 立即读取电池并触发回调通知界面
     * - 每 6 秒读取一次电池电量
     */
    void CheckBatteryStatus() {
        // 1. 检查中断事件标志（充满事件）
        if (battery_full_event_) {
            battery_full_event_ = false;  // 清除标志
            UpdateBatteryFullStatus();
            ReadBatteryData();  // 立即读取电池数据
            ESP_LOGI(TAG, "检测到充满中断事件，立即更新状态");
        }

        // 2. 定期更新电池充满状态（防止遗漏）
        UpdateBatteryFullStatus();

        // 3. 获取充电状态（VBUS连接检测）
        bool new_charging_status = false;
        esp_err_t ret = adc_battery_estimation_get_charging_state(
            adc_battery_estimation_handle_, &new_charging_status);

        // 4. 如果DONE引脚显示已充满，则不再显示充电中
        if (new_charging_status && is_battery_full_) {
            new_charging_status = false;
        }

        // 5. 充电状态变化：立即读取并通知
        if (ret == ESP_OK && new_charging_status != is_charging_) {
            is_charging_ = new_charging_status;
            ESP_LOGI(TAG, "充电状态变化: %s", is_charging_ ? "充电中" : "未充电");

            if (on_charging_status_changed_) {
                on_charging_status_changed_(is_charging_);
            }

            ReadBatteryData();  // 充电状态变化时立即读取
            return;
        }

        // 6. 定期读取电池电量（每 6 秒）
        ticks_++;
        if (ticks_ % kBatteryReadInterval == 0) {
            ReadBatteryData();
        }
    }
    
    /**
     * @brief 读取电池电压和电量
     * 
     * 步骤：
     * 1. 多次采样求平均（5次，间隔10ms）
     * 2. 转换为实际电池电压（考虑分压比）
     * 3. 通过库获取电量百分比
     * 4. 输出日志
     */
    void ReadBatteryData() {
        // ---- 步骤 1: 多次采样求平均 ----
        int total_voltage_mv = 0;
        int sample_count = JIUCHUAN_ADC_SAMPLE_COUNT;
        
        for (int i = 0; i < sample_count; i++) {
            int adc_raw = 0;
            int voltage_mv = 0;
            
            esp_err_t ret = adc_oneshot_read(adc_handle_, JIUCHUAN_BATTERY_ADC_CHANNEL, &adc_raw);
            if (ret == ESP_OK) {
                if (adc_cali_handle_) {
                    adc_cali_raw_to_voltage(adc_cali_handle_, adc_raw, &voltage_mv);
                } else {
                    // 无校准时使用简化计算
                    voltage_mv = (adc_raw * 3100) / 4095;
                }
                total_voltage_mv += voltage_mv;
            }
            
            vTaskDelay(pdMS_TO_TICKS(JIUCHUAN_ADC_SAMPLE_INTERVAL_MS));
        }
        
        int avg_voltage_mv = total_voltage_mv / sample_count;
        
        // ---- 步骤 2: 计算实际电池电压 ----
        // 分压比 = 100kΩ / (200kΩ + 100kΩ) = 1/3
        // 实际电压 = ADC测量 × 3 × 校准系数
        float actual_battery_voltage = (avg_voltage_mv * 3 * JIUCHUAN_BATTERY_VOLTAGE_CALIBRATION) / 1000.0f;
        
        // ---- 步骤 3: 获取电池容量百分比 ----
        float battery_capacity = 0;
        adc_battery_estimation_get_capacity(adc_battery_estimation_handle_, &battery_capacity);
        
        // 处理异常值
        if (battery_capacity > -10 && battery_capacity <= 0) {
            battery_level_ = 0;
        } else {
            battery_level_ = static_cast<int32_t>(battery_capacity);
        }
        
        // ---- 步骤 4: 获取充电状态并输出日志 ----
        bool charging = false;
        adc_battery_estimation_get_charging_state(adc_battery_estimation_handle_, &charging);

        // 根据is_battery_full_状态决定显示（统一由UpdateBatteryFullStatus更新）
        if (charging && is_battery_full_) {
            charging = false;
        }

        ESP_LOGI(TAG, "电池: %.2fV, %.1f%% %s%s",
                 actual_battery_voltage,
                 battery_capacity,
                 charging ? "[充电中]" : "",
                 is_battery_full_ ? " [已充满]" : "");
    }
    
    /**
     * @brief 充电检测回调函数（静态成员，供 adc_battery_estimation 调用）
     * 
     * 通过读取 GPIO5 (VBUS) 的 ADC 电压判断是否连接 USB 充电
     * 
     * @param user_data PowerManager 实例指针
     * @return true: 充电中, false: 未充电
     */
    static bool ChargingDetectCallback(void* user_data) {
        PowerManager* self = static_cast<PowerManager*>(user_data);
        
        // 1. 读取 VBUS ADC 原始值
        int adc_raw = 0;
        esp_err_t ret = adc_oneshot_read(self->adc_handle_, JIUCHUAN_VBUS_ADC_CHANNEL, &adc_raw);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "读取 VBUS(GPIO5) ADC 失败: %s", esp_err_to_name(ret));
            return false;
        }
        
        // 2. 转换为电压（mV）
        int voltage_mv = 0;
        if (self->adc_cali_handle_) {
            adc_cali_raw_to_voltage(self->adc_cali_handle_, adc_raw, &voltage_mv);
        } else {
            // 无校准时使用简化计算
            voltage_mv = (adc_raw * 3100) / 4095;
        }
        
        // 3. 判断充电状态
        // USB 5V 经分压后约 1.67V，阈值设为 1.0V
        bool is_charging = (voltage_mv > JIUCHUAN_VBUS_CHARGING_THRESHOLD_MV);
        
        // 计算实际 VBUS 电压（用于调试）
        int actual_vbus_mv = voltage_mv * 3;
        ESP_LOGD(TAG, "VBUS: %.2fV, %s", actual_vbus_mv / 1000.0f, 
                 is_charging ? "充电" : "未充电");
        
        return is_charging;
    }
    
    /**
     * @brief 初始化共享 ADC
     * 
     * 创建 ADC 单元并配置两个通道：
     * - GPIO4: 电池电压检测
     * - GPIO5: VBUS 电压检测
     */
    void InitializeADC() {
        // 1. 创建 ADC 单元
        adc_oneshot_unit_init_cfg_t adc_cfg = {
            .unit_id = JIUCHUAN_ADC_UNIT,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_cfg, &adc_handle_));
        
        // 2. 配置通道参数
        adc_oneshot_chan_cfg_t chan_cfg = {
            .atten = JIUCHUAN_ADC_ATTEN,
            .bitwidth = JIUCHUAN_ADC_BITWIDTH,
        };
        
        // 配置 GPIO4 - 电池电压检测
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, JIUCHUAN_BATTERY_ADC_CHANNEL, &chan_cfg));
        
        // 配置 GPIO5 - VBUS 电压检测
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle_, JIUCHUAN_VBUS_ADC_CHANNEL, &chan_cfg));
        
        // 3. 创建校准方案（提高精度）
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = JIUCHUAN_ADC_UNIT,
            .atten = JIUCHUAN_ADC_ATTEN,
            .bitwidth = JIUCHUAN_ADC_BITWIDTH,
        };
        
        esp_err_t cali_ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle_);
        if (cali_ret == ESP_OK) {
            ESP_LOGI(TAG, "ADC 校准成功");
        } else {
            ESP_LOGW(TAG, "ADC 校准失败，使用原始值: %s", esp_err_to_name(cali_ret));
            adc_cali_handle_ = nullptr;
        }
        
        ESP_LOGI(TAG, "共享 ADC 初始化完成: GPIO4(电池)=ADC1_CH3, GPIO5(VBUS)=ADC1_CH4");
    }
    
    /**
     * @brief 初始化电池估算库
     * 
     * 配置 adc_battery_estimation，使用自定义 OCV-SOC 曲线
     */
    void InitializeBatteryEstimation() {
        // 自定义电压-电量映射表（适配实际电池特性）
        static const battery_point_t battery_curve_table[] = {
            {4.2,  100},    // 4.2V = 100%
            {4.06, 80},     // 4.06V = 80%
            {3.82, 60},     // 3.82V = 60%
            {3.58, 40},     // 3.58V = 40%
            {3.34, 20},     // 3.34V = 20%
            {3.1,  0},      // 3.1V = 0%
            {3.0,  -10}     // 3.0V = -10% (保护阈值)
        };
        
        adc_battery_estimation_t config = {
            .external = {
                .adc_handle = adc_handle_,              // 使用共享 ADC
                .adc_cali_handle = adc_cali_handle_,    // 使用共享校准
            },
            .adc_channel = JIUCHUAN_BATTERY_ADC_CHANNEL,
            .upper_resistor = JIUCHUAN_BATTERY_RESISTOR_UPPER,
            .lower_resistor = JIUCHUAN_BATTERY_RESISTOR_LOWER,
            .battery_points = battery_curve_table,
            .battery_points_count = sizeof(battery_curve_table) / sizeof(battery_curve_table[0]),
            .charging_detect_cb = ChargingDetectCallback,   // 充电检测回调
            .charging_detect_user_data = this               // 传递 this 指针
        };
        
        adc_battery_estimation_handle_ = adc_battery_estimation_create(&config);
        if (!adc_battery_estimation_handle_) {
            ESP_LOGE(TAG, "创建 adc_battery_estimation 失败");
        } else {
            ESP_LOGI(TAG, "电池电量估算初始化完成（含 VBUS 充电检测）");
        }
    }
    
    /**
     * @brief 初始化电源控制 GPIO
     * 
     * 在系统启动时初始化电源使能 GPIO (GPIO15)，
     * 并处理从深度睡眠唤醒后的 GPIO 恢复
     */
    void InitializePowerControl() {
        // 检查唤醒原因
        esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
        ESP_LOGD(TAG, "Wakeup cause: %d %s", wakeup_reason,
                 wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 ? "(ext0)" : "");

        // 初始化电源使能 GPIO (GPIO15)
        rtc_gpio_init(PWR_EN_GPIO);
        rtc_gpio_set_direction(PWR_EN_GPIO, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_hold_dis(PWR_EN_GPIO);  // 释放可能存在的 hold
        rtc_gpio_set_level(PWR_EN_GPIO, 1);

        // 初始化电池充满检测引脚 (GPIO16 - LGS4056HEP DONE)
        if (battery_full_pin_ != GPIO_NUM_NC) {
            gpio_config_t io_conf = {};
            io_conf.pin_bit_mask = (1ULL << battery_full_pin_);
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;    // 使能内部上拉，DONE为开漏输出
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.intr_type = GPIO_INTR_NEGEDGE;      // 仅下降沿触发中断(充电中→充满)
            ESP_ERROR_CHECK(gpio_config(&io_conf));

            // 安全安装GPIO中断服务（可能已被其他模块安装）
            esp_err_t isr_ret = gpio_install_isr_service(0);
            if (isr_ret == ESP_ERR_INVALID_STATE) {
                ESP_LOGD(TAG, "GPIO ISR服务已安装，跳过");
            } else if (isr_ret != ESP_OK) {
                ESP_LOGE(TAG, "安装GPIO ISR服务失败: %s", esp_err_to_name(isr_ret));
                return;
            }

            // 添加中断处理函数
            ESP_ERROR_CHECK(gpio_isr_handler_add(battery_full_pin_, DonePinIsrHandler, this));

            // 读取初始状态
            int initial_level = gpio_get_level(battery_full_pin_);
            is_battery_full_ = (initial_level == 0);

            ESP_LOGI(TAG, "电池充满检测引脚 GPIO%d 初始化完成 (当前状态: %s, 下降沿中断)",
                     battery_full_pin_,
                     is_battery_full_ ? "已充满" : "未充满");
        }

        // 如果从深度睡眠唤醒，解除 GPIO3 的 RTC GPIO 模式，恢复为普通 GPIO
        // 这样按钮驱动才能正常工作
        if (rtc_gpio_is_valid_gpio(GPIO_NUM_3)) {
            rtc_gpio_deinit(GPIO_NUM_3);
        }

        ESP_LOGI(TAG, "电源控制初始化完成");
    }
    
    /**
     * @brief 处理关机流程
     *
     * 统一采用“拉低 EN(PWR_EN)”关机方案：
     * - 等待电源键释放，避免按键按下时硬件持续拉高 EN 导致关机失败
     * - 拉低 GPIO15(PWR_EN) 触发硬件掉电
     * - 若仍未掉电（例如 USB 仍在供电导致 EN 被 VIN 拉高），进入深度睡眠作为兜底
     */
    void HandleShutdown() {
        // 配置 GPIO3 为 RTC GPIO（用于读取/去抖；作为兜底深睡唤醒源）
        ESP_ERROR_CHECK(rtc_gpio_init(PWR_BUTTON_GPIO));
        ESP_ERROR_CHECK(rtc_gpio_set_direction(PWR_BUTTON_GPIO, RTC_GPIO_MODE_INPUT_ONLY));
        ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(PWR_BUTTON_GPIO));
        ESP_ERROR_CHECK(rtc_gpio_pullup_dis(PWR_BUTTON_GPIO));

        // 等待按键释放：按键按下时硬件可能持续拉高 PWR_EN，必须释放后才能关机成功
        vTaskDelay(pdMS_TO_TICKS(100));
        int wait_count = 0;
        while (rtc_gpio_get_level(PWR_BUTTON_GPIO) == 1 && wait_count < 50) {
            vTaskDelay(pdMS_TO_TICKS(100));
            wait_count++;
        }

        if (rtc_gpio_get_level(PWR_BUTTON_GPIO) == 0) {
            ESP_LOGI(TAG, "电源键已释放，开始关机流程");
        } else {
            ESP_LOGW(TAG, "等待电源键释放超时，继续执行关机流程");
        }

        // 额外延迟去抖动
        vTaskDelay(pdMS_TO_TICKS(200));

        // 拉低 PWR_EN 断电关机（EN 由硬件下拉电阻拉低）
        ESP_LOGI(TAG, "拉低 PWR_EN 断电关机");
        ESP_ERROR_CHECK(rtc_gpio_init(PWR_EN_GPIO));
        ESP_ERROR_CHECK(rtc_gpio_set_direction(PWR_EN_GPIO, RTC_GPIO_MODE_OUTPUT_ONLY));
        rtc_gpio_hold_dis(PWR_EN_GPIO);
        rtc_gpio_set_level(PWR_EN_GPIO, 0);
        rtc_gpio_hold_en(PWR_EN_GPIO);

        // 如果未能立即掉电（例如 USB 仍供电 / EN 仍被拉高），进入深度睡眠以降低功耗
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_LOGW(TAG, "仍未掉电，进入深度睡眠作为兜底");
        ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(PWR_BUTTON_GPIO, 1));
        esp_deep_sleep_start();
    }

public:
    // ------------------------------------------------------------------------
    // 构造函数和析构函数
    // ------------------------------------------------------------------------
    
    /**
     * @brief 构造函数
     * 
     * @param pin 充电检测引脚（兼容参数，实际使用 ADC 检测）
     */
    PowerManager(gpio_num_t pin)
        : timer_handle_(nullptr)
        , adc_handle_(nullptr)
        , adc_cali_handle_(nullptr)
        , adc_battery_estimation_handle_(nullptr)
        , charging_pin_(pin)
        , battery_full_pin_(BATTERY_FULL_PIN)
        , battery_level_(100)
        , is_charging_(false)
        , is_battery_full_(false)
        , battery_full_event_(false)
        , is_low_battery_(false)
        , is_empty_battery_(false)
        , ticks_(0) {
        
        // 1. 初始化电源控制 GPIO
        InitializePowerControl();
        
        // 2. 初始化 ADC
        InitializeADC();
        
        // 3. 初始化电池估算库
        InitializeBatteryEstimation();
        
        // 4. 创建定时器（每 6 秒检查一次）
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
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_, JIUCHUAN_BATTERY_CHECK_INTERVAL_MS * 1000));
        
        // 6. 延迟后读取初始状态
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_LOGI(TAG, "PowerManager 初始化完成");
        ReadBatteryData();
    }
    
    /**
     * @brief 析构函数
     * 
     * 释放所有资源
     */
    ~PowerManager() {
        // 移除GPIO中断处理
        if (battery_full_pin_ != GPIO_NUM_NC) {
            gpio_isr_handler_remove(battery_full_pin_);
        }

        // 停止并删除定时器
        if (timer_handle_) {
            esp_timer_stop(timer_handle_);
            esp_timer_delete(timer_handle_);
        }

        // 销毁电池估算句柄
        if (adc_battery_estimation_handle_) {
            adc_battery_estimation_destroy(adc_battery_estimation_handle_);
        }

        // 释放 ADC 校准句柄
        if (adc_cali_handle_) {
            adc_cali_delete_scheme_curve_fitting(adc_cali_handle_);
        }

        // 释放共享 ADC 句柄
        if (adc_handle_) {
            adc_oneshot_del_unit(adc_handle_);
        }
    }
    
    // ------------------------------------------------------------------------
    // 公共接口方法
    // ------------------------------------------------------------------------
    
    /**
     * @brief 判断是否正在充电
     *
     * @return true: 充电中, false: 未充电
     * @note 根据DONE引脚判断：充满时返回 false（用于界面显示）
     */
    bool IsCharging() {
        // 如果DONE引脚显示已充满，则不显示充电中
        if (is_battery_full_) {
            return false;
        }
        return is_charging_;
    }
    
    /**
     * @brief 判断是否连接 USB（硬件检测）
     * 
     * @return true: 连接 USB, false: 未连接
     * @note 不考虑电池电量，只检测 VBUS 电压
     *       用于判断电源模式（深度睡眠 vs 断电）
     */
    bool IsUsbConnected() {
        bool usb_connected = false;
        adc_battery_estimation_get_charging_state(
            adc_battery_estimation_handle_, &usb_connected);
        return usb_connected;
    }
    
    /**
     * @brief 判断是否正在放电
     * 
     * @return true: 放电中, false: 充电中
     */
    bool IsDischarging() {
        return !is_charging_;
    }
    
    /**
     * @brief 获取电池电量
     * 
     * @return 电池电量百分比 (0-100)
     */
    int32_t GetBatteryLevel() {
        return battery_level_;
    }
    
    /**
     * @brief 关机
     * 
     * 统一尝试“拉低 EN(PWR_EN)”硬件断电关机；若未能掉电则进入深度睡眠兜底
     */
    void Shutdown() {
        HandleShutdown();
    }
    
    /**
     * @brief 注册充电状态变化回调
     * 
     * @param callback 回调函数，参数为充电状态（true=充电，false=放电）
     */
    void OnChargingStatusChanged(std::function<void(bool)> callback) {
        on_charging_status_changed_ = callback;
    }
    
    /**
     * @brief 注册低电量状态变化回调
     * 
     * @param callback 回调函数，参数为低电量状态（true=低电量，false=正常）
     */
    void OnLowBatteryStatusChanged(std::function<void(bool)> callback) {
        on_low_battery_status_changed_ = callback;
    }
};
