#include "power_manager.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "PowerMgr";

/* ========== 硬件配置 ========== */
#define POWER_ADC_UNIT      ADC_UNIT_2
#define POWER_ADC_CHANNEL   ADC_CHANNEL_5
#define POWER_CHARGE_PIN    GPIO_NUM_53

/* ========== 业务逻辑常量（对齐 C++ 参考） ========== */
#define BATTERY_ADC_DATA_COUNT   3      /* 滑动窗口大小 */
#define BATTERY_ADC_INTERVAL     60     /* 数据充足后，每隔多少个 tick 采样一次 */
#define LOW_BATTERY_LEVEL        15     /* 低电量阈值 (%) */
#define TIMER_PERIOD_US          10000000 /* 定时器周期: 10秒 */

/* ========== 电池电量映射表（分段线性插值） ========== */
typedef struct {
    uint16_t adc;
    uint8_t  level;
} battery_map_t;

static const battery_map_t BATTERY_LEVELS[] = {
    {1650,   0},    /* 3.30V → 0%   (保护电压) */
    {1750,  10},    /* 3.50V → 10%  */
    {1825,  20},    /* 3.65V → 20%  */
    {1900,  40},    /* 3.80V → 40%  */
    {1975,  60},    /* 3.95V → 60%  */
    {2050,  80},    /* 4.10V → 80%  */
    {2100, 100},    /* 4.20V → 100% (满电) */
};

#define MAP_SIZE (sizeof(BATTERY_LEVELS) / sizeof(BATTERY_LEVELS[0]))

/* ========== 模块内部状态 ========== */
typedef struct {
    /* 外设句柄 */
    adc_oneshot_unit_handle_t adc_handle;
    esp_timer_handle_t        timer;

    /* 回调 */
    power_charging_cb_t       on_charging_changed;
    power_low_battery_cb_t    on_low_battery_changed;

    /* ADC 滑动窗口 */
    uint16_t adc_values[BATTERY_ADC_DATA_COUNT];
    int      adc_count;         /* 当前窗口中有效样本数 (0 ~ BATTERY_ADC_DATA_COUNT) */

    /* 电池状态 */
    uint32_t battery_level;     /* 当前电量百分比 */
    bool     is_charging;       /* 当前充电状态 */
    bool     is_low_battery;    /* 当前低电量状态 */
    int      ticks;             /* 定时器 tick 计数器 */

    /* 控制标志 */
    bool     paused;
    bool     initialized;
} power_mgr_t;

static power_mgr_t s_mgr = {0};

/* ================================================================
 *  内部函数：滑动窗口操作
 * ================================================================ */

/**
 * 向滑动窗口追加一个 ADC 值。
 * 窗口未满时直接追加；满了则丢弃最旧的一个（左移）。
 * 完全对应 C++ 中 vector 的 push_back + erase(begin) 语义。
 */
static void adc_window_push(uint16_t value)
{
    if (s_mgr.adc_count < BATTERY_ADC_DATA_COUNT) {
        /* 窗口未满，直接追加 */
        s_mgr.adc_values[s_mgr.adc_count++] = value;
    } else {
        /* 窗口已满，左移丢弃最旧值 */
        for (int i = 0; i < BATTERY_ADC_DATA_COUNT - 1; i++) {
            s_mgr.adc_values[i] = s_mgr.adc_values[i + 1];
        }
        s_mgr.adc_values[BATTERY_ADC_DATA_COUNT - 1] = value;
    }
}

/**
 * 计算窗口内所有样本的算术平均值
 */
static uint32_t adc_window_average(void)
{
    if (s_mgr.adc_count == 0) return 0;

    uint32_t sum = 0;
    for (int i = 0; i < s_mgr.adc_count; i++) {
        sum += s_mgr.adc_values[i];
    }
    return sum / (uint32_t)s_mgr.adc_count;
}

/* ================================================================
 *  内部函数：分段线性插值计算电量百分比
 * ================================================================
 *
 *  注意：C++ 原版在上界判断时使用了 levels[5]（即 {2050,80}），
 *  但映射表实际有 7 项（索引 0~6），正确的上界应该是 levels[6]。
 *  这里用 MAP_SIZE-1 修正了这个 bug。
 */
static uint8_t calc_battery_level(uint32_t adc_avg)
{
    /* 低于最低值 */
    if (adc_avg < BATTERY_LEVELS[0].adc) {
        return 0;
    }

    /* 高于最高值 */
    if (adc_avg >= BATTERY_LEVELS[MAP_SIZE - 1].adc) {
        return 100;
    }

    /* 在中间区间做线性插值 */
    for (int i = 0; i < (int)(MAP_SIZE - 1); i++) {
        if (adc_avg >= BATTERY_LEVELS[i].adc && adc_avg < BATTERY_LEVELS[i + 1].adc) {
            float ratio = (float)(adc_avg - BATTERY_LEVELS[i].adc) /
                          (float)(BATTERY_LEVELS[i + 1].adc - BATTERY_LEVELS[i].adc);
            return BATTERY_LEVELS[i].level +
                   (uint8_t)(ratio * (float)(BATTERY_LEVELS[i + 1].level - BATTERY_LEVELS[i].level));
        }
    }

    return 100; /* 理论上不会走到这里 */
}

/* ================================================================
 *  内部函数：读取一次 ADC 并更新电池状态
 *  完全对应 C++ 的 ReadBatteryAdcData()
 * ================================================================ */
static void read_battery_adc_data(void)
{
    int adc_value = 0;
    esp_err_t ret = adc_oneshot_read(s_mgr.adc_handle, POWER_ADC_CHANNEL, &adc_value);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "adc_value %d", adc_value);

    /* 将本次采样值加入滑动窗口 */
    adc_window_push((uint16_t)adc_value);

    /* 计算窗口平均值 */
    uint32_t average_adc = adc_window_average();

    /* 分段线性插值得到电量百分比 */
    s_mgr.battery_level = calc_battery_level(average_adc);

    /* 低电量状态检测：只有窗口数据充足时才判断，避免初始抖动 */
    if (s_mgr.adc_count >= BATTERY_ADC_DATA_COUNT) {
        bool new_low_battery = (s_mgr.battery_level <= LOW_BATTERY_LEVEL);
        if (new_low_battery != s_mgr.is_low_battery) {
            s_mgr.is_low_battery = new_low_battery;
            if (s_mgr.on_low_battery_changed) {
                s_mgr.on_low_battery_changed(s_mgr.is_low_battery);
            }
        }
    }

    ESP_LOGI(TAG, "ADC value: %d, average: %lu, level: %lu%%",
             adc_value, (unsigned long)average_adc, (unsigned long)s_mgr.battery_level);
}

/* ================================================================
 *  内部函数：定时器回调 — 电池状态检查主循环
 *  完全对应 C++ 的 CheckBatteryStatus()
 *
 *  逻辑流程：
 *    1. 检测充电状态是否变化 → 变化则立即通知 + 采样 + 返回
 *    2. 滑动窗口数据不足 → 立即采样 + 返回
 *    3. 数据充足 → 每 BATTERY_ADC_INTERVAL 个 tick 采样一次
 * ================================================================ */
static void power_timer_callback(void *arg)
{
    (void)arg;

    if (s_mgr.paused) {
        return;
    }

    /* ---- 第一步：检测充电状态变化 ---- */
    bool new_charging = (gpio_get_level(POWER_CHARGE_PIN) == 0);

    if (new_charging != s_mgr.is_charging) {
        s_mgr.is_charging = new_charging;

        /* 通知外部：充电状态变了 */
        if (s_mgr.on_charging_changed) {
            s_mgr.on_charging_changed(s_mgr.is_charging);
        }

        /* 充电状态变化时立即采样一次，因为电压会突变 */
        read_battery_adc_data();
        return;
    }

    /* ---- 第二步：窗口数据不足，立即补采 ---- */
    if (s_mgr.adc_count < BATTERY_ADC_DATA_COUNT) {
        read_battery_adc_data();
        return;
    }

    /* ---- 第三步：数据充足，按间隔节流采样 ---- */
    s_mgr.ticks++;
    if (s_mgr.ticks % BATTERY_ADC_INTERVAL == 0) {
        read_battery_adc_data();
    }
}

/* ================================================================
 *  公开 API
 * ================================================================ */

esp_err_t power_manager_init(const power_config_t *config)
{
    if (s_mgr.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* 清零所有状态 */
    memset(&s_mgr, 0, sizeof(power_mgr_t));

    /* ---------- 初始化充电检测引脚 ---------- */
    gpio_config_t io_conf = {
        .pin_bit_mask  = (1ULL << POWER_CHARGE_PIN),
        .mode          = GPIO_MODE_INPUT,
        .pull_up_en    = GPIO_PULLUP_ENABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    /* ---------- 初始化 ADC ---------- */
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id  = POWER_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &s_mgr.adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_mgr.adc_handle, POWER_ADC_CHANNEL, &chan_cfg));

    /* ---------- 保存回调 ---------- */
    if (config) {
        s_mgr.on_charging_changed    = config->on_charging_changed;
        s_mgr.on_low_battery_changed = config->on_low_battery_changed;
    }

    /* ---------- 初始状态 ---------- */
    s_mgr.battery_level = 80;  /* 与 C++ 一致的初始默认值 */
    s_mgr.is_charging   = (gpio_get_level(POWER_CHARGE_PIN) == 0);

    /* ---------- 首次采样（填充窗口） ---------- */
    read_battery_adc_data();

    /* ---------- 创建并启动定时器（1秒周期） ---------- */
    const esp_timer_create_args_t timer_args = {
        .callback            = power_timer_callback,
        .arg                 = NULL,
        .dispatch_method     = ESP_TIMER_TASK,
        .name                = "battery_check_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_mgr.timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_mgr.timer, TIMER_PERIOD_US));

    s_mgr.initialized = true;
    ESP_LOGI(TAG, "Initialized. Charging: %s, Level: %lu%%",
             s_mgr.is_charging ? "YES" : "NO", (unsigned long)s_mgr.battery_level);

    return ESP_OK;
}

uint8_t power_manager_get_level(void)
{
    return (uint8_t)s_mgr.battery_level;
}

bool power_manager_is_charging(void)
{
    /*
     * 与 C++ 逻辑一致：
     * 如果电量已经到 100%，即使物理上仍在充电，也返回 false。
     * 这是为了 UI 层面显示"已充满"而非"充电中"。
     */
    if (s_mgr.battery_level >= 100) {
        return false;
    }
    return s_mgr.is_charging;
}

bool power_manager_is_discharging(void)
{
    return !s_mgr.is_charging;
}

bool power_manager_is_low_battery(void)
{
    return s_mgr.is_low_battery;
}

void power_manager_pause(void)
{
    s_mgr.paused = true;
    ESP_LOGI(TAG, "Sampling paused");
}

void power_manager_resume(void)
{
    s_mgr.paused = false;
    ESP_LOGI(TAG, "Sampling resumed");
}

void power_manager_deinit(void)
{
    if (!s_mgr.initialized) {
        return;
    }

    if (s_mgr.timer) {
        esp_timer_stop(s_mgr.timer);
        esp_timer_delete(s_mgr.timer);
    }

    if (s_mgr.adc_handle) {
        adc_oneshot_del_unit(s_mgr.adc_handle);
    }

    memset(&s_mgr, 0, sizeof(power_mgr_t));
    ESP_LOGI(TAG, "Deinitialized");
}
