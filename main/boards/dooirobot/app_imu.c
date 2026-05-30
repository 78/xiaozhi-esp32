/** 
 * @file    app_imu.c
 * @brief   IMU 应用层实现 — 按需上电 + 偏航角积分 + 倾斜检测 + 推动检测
 */

#include "app_imu.h"
#include "imu_bmi260.h"

#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* ------------------------------------------------------------------ */
/*  常量                                                               */
/* ------------------------------------------------------------------ */

static const char *TAG = "APP_IMU";

/* 倾斜稳定判定 */
#define TILT_DEBOUNCE_TICKS         10
#define TILT_HYSTERESIS_DEG         5.0f
#define TILT_SMOOTH_ALPHA           0.25f

/* 偏航积分 */
#define DT_MAX_SEC                  0.2f

/* 重力估计低通（越小越稳，越抗水平移动） */
#define GRAVITY_LPF_ALPHA           0.10f

/* 静止判定：线性加速度与陀螺仪阈值 */
#define LINACC_STATIC_MAX_G         0.12f
#define GYRO_STATIC_MAX_DPS         8.0f
#define STATIC_CONFIRM_TICKS        3

/* 1g 合法范围（防止剧烈运动污染重力估计） */
#define ACCEL_MAG_MIN_G             0.70f
#define ACCEL_MAG_MAX_G             1.30f

/* 推动检测触发后的冷却时间 (50Hz ODR, 25ticks = 0.5秒) */
#define PUSH_COOLDOWN_TICKS         25

#define SQ(x)                       ((x) * (x))

/* ------------------------------------------------------------------ */
/*  外部依赖                                                           */
/* ------------------------------------------------------------------ */

extern i2c_master_bus_handle_t i2c_bus_;

/* ------------------------------------------------------------------ */
/*  全局回调                                                           */
/* ------------------------------------------------------------------ */

static imu_tilt_cb_t g_tilt_cb  = NULL;
static void         *g_tilt_arg = NULL;

static imu_push_cb_t g_push_cb  = NULL;   // <== 新增
static void         *g_push_arg = NULL;   // <== 新增

/* ------------------------------------------------------------------ */
/*  内部状态                                                           */
/* ------------------------------------------------------------------ */

static struct {
    portMUX_TYPE mux;
    float        yaw_integral;
    int64_t      last_ts_us;
    app_imu_data_t latest_data;
} s_yaw = {
    .mux          = portMUX_INITIALIZER_UNLOCKED,
    .yaw_integral = 0.0f,
    .last_ts_us   = 0,
};

static struct {
    bool hw_on;
    bool tilt_en;
    bool push_en;          // <== 新增
    bool motor_require;
} s_pwr = {
    .hw_on         = true,
    .tilt_en       = false,
    .push_en       = false, // <== 新增
    .motor_require = false,
};

/* 推动检测状态 */
static struct {
    float threshold_g;
    int   cooldown_count;
} s_push = {
    .threshold_g    = 0.3f,
    .cooldown_count = 0,
};

static struct {
    float threshold_deg;
    bool  first_frame;
    float smooth_pitch;
    float smooth_roll;
    imu_tilt_dir_t stable_dir;
    imu_tilt_dir_t candidate_dir;
    int            candidate_count;

    bool  gravity_inited;
    float gx, gy, gz;
    int static_count;
} s_tilt = {
    .threshold_deg   = 30.0f,
    .first_frame     = true,
    .smooth_pitch    = 0.0f,
    .smooth_roll     = 0.0f,
    .stable_dir      = IMU_TILT_NONE,
    .candidate_dir   = IMU_TILT_NONE,
    .candidate_count = 0,

    .gravity_inited  = false,
    .gx              = 0.0f,
    .gy              = 0.0f,
    .gz              = -1.0f,
    .static_count    = 0,
};

/* ------------------------------------------------------------------ */
/*  私有辅助                                                           */
/* ------------------------------------------------------------------ */

static inline float calc_pitch(float ax, float ay, float az) {
    return atan2f(-ax, sqrtf(SQ(ay) + SQ(az))) * (180.0f / (float)M_PI);
}

static inline float calc_roll(float ax, float ay, float az) {
    return atan2f(ay, az) * (180.0f / (float)M_PI);
}

static inline bool accel_mag_reasonable(float ax, float ay, float az) {
    const float mag_sq = SQ(ax) + SQ(ay) + SQ(az);
    return (mag_sq >= SQ(ACCEL_MAG_MIN_G)) && (mag_sq <= SQ(ACCEL_MAG_MAX_G));
}

static inline float tilt_threshold_for(imu_tilt_dir_t dir) {
    float thr = s_tilt.threshold_deg;
    if (s_tilt.candidate_dir == dir) {
        thr -= TILT_HYSTERESIS_DEG;
    }
    return thr;
}

static imu_tilt_dir_t detect_dir(float pitch, float roll) {
    if      (roll  >  tilt_threshold_for(IMU_TILT_RIGHT)) return IMU_TILT_RIGHT;
    else if (roll  < -tilt_threshold_for(IMU_TILT_LEFT))  return IMU_TILT_LEFT;
    else if (pitch >  tilt_threshold_for(IMU_TILT_FRONT)) return IMU_TILT_FRONT;
    else if (pitch < -tilt_threshold_for(IMU_TILT_BACK))  return IMU_TILT_BACK;
    return IMU_TILT_NONE;
}

static void debounce_update(imu_tilt_dir_t cur_dir) {
    if (cur_dir != s_tilt.candidate_dir) {
        s_tilt.candidate_dir   = cur_dir;
        s_tilt.candidate_count = 1;
        return;
    }

    if (s_tilt.candidate_count < TILT_DEBOUNCE_TICKS) {
        s_tilt.candidate_count++;
    }

    if (s_tilt.candidate_count >= TILT_DEBOUNCE_TICKS &&
        s_tilt.stable_dir != s_tilt.candidate_dir)
    {
        s_tilt.stable_dir = s_tilt.candidate_dir;
        if (s_tilt.stable_dir != IMU_TILT_NONE && g_tilt_cb) {
            g_tilt_cb(s_tilt.stable_dir, g_tilt_arg);
        }
    }
}

static inline void clear_tilt_state_keep_gravity(void) {
    s_tilt.first_frame     = true;
    s_tilt.stable_dir      = IMU_TILT_NONE;
    s_tilt.candidate_dir   = IMU_TILT_NONE;
    s_tilt.candidate_count = 0;
    s_tilt.static_count    = 0;
}

static void update_hw_power(void) {
    bool need = s_pwr.tilt_en || s_pwr.push_en || s_pwr.motor_require;

    if (need == s_pwr.hw_on) return;

    if (bmi260_is_initialized()) {
        bmi260_set_power(need);
        s_pwr.hw_on = need;
    }

    if (need) {
        portENTER_CRITICAL(&s_yaw.mux);
        s_yaw.last_ts_us   = 0;
        s_yaw.yaw_integral = 0.0f;
        portEXIT_CRITICAL(&s_yaw.mux);

        s_tilt.first_frame     = true;
        s_tilt.stable_dir      = IMU_TILT_NONE;
        s_tilt.candidate_dir   = IMU_TILT_NONE;
        s_tilt.candidate_count = 0;

        s_tilt.gravity_inited  = false;
        s_tilt.gx              = 0.0f;
        s_tilt.gy              = 0.0f;
        s_tilt.gz              = -1.0f;
        s_tilt.static_count    = 0;

        s_push.cooldown_count  = 0; // 重置推动冷却
    }
}

/* ------------------------------------------------------------------ */
/*  传感器数据回调                                                     */
/* ------------------------------------------------------------------ */

static void on_sensor_data(const bmi260_raw_data_t *raw,
                           const bmi260_phys_data_t *phys,
                           void *arg)
{
    (void)raw;
    (void)arg;

    const float ax =  phys->ax;
    const float ay = -phys->ay;
    const float az = -phys->az;

    const float gx =  phys->gx;
    const float gy = -phys->gy;
    const float gz = -phys->gz;

    /* ── 1. 偏航角积分 ── */
    portENTER_CRITICAL(&s_yaw.mux);
    s_yaw.latest_data.ax = ax;
    s_yaw.latest_data.ay = ay;
    s_yaw.latest_data.az = az;
    s_yaw.latest_data.gx = gx;
    s_yaw.latest_data.gy = gy;
    s_yaw.latest_data.gz = gz;

    if (s_yaw.last_ts_us != 0) {
        float dt = (phys->timestamp_us - s_yaw.last_ts_us) / 1e6f;
        if (dt > 0.0f && dt < DT_MAX_SEC) {
            s_yaw.yaw_integral += gz * dt;
        }
    }
    s_yaw.last_ts_us = phys->timestamp_us;
    portEXIT_CRITICAL(&s_yaw.mux);

    /* ── 2. 判断是否需要处理高级特性（倾斜/推动） ── */
    bool do_tilt = s_pwr.tilt_en && g_tilt_cb;
    bool do_push = s_pwr.push_en && g_push_cb;

    if (!do_tilt && !do_push) {
        return;
    }

    /* ── 3. 更新重力估计 ── 
     * 只有在受力相对平缓(接近1g)时，才将其视为重力方向的更新。
     * 剧烈推车时（大于1.3g），不更新重力（保留推车前的垂直向下向量），
     * 以便更准确地分离出水平向的推力！
     */
    if (accel_mag_reasonable(ax, ay, az)) {
        if (!s_tilt.gravity_inited) {
            s_tilt.gx = ax;
            s_tilt.gy = ay;
            s_tilt.gz = az;
            s_tilt.gravity_inited = true;
        } else {
            const float inv = 1.0f - GRAVITY_LPF_ALPHA;
            s_tilt.gx = s_tilt.gx * inv + ax * GRAVITY_LPF_ALPHA;
            s_tilt.gy = s_tilt.gy * inv + ay * GRAVITY_LPF_ALPHA;
            s_tilt.gz = s_tilt.gz * inv + az * GRAVITY_LPF_ALPHA;
        }
    } else {
        // 加速度异常（可能正在推车或晃动），清除倾斜稳定状态，但不能 return！
        if (do_tilt) clear_tilt_state_keep_gravity();
    }

    // 若重力未初始化完毕，无法剥离重力，直接返回
    if (!s_tilt.gravity_inited) {
        return;
    }

    /* 计算动态线性加速度（当前加速度 - 重力加速度） */
    const float lax = ax - s_tilt.gx;
    const float lay = ay - s_tilt.gy;
    const float laz = az - s_tilt.gz;

    /* ── 4. 推动检测逻辑 ── */
    if (do_push) {
        if (s_push.cooldown_count > 0) {
            s_push.cooldown_count--; // 处于冷却期
        } else {
            // 计算水平面(XY轴)的动态加速度平方和
            const float horiz_accel_sq = SQ(lax) + SQ(lay);
            
            if (horiz_accel_sq > SQ(s_push.threshold_g)) {
                // 触发了推动！
                s_push.cooldown_count = PUSH_COOLDOWN_TICKS;
                g_push_cb(g_push_arg);
            }
        }
    }

    /* ── 5. 倾斜检测逻辑 ── */
    if (do_tilt) {
        const float linacc_sq = SQ(lax) + SQ(lay) + SQ(laz);
        const float gyro_sq   = SQ(gx) + SQ(gy) + SQ(gz);

        const bool linacc_static = (linacc_sq <= SQ(LINACC_STATIC_MAX_G));
        const bool gyro_static   = (gyro_sq   <= SQ(GYRO_STATIC_MAX_DPS));

        if (!(linacc_static && gyro_static)) {
            s_tilt.static_count = 0;
            clear_tilt_state_keep_gravity();
            return;
        }

        if (s_tilt.static_count < STATIC_CONFIRM_TICKS) {
            s_tilt.static_count++;
            return;
        }

        const float raw_pitch = calc_pitch(s_tilt.gx, s_tilt.gy, s_tilt.gz);
        const float raw_roll  = calc_roll (s_tilt.gx, s_tilt.gy, s_tilt.gz);

        if (s_tilt.first_frame) {
            s_tilt.smooth_pitch = raw_pitch;
            s_tilt.smooth_roll  = raw_roll;
            s_tilt.first_frame  = false;
        } else {
            const float inv = 1.0f - TILT_SMOOTH_ALPHA;
            s_tilt.smooth_pitch = s_tilt.smooth_pitch * inv + raw_pitch * TILT_SMOOTH_ALPHA;
            s_tilt.smooth_roll  = s_tilt.smooth_roll  * inv + raw_roll  * TILT_SMOOTH_ALPHA;
        }

        imu_tilt_dir_t cur_dir = detect_dir(s_tilt.smooth_pitch, s_tilt.smooth_roll);
        debounce_update(cur_dir);
    }
}

/* ------------------------------------------------------------------ */
/*  公共接口                                                           */
/* ------------------------------------------------------------------ */

esp_err_t app_imu_init(void)
{
    const bmi260_config_t drv_cfg = {
        .i2c_bus     = i2c_bus_,
        .i2c_addr    = BMI260_I2C_ADDR_PRIMARY,
        .int_pin     = IMU_INT_PIN,
        .accel_range = BMI260_ACCEL_RANGE_4G,
        .gyro_range  = BMI260_GYRO_RANGE_1000DPS,
        .accel_odr   = BMI260_ODR_50HZ,
        .gyro_odr    = BMI260_ODR_50HZ,
    };

    esp_err_t ret = bmi260_init(&drv_cfg);
    if (ret != ESP_OK) return ret;

    bmi260_register_data_callback(on_sensor_data, NULL);

    ret = bmi260_start_task(IMU_TASK_PRIORITY, IMU_TASK_STACK);
    if (ret != ESP_OK) return ret;

    update_hw_power();
    ESP_LOGI(TAG, "App IMU initialized");
    return ESP_OK;
}

void app_imu_set_tilt_callback(imu_tilt_cb_t cb, void *arg) {
    g_tilt_cb  = cb;
    g_tilt_arg = arg;
}

void app_imu_enable_tilt_detect(bool enable, float threshold_deg) {
    s_tilt.threshold_deg = threshold_deg;
    s_pwr.tilt_en        = enable;
    update_hw_power();
    if (!enable) clear_tilt_state_keep_gravity();
}

/* ── 新增：推动检测公共接口 ── */

void app_imu_set_push_callback(imu_push_cb_t cb, void *arg) {
    g_push_cb  = cb;
    g_push_arg = arg;
    ESP_LOGI(TAG, "Push callback %s", cb ? "registered" : "cleared");
}

void app_imu_enable_push_detect(bool enable, float threshold_g) {
    s_push.threshold_g = threshold_g;
    s_pwr.push_en      = enable;
    update_hw_power();
    ESP_LOGI(TAG, "Push detect %s (threshold=%.2fg)", enable ? "enabled" : "disabled", threshold_g);
}

// ... 其它原有 API 代码保持不变 ...
void app_imu_require(bool require) {
    s_pwr.motor_require = require;
    update_hw_power();
}

void app_imu_reset_yaw(void) {
    portENTER_CRITICAL(&s_yaw.mux);
    s_yaw.yaw_integral = 0.0f;
    s_yaw.last_ts_us   = 0;
    portEXIT_CRITICAL(&s_yaw.mux);
}

float app_imu_get_yaw(void) {
    portENTER_CRITICAL(&s_yaw.mux);
    float y = s_yaw.yaw_integral;
    portEXIT_CRITICAL(&s_yaw.mux);
    return y;
}

void app_imu_get_data(app_imu_data_t *data) {
    if (!data) return;
    portENTER_CRITICAL(&s_yaw.mux);
    *data = s_yaw.latest_data;
    portEXIT_CRITICAL(&s_yaw.mux);
}