/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "mpu6050.h"
#include <freertos/FreeRTOS.h>

#define ALPHA 0.99f             /*!< Weight of gyroscope */
#define RAD_TO_DEG 57.27272727f /*!< Radians to degrees */

/* MPU6050 register */
#define MPU6050_GYRO_CONFIG 0x1Bu
#define MPU6050_ACCEL_CONFIG 0x1Cu
#define MPU6050_INTR_PIN_CFG 0x37u
#define MPU6050_INTR_ENABLE 0x38u
#define MPU6050_INTR_THRESHOLD 0x1Fu
#define MPU6050_INTR_DURATION 0x20u
#define MPU6050_INTR_STATUS 0x3Au
#define MPU6050_ACCEL_XOUT_H 0x3Bu
#define MPU6050_GYRO_XOUT_H 0x43u
#define MPU6050_TEMP_XOUT_H 0x41u
#define MPU6050_USER_CTRL 0x6Au
#define MPU6050_PWR_MGMT_1 0x6Bu
#define MPU6050_PWR_MGMT_2 0x6Cu
#define MPU6050_WHO_AM_I 0x75u
#define MPU6050_SAMPLE_RATE 0x19u
#define MPU6050_CFG_REG 0X1Au
#define MPU6050_FIFO_EN 0X23u

const uint8_t MPU6050_DATA_RDY_INT_BIT = (uint8_t)BIT0;
const uint8_t MPU6050_I2C_MASTER_INT_BIT = (uint8_t)BIT3;
const uint8_t MPU6050_FIFO_OVERFLOW_INT_BIT = (uint8_t)BIT4;
const uint8_t MPU6050_MOT_DETECT_INT_BIT = (uint8_t)BIT6;
const uint8_t MPU6050_ALL_INTERRUPTS = (MPU6050_DATA_RDY_INT_BIT | MPU6050_I2C_MASTER_INT_BIT | MPU6050_FIFO_OVERFLOW_INT_BIT | MPU6050_MOT_DETECT_INT_BIT);

typedef struct
{
    i2c_bus_device_handle_t i2c_dev;
    gpio_num_t int_pin;
    uint16_t dev_addr;
    uint32_t counter;
    float dt; /*!< delay time between two measurements, dt should be small (ms level) */
    struct timeval *timer;
} mpu6050_dev_t;

static esp_err_t mpu6050_write(mpu6050_handle_t sensor, const uint8_t reg_start_addr, const uint8_t *const data_buf, uint8_t data_len)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *)sensor;
    esp_err_t ret;

    ret = i2c_bus_write_bytes(sens->i2c_dev, reg_start_addr, data_len, data_buf);

    return ret;
}

static esp_err_t mpu6050_read(mpu6050_handle_t sensor, const uint8_t reg_start_addr, uint8_t *const data_buf, const uint8_t data_len)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *)sensor;
    esp_err_t ret;

    ret = i2c_bus_read_bytes(sens->i2c_dev, reg_start_addr, data_len, data_buf);

    return ret;
}

mpu6050_handle_t mpu6050_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *)calloc(1, sizeof(mpu6050_dev_t));
    sens->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sens->i2c_dev == NULL)
    {
        free(sens);
        return NULL;
    }
    sens->dev_addr = dev_addr;
    sens->counter = 0;
    sens->dt = 0;
    sens->timer = (struct timeval *)calloc(1, sizeof(struct timeval));
    return (mpu6050_handle_t)sens;
}

void mpu6050_delete(mpu6050_handle_t sensor)
{
    mpu6050_dev_t *sens = (mpu6050_dev_t *)sensor;
    free(sens);
}

esp_err_t mpu6050_get_deviceid(mpu6050_handle_t sensor, uint8_t *const deviceid)
{
    return mpu6050_read(sensor, MPU6050_WHO_AM_I, deviceid, 1);
}

esp_err_t mpu6050_wake_up(mpu6050_handle_t sensor)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = mpu6050_read(sensor, MPU6050_PWR_MGMT_1, &tmp, 1);
    if (ESP_OK != ret)
    {
        return ret;
    }
    tmp &= (~BIT6);
    ret = mpu6050_write(sensor, MPU6050_PWR_MGMT_1, &tmp, 1);
    return ret;
}

esp_err_t mpu6050_sleep(mpu6050_handle_t sensor)
{
    esp_err_t ret;
    uint8_t tmp;
    ret = mpu6050_read(sensor, MPU6050_PWR_MGMT_1, &tmp, 1);
    if (ESP_OK != ret)
    {
        return ret;
    }
    tmp |= BIT6;
    ret = mpu6050_write(sensor, MPU6050_PWR_MGMT_1, &tmp, 1);
    return ret;
}

esp_err_t mpu6050_init(mpu6050_handle_t sensor, const mpu6050_acce_fs_t acce_fs, const mpu6050_gyro_fs_t gyro_fs)
{
    esp_err_t ret;
    uint8_t val = 0x80;
    ret = mpu6050_write(sensor, MPU6050_PWR_MGMT_1, &val, 1);
    if (ESP_OK != ret)
    {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    val = 0x00;
    ret = mpu6050_write(sensor, MPU6050_PWR_MGMT_1, &val, 1);
    if (ESP_OK != ret)
    {
        return ret;
    }
    ret = mpu6050_config(sensor, acce_fs, gyro_fs);
    if (ESP_OK != ret)
    {
        return ret;
    }
    uint16_t rate = 50;
    val = 1000 / rate - 1;
    ret = mpu6050_write(sensor, MPU6050_SAMPLE_RATE, &val, 1);
    if (ESP_OK != ret)
    {
        return ret;
    }
    uint16_t lpf = rate / 2;
    val = 0;
    if (lpf >= 188)
        val = 1;
    else if (lpf >= 98)
        val = 2;
    else if (lpf >= 42)
        val = 3;
    else if (lpf >= 20)
        val = 4;
    else if (lpf >= 10)
        val = 5;
    else
        val = 6;
    ret = mpu6050_write(sensor, MPU6050_CFG_REG, &val, 1);
    if (ESP_OK != ret)
    {
        return ret;
    }
    val = 0;
    ret = mpu6050_write(sensor, MPU6050_INTR_ENABLE, &val, 1);
    if (ESP_OK != ret)
    {
        return ret;
    }
    val = 0;
    ret = mpu6050_write(sensor, MPU6050_USER_CTRL, &val, 1);
    if (ESP_OK != ret)
    {
        return ret;
    }
    val = 0;
    ret = mpu6050_write(sensor, MPU6050_FIFO_EN, &val, 1);
    if (ESP_OK != ret)
    {
        return ret;
    }
    val = 0x80;
    ret = mpu6050_write(sensor, MPU6050_INTR_PIN_CFG, &val, 1);
    if (ESP_OK != ret)
    {
        return ret;
    }
    ret = mpu6050_get_deviceid(sensor, &val);
    if (ESP_OK != ret)
    {
        return ret;
    }
    if(val == MPU6050_WHO_AM_I_VAL)
    {
        val = 0x01;
        ret = mpu6050_write(sensor, MPU6050_PWR_MGMT_1, &val, 1);
        if (ESP_OK != ret)
        {
            return ret;
        }
        val = 0x00;
        ret = mpu6050_write(sensor, MPU6050_PWR_MGMT_2, &val, 1);
        if (ESP_OK != ret)
        {
            return ret;
        }

        uint16_t rate = 100;
        val = 1000 / rate - 1;
        ret = mpu6050_write(sensor, MPU6050_SAMPLE_RATE, &val, 1);
        if (ESP_OK != ret)
        {
            return ret;
        }
        uint16_t lpf = rate / 2;
        val = 0;
        if (lpf >= 188)
            val = 1;
        else if (lpf >= 98)
            val = 2;
        else if (lpf >= 42)
            val = 3;
        else if (lpf >= 20)
            val = 4;
        else if (lpf >= 10)
            val = 5;
        else
            val = 6;
        ret = mpu6050_write(sensor, MPU6050_CFG_REG, &val, 1);
        if (ESP_OK != ret)
        {
            return ret;
        }
    }
    return ret;
}

esp_err_t mpu6050_config(mpu6050_handle_t sensor, const mpu6050_acce_fs_t acce_fs, const mpu6050_gyro_fs_t gyro_fs)
{
    uint8_t config_regs[2] = {(uint8_t)(gyro_fs << 3), (uint8_t)(acce_fs << 3)};
    return mpu6050_write(sensor, MPU6050_GYRO_CONFIG, config_regs, sizeof(config_regs));
}

esp_err_t mpu6050_get_acce_sensitivity(mpu6050_handle_t sensor, float *const acce_sensitivity)
{
    esp_err_t ret;
    uint8_t acce_fs;
    ret = mpu6050_read(sensor, MPU6050_ACCEL_CONFIG, &acce_fs, 1);
    acce_fs = (acce_fs >> 3) & 0x03;
    switch (acce_fs)
    {
    case ACCE_FS_2G:
        *acce_sensitivity = 16384;
        break;

    case ACCE_FS_4G:
        *acce_sensitivity = 8192;
        break;

    case ACCE_FS_8G:
        *acce_sensitivity = 4096;
        break;

    case ACCE_FS_16G:
        *acce_sensitivity = 2048;
        break;

    default:
        break;
    }
    return ret;
}

esp_err_t mpu6050_get_gyro_sensitivity(mpu6050_handle_t sensor, float *const gyro_sensitivity)
{
    esp_err_t ret;
    uint8_t gyro_fs;
    ret = mpu6050_read(sensor, MPU6050_GYRO_CONFIG, &gyro_fs, 1);
    gyro_fs = (gyro_fs >> 3) & 0x03;
    switch (gyro_fs)
    {
    case GYRO_FS_250DPS:
        *gyro_sensitivity = 131;
        break;

    case GYRO_FS_500DPS:
        *gyro_sensitivity = 65.5;
        break;

    case GYRO_FS_1000DPS:
        *gyro_sensitivity = 32.8;
        break;

    case GYRO_FS_2000DPS:
        *gyro_sensitivity = 16.4;
        break;

    default:
        break;
    }
    return ret;
}

esp_err_t mpu6050_config_interrupts(mpu6050_handle_t sensor, const mpu6050_int_config_t *const interrupt_configuration)
{
    esp_err_t ret = ESP_OK;

    if (NULL == interrupt_configuration)
    {
        ret = ESP_ERR_INVALID_ARG;
        return ret;
    }

    if (GPIO_IS_VALID_GPIO(interrupt_configuration->interrupt_pin))
    {
        // Set GPIO connected to MPU6050 INT pin only when user configures interrupts.
        mpu6050_dev_t *sensor_device = (mpu6050_dev_t *)sensor;
        sensor_device->int_pin = interrupt_configuration->interrupt_pin;
    }
    else
    {
        ret = ESP_ERR_INVALID_ARG;
        return ret;
    }

    uint8_t int_pin_cfg = 0x00;

    ret = mpu6050_read(sensor, MPU6050_INTR_PIN_CFG, &int_pin_cfg, 1);

    if (ESP_OK != ret)
    {
        return ret;
    }

    if (INTERRUPT_PIN_ACTIVE_LOW == interrupt_configuration->active_level)
    {
        int_pin_cfg |= BIT7;
    }

    if (INTERRUPT_PIN_OPEN_DRAIN == interrupt_configuration->pin_mode)
    {
        int_pin_cfg |= BIT6;
    }

    if (INTERRUPT_LATCH_UNTIL_CLEARED == interrupt_configuration->interrupt_latch)
    {
        int_pin_cfg |= BIT5;
    }

    if (INTERRUPT_CLEAR_ON_ANY_READ == interrupt_configuration->interrupt_clear_behavior)
    {
        int_pin_cfg |= BIT4;
    }

    ret = mpu6050_write(sensor, MPU6050_INTR_PIN_CFG, &int_pin_cfg, 1);

    if (ESP_OK != ret)
    {
        return ret;
    }

    gpio_int_type_t gpio_intr_type;

    if (INTERRUPT_PIN_ACTIVE_LOW == interrupt_configuration->active_level)
    {
        gpio_intr_type = GPIO_INTR_NEGEDGE;
    }
    else
    {
        gpio_intr_type = GPIO_INTR_POSEDGE;
    }

    gpio_config_t int_gpio_config = {
        .pin_bit_mask = (uint64_t)(BIT0 << interrupt_configuration->interrupt_pin),
        .mode = GPIO_MODE_INPUT,
        .intr_type = gpio_intr_type};

    ret = gpio_config(&int_gpio_config);

    return ret;
}

esp_err_t mpu6050_register_isr(mpu6050_handle_t sensor, const mpu6050_isr_t isr)
{
    esp_err_t ret;
    mpu6050_dev_t *sensor_device = (mpu6050_dev_t *)sensor;

    if (NULL == sensor_device)
    {
        ret = ESP_ERR_INVALID_ARG;
        return ret;
    }

    ret = gpio_isr_handler_add(
        sensor_device->int_pin,
        ((gpio_isr_t) * (isr)),
        ((void *)sensor));

    if (ESP_OK != ret)
    {
        return ret;
    }

    ret = gpio_intr_enable(sensor_device->int_pin);

    return ret;
}

esp_err_t mpu6050_enable_interrupts(mpu6050_handle_t sensor, uint8_t interrupt_sources)
{
    esp_err_t ret;
    uint8_t enabled_interrupts = 0x00;

    ret = mpu6050_read(sensor, MPU6050_INTR_ENABLE, &enabled_interrupts, 1);

    if (ESP_OK != ret)
    {
        return ret;
    }

    if (enabled_interrupts != interrupt_sources)
    {

        enabled_interrupts |= interrupt_sources;

        ret = mpu6050_write(sensor, MPU6050_INTR_ENABLE, &enabled_interrupts, 1);
    }

    return ret;
}

esp_err_t mpu6050_disable_interrupts(mpu6050_handle_t sensor, uint8_t interrupt_sources)
{
    esp_err_t ret;
    uint8_t enabled_interrupts = 0x00;

    ret = mpu6050_read(sensor, MPU6050_INTR_ENABLE, &enabled_interrupts, 1);

    if (ESP_OK != ret)
    {
        return ret;
    }

    if (0 != (enabled_interrupts & interrupt_sources))
    {
        enabled_interrupts &= (~interrupt_sources);

        ret = mpu6050_write(sensor, MPU6050_INTR_ENABLE, &enabled_interrupts, 1);
    }

    return ret;
}

esp_err_t mpu6050_get_interrupt_status(mpu6050_handle_t sensor, uint8_t *const out_intr_status)
{
    esp_err_t ret;

    if (NULL == out_intr_status)
    {
        ret = ESP_ERR_INVALID_ARG;
        return ret;
    }

    ret = mpu6050_read(sensor, MPU6050_INTR_STATUS, out_intr_status, 1);

    return ret;
}

inline uint8_t mpu6050_is_data_ready_interrupt(uint8_t interrupt_status)
{
    return (MPU6050_DATA_RDY_INT_BIT == (MPU6050_DATA_RDY_INT_BIT & interrupt_status));
}

inline uint8_t mpu6050_is_i2c_master_interrupt(uint8_t interrupt_status)
{
    return (uint8_t)(MPU6050_I2C_MASTER_INT_BIT == (MPU6050_I2C_MASTER_INT_BIT & interrupt_status));
}

inline uint8_t mpu6050_is_fifo_overflow_interrupt(uint8_t interrupt_status)
{
    return (uint8_t)(MPU6050_FIFO_OVERFLOW_INT_BIT == (MPU6050_FIFO_OVERFLOW_INT_BIT & interrupt_status));
}

esp_err_t mpu6050_get_raw_acce(mpu6050_handle_t sensor, mpu6050_raw_acce_value_t *const raw_acce_value)
{
    uint8_t data_rd[6];
    esp_err_t ret = mpu6050_read(sensor, MPU6050_ACCEL_XOUT_H, data_rd, sizeof(data_rd));

    raw_acce_value->raw_acce_x = (int16_t)((data_rd[0] << 8) + (data_rd[1]));
    raw_acce_value->raw_acce_y = (int16_t)((data_rd[2] << 8) + (data_rd[3]));
    raw_acce_value->raw_acce_z = (int16_t)((data_rd[4] << 8) + (data_rd[5]));
    return ret;
}

esp_err_t mpu6050_get_raw_gyro(mpu6050_handle_t sensor, mpu6050_raw_gyro_value_t *const raw_gyro_value)
{
    uint8_t data_rd[6];
    esp_err_t ret = mpu6050_read(sensor, MPU6050_GYRO_XOUT_H, data_rd, sizeof(data_rd));

    raw_gyro_value->raw_gyro_x = (int16_t)((data_rd[0] << 8) + (data_rd[1]));
    raw_gyro_value->raw_gyro_y = (int16_t)((data_rd[2] << 8) + (data_rd[3]));
    raw_gyro_value->raw_gyro_z = (int16_t)((data_rd[4] << 8) + (data_rd[5]));

    return ret;
}

esp_err_t mpu6050_get_acce(mpu6050_handle_t sensor, mpu6050_acce_value_t *const acce_value)
{
    esp_err_t ret;
    float acce_sensitivity;
    mpu6050_raw_acce_value_t raw_acce;

    ret = mpu6050_get_acce_sensitivity(sensor, &acce_sensitivity);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = mpu6050_get_raw_acce(sensor, &raw_acce);
    if (ret != ESP_OK)
    {
        return ret;
    }

    acce_value->acce_x = raw_acce.raw_acce_x / acce_sensitivity;
    acce_value->acce_y = raw_acce.raw_acce_y / acce_sensitivity;
    acce_value->acce_z = raw_acce.raw_acce_z / acce_sensitivity;
    return ESP_OK;
}

esp_err_t mpu6050_get_gyro(mpu6050_handle_t sensor, mpu6050_gyro_value_t *const gyro_value)
{
    esp_err_t ret;
    float gyro_sensitivity;
    mpu6050_raw_gyro_value_t raw_gyro;

    ret = mpu6050_get_gyro_sensitivity(sensor, &gyro_sensitivity);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = mpu6050_get_raw_gyro(sensor, &raw_gyro);
    if (ret != ESP_OK)
    {
        return ret;
    }

    gyro_value->gyro_x = raw_gyro.raw_gyro_x / gyro_sensitivity;
    gyro_value->gyro_y = raw_gyro.raw_gyro_y / gyro_sensitivity;
    gyro_value->gyro_z = raw_gyro.raw_gyro_z / gyro_sensitivity;
    return ESP_OK;
}

esp_err_t mpu6050_get_temp(mpu6050_handle_t sensor, mpu6050_temp_value_t *const temp_value)
{
    uint8_t data_rd[2];
    esp_err_t ret = mpu6050_read(sensor, MPU6050_TEMP_XOUT_H, data_rd, sizeof(data_rd));
    temp_value->temp = (int16_t)((data_rd[0] << 8) | (data_rd[1])) / 340.00 + 36.53;
    return ret;
}

esp_err_t mpu6050_complimentory_filter(mpu6050_handle_t sensor, const mpu6050_acce_value_t *const acce_value,
                                       const mpu6050_gyro_value_t *const gyro_value, complimentary_angle_t *const complimentary_angle)
{
    float acce_angle[2];
    float gyro_angle[2];
    float gyro_rate[2];
    mpu6050_dev_t *sens = (mpu6050_dev_t *)sensor;

    sens->counter++;
    if (sens->counter == 1)
    {
        acce_angle[0] = (atan2(acce_value->acce_y, acce_value->acce_z) * RAD_TO_DEG);
        acce_angle[1] = (atan2(acce_value->acce_x, acce_value->acce_z) * RAD_TO_DEG);
        complimentary_angle->roll = acce_angle[0];
        complimentary_angle->pitch = acce_angle[1];
        gettimeofday(sens->timer, NULL);
        return ESP_OK;
    }

    struct timeval now, dt_t;
    gettimeofday(&now, NULL);
    timersub(&now, sens->timer, &dt_t);
    sens->dt = (float)(dt_t.tv_sec) + (float)dt_t.tv_usec / 1000000;
    gettimeofday(sens->timer, NULL);

    acce_angle[0] = (atan2(acce_value->acce_y, acce_value->acce_z) * RAD_TO_DEG);
    acce_angle[1] = (atan2(acce_value->acce_x, acce_value->acce_z) * RAD_TO_DEG);

    gyro_rate[0] = gyro_value->gyro_x;
    gyro_rate[1] = gyro_value->gyro_y;
    gyro_angle[0] = gyro_rate[0] * sens->dt;
    gyro_angle[1] = gyro_rate[1] * sens->dt;

    complimentary_angle->roll = (ALPHA * (complimentary_angle->roll + gyro_angle[0])) + ((1 - ALPHA) * acce_angle[0]);
    complimentary_angle->pitch = (ALPHA * (complimentary_angle->pitch + gyro_angle[1])) + ((1 - ALPHA) * acce_angle[1]);

    return ESP_OK;
}

esp_err_t mpu6050_enable_motiondetection(mpu6050_handle_t sensor, uint8_t threshold, uint8_t duration)
{
    esp_err_t ret;
    uint8_t enabled_interrupts = 0x01;

    uint8_t data = 0b00000000;
    ret = mpu6050_write(sensor, 0x6B, &data, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    data = 0b11000111;
    ret = mpu6050_write(sensor, 0x6C, &data, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    data = 0b00000000;
    ret = mpu6050_write(sensor, 0x1C, &data, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    data = 0b00000000;
    ret = mpu6050_write(sensor, 0x1A, &data, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    data = 0b11000000;
    ret = mpu6050_write(sensor, 0x38, &data, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    data = 0b00000001;
    ret = mpu6050_write(sensor, 0x20, &data, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    data = 0b00000100;
    ret = mpu6050_write(sensor, 0x1F, &data, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    data = 0b00000111;
    ret = mpu6050_write(sensor, 0x1C, &data, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    data = 0b01000111;
    ret = mpu6050_write(sensor, 0x6C, &data, 1);
    if (ret != ESP_OK) {
        return ret;
    }
    data = 0b00100000;
    ret = mpu6050_write(sensor, 0x6B, &data, 1);
    if (ret != ESP_OK) {
        return ret;
    }

    return ret;
}