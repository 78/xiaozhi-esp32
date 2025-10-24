#include "mpu6050_sensor.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <esp_log.h>
#include "settings.h"
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG_MPU = "MPU6050";

// MPU6050 registers
static constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
static constexpr uint8_t REG_SMPLRT_DIV = 0x19;
static constexpr uint8_t REG_CONFIG = 0x1A;
static constexpr uint8_t REG_GYRO_CONFIG = 0x1B;
static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;

Mpu6050Sensor::Mpu6050Sensor(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, uint8_t addr, int hz)
    : port_(port), sda_(sda), scl_(scl), addr_(addr), hz_(hz) {}

Mpu6050Sensor::~Mpu6050Sensor() {
    if (dev_) {
        i2c_master_bus_rm_device(dev_);
        dev_ = nullptr;
    }
    if (bus_) {
        i2c_del_master_bus(bus_);
        bus_ = nullptr;
    }
}

bool Mpu6050Sensor::Initialize() {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = port_,
        .sda_io_num = sda_,
        .scl_io_num = scl_,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {.enable_internal_pullup = 1},
    };
    if (i2c_new_master_bus(&bus_cfg, &bus_) != ESP_OK) {
        ESP_LOGE(TAG_MPU, "Failed to init I2C bus");
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr_,
        .scl_speed_hz = static_cast<uint32_t>(hz_),
    };
    if (i2c_master_bus_add_device(bus_, &dev_cfg, &dev_) != ESP_OK) {
        ESP_LOGE(TAG_MPU, "Failed to add I2C device @0x%02x", addr_);
        return false;
    }

    // Wake up device
    if (!WriteReg(REG_PWR_MGMT_1, 0x00)) return false;
    // Set sample rate divider
    WriteReg(REG_SMPLRT_DIV, 0x07); // ~1kHz/(1+7)=125Hz base (then filtered)
    // DLPF config (5Hz accel, 5Hz gyro)
    WriteReg(REG_CONFIG, 0x06);
    // Gyro full-scale +/-250 deg/s
    WriteReg(REG_GYRO_CONFIG, 0x00);
    // Accel full-scale +/-2g
    WriteReg(REG_ACCEL_CONFIG, 0x00);
    ESP_LOGI(TAG_MPU, "MPU6050 initialized");
    return true;
}

bool Mpu6050Sensor::WriteReg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    if (i2c_master_transmit(dev_, buf, sizeof(buf), 100) != ESP_OK) {
        ESP_LOGE(TAG_MPU, "I2C write 0x%02x=0x%02x failed", reg, val);
        return false;
    }
    return true;
}

bool Mpu6050Sensor::ReadRegs(uint8_t reg, uint8_t *buf, size_t len) {
    if (i2c_master_transmit_receive(dev_, &reg, 1, buf, len, 100) != ESP_OK) {
        ESP_LOGE(TAG_MPU, "I2C read 0x%02x len %u failed", reg, (unsigned)len);
        return false;
    }
    return true;
}

bool Mpu6050Sensor::Read(Sample &out) {
    uint8_t raw[14];
    if (!ReadRegs(REG_ACCEL_XOUT_H, raw, sizeof(raw))) {
        return false;
    }
    auto rd16 = [&](int idx) -> int16_t {
        return (int16_t)((raw[idx] << 8) | raw[idx + 1]);
    };

    int16_t ax = rd16(0), ay = rd16(2), az = rd16(4);
    int16_t gx = rd16(8), gy = rd16(10), gz = rd16(12);

    // scale
    out.ax = ax / 16384.0f; // 2g
    out.ay = ay / 16384.0f;
    out.az = az / 16384.0f;
    out.gx = gx / 131.0f;   // 250 dps
    out.gy = gy / 131.0f;
    out.gz = gz / 131.0f;

    // pitch/roll (simple from accel)
    out.roll = atan2f(out.ay, out.az) * 180.0f / (float)M_PI;
    out.pitch = atan2f(-out.ax, sqrtf(out.ay*out.ay + out.az*out.az)) * 180.0f / (float)M_PI;
    return true;
}

bool Mpu6050Sensor::ReadFiltered(Sample &out) {
    if (!Read(out)) return false;

    // Apply calibration offsets/biases if available
    if (calib_.valid) {
        out.ax -= calib_.ax_off;
        out.ay -= calib_.ay_off;
        out.az -= calib_.az_off; // az_off should be ~ (avg_az - 1.0)
        out.gx -= calib_.gx_bias;
        out.gy -= calib_.gy_bias;
        out.gz -= calib_.gz_bias;
    }

    // Compute accel-only angles again after offset correction
    float roll_acc = atan2f(out.ay, out.az) * 180.0f / (float)M_PI;
    float pitch_acc = atan2f(-out.ax, sqrtf(out.ay*out.ay + out.az*out.az)) * 180.0f / (float)M_PI;

    // Complementary filter: integrate gyro, blend with accel
    int64_t now = esp_timer_get_time();
    if (!filter_initialized_ || last_us_ == 0) {
        roll_filt_ = roll_acc;
        pitch_filt_ = pitch_acc;
        filter_initialized_ = true;
    } else {
        float dt = (now - last_us_) / 1e6f; // seconds
        // Integrate gyro (deg/s)
        float roll_gyro = roll_filt_ + out.gx * dt;
        float pitch_gyro = pitch_filt_ + out.gy * dt;
        // Blend
        roll_filt_ = alpha_ * roll_gyro + (1.0f - alpha_) * roll_acc;
        pitch_filt_ = alpha_ * pitch_gyro + (1.0f - alpha_) * pitch_acc;
    }
    last_us_ = now;

    out.roll = roll_filt_;
    out.pitch = pitch_filt_;
    return true;
}

bool Mpu6050Sensor::Calibrate(int samples, int sample_delay_ms) {
    // Average many samples while stationary
    float ax_sum = 0, ay_sum = 0, az_sum = 0;
    float gx_sum = 0, gy_sum = 0, gz_sum = 0;
    int count = 0;
    for (int i = 0; i < samples; ++i) {
        Sample s{};
        if (Read(s)) {
            ax_sum += s.ax;
            ay_sum += s.ay;
            az_sum += s.az;
            gx_sum += s.gx;
            gy_sum += s.gy;
            gz_sum += s.gz;
            count++;
        }
        vTaskDelay(pdMS_TO_TICKS(sample_delay_ms));
    }
    if (count < samples / 2) {
        ESP_LOGE(TAG_MPU, "Calibration failed: insufficient samples (%d)", count);
        return false;
    }
    float ax_avg = ax_sum / count;
    float ay_avg = ay_sum / count;
    float az_avg = az_sum / count;
    float gx_avg = gx_sum / count;
    float gy_avg = gy_sum / count;
    float gz_avg = gz_sum / count;

    // When the board is face-up and still: ax≈0, ay≈0, az≈+1g
    calib_.ax_off = ax_avg;
    calib_.ay_off = ay_avg;
    calib_.az_off = az_avg - 1.0f;
    calib_.gx_bias = gx_avg;
    calib_.gy_bias = gy_avg;
    calib_.gz_bias = gz_avg;
    calib_.valid = true;
    ESP_LOGI(TAG_MPU, "Calib OK: a_off(%.3f,%.3f,%.3f) g_bias(%.3f,%.3f,%.3f)",
             calib_.ax_off, calib_.ay_off, calib_.az_off, calib_.gx_bias, calib_.gy_bias, calib_.gz_bias);
    // Reset filter seed to accel angles after calibration
    filter_initialized_ = false;
    last_us_ = 0;
    return true;
}

bool Mpu6050Sensor::SaveCalibration() {
    Settings s("imu", true);
    s.SetBool("valid", calib_.valid);
    s.SetString("ver", "1");
    s.SetInt("alpha_scaled", (int)(alpha_ * 1000));
    // Store as scaled integers to avoid locale/float issues if needed, but here we can store as strings too.
    char buf[64];
    snprintf(buf, sizeof(buf), "%.6f", calib_.ax_off); s.SetString("ax_off", buf);
    snprintf(buf, sizeof(buf), "%.6f", calib_.ay_off); s.SetString("ay_off", buf);
    snprintf(buf, sizeof(buf), "%.6f", calib_.az_off); s.SetString("az_off", buf);
    snprintf(buf, sizeof(buf), "%.6f", calib_.gx_bias); s.SetString("gx_bias", buf);
    snprintf(buf, sizeof(buf), "%.6f", calib_.gy_bias); s.SetString("gy_bias", buf);
    snprintf(buf, sizeof(buf), "%.6f", calib_.gz_bias); s.SetString("gz_bias", buf);
    return true;
}

bool Mpu6050Sensor::LoadCalibration() {
    Settings s("imu", false);
    if (!s.GetBool("valid", false)) {
        calib_.valid = false;
        return false;
    }
    auto parsef = [&](const std::string &k, float defv) -> float {
        std::string v = s.GetString(k, "");
        if (v.empty()) return defv;
        return strtof(v.c_str(), nullptr);
    };
    calib_.ax_off = parsef("ax_off", 0.0f);
    calib_.ay_off = parsef("ay_off", 0.0f);
    calib_.az_off = parsef("az_off", 0.0f);
    calib_.gx_bias = parsef("gx_bias", 0.0f);
    calib_.gy_bias = parsef("gy_bias", 0.0f);
    calib_.gz_bias = parsef("gz_bias", 0.0f);
    int alpha_scaled = s.GetInt("alpha_scaled", (int)(alpha_ * 1000));
    alpha_ = (float)alpha_scaled / 1000.0f;
    calib_.valid = true;
    filter_initialized_ = false;
    last_us_ = 0;
    ESP_LOGI(TAG_MPU, "Calib loaded: a_off(%.3f,%.3f,%.3f) g_bias(%.3f,%.3f,%.3f) alpha=%.2f",
             calib_.ax_off, calib_.ay_off, calib_.az_off, calib_.gx_bias, calib_.gy_bias, calib_.gz_bias, alpha_);
    return true;
}

void Mpu6050Sensor::SetFilterAlpha(float a) {
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    alpha_ = a;
}
