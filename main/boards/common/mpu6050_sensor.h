#pragma once

#include <driver/i2c_master.h>
#include <cstdint>

class Mpu6050Sensor {
public:
    struct Sample {
        float ax, ay, az; // g
        float gx, gy, gz; // deg/s
        float pitch, roll; // degrees
    };

    // addr usually 0x68 (AD0 low) or 0x69 (AD0 high)
    Mpu6050Sensor(i2c_port_t port, gpio_num_t sda, gpio_num_t scl, uint8_t addr = 0x68, int hz = 400000);
    ~Mpu6050Sensor();

    bool Initialize();
    bool Read(Sample &out);
    // Read with bias correction and complementary filter on pitch/roll
    bool ReadFiltered(Sample &out);

    // Calibrate by sampling while the device is steady on a flat surface
    // - Gyro biases are measured (should be ~0 deg/s when still)
    // - Accel offsets are measured so that az ~= +1g when face-up
    bool Calibrate(int samples = 300, int sample_delay_ms = 5);

    // Save/Load calibration to NVS under namespace "imu"
    bool SaveCalibration();
    bool LoadCalibration();

    void SetFilterAlpha(float a); // 0..1, higher trusts gyro more

private:
    i2c_port_t port_;
    gpio_num_t sda_;
    gpio_num_t scl_;
    uint8_t addr_;
    int hz_;
    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;

    // Calibration / filtering state
    struct Calibration {
        float ax_off = 0.0f, ay_off = 0.0f, az_off = 0.0f; // g offsets
        float gx_bias = 0.0f, gy_bias = 0.0f, gz_bias = 0.0f; // deg/s biases
        bool valid = false;
    } calib_;
    float alpha_ = 0.98f; // complementary filter coefficient
    bool filter_initialized_ = false;
    float pitch_filt_ = 0.0f, roll_filt_ = 0.0f; // degrees
    int64_t last_us_ = 0; // for dt integration

    bool WriteReg(uint8_t reg, uint8_t val);
    bool ReadRegs(uint8_t reg, uint8_t *buf, size_t len);
};
