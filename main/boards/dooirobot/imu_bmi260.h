/**

@file imu_bmi260.h

@brief BMI260 IMU 纯硬件驱动层
*/
#ifndef IMU_BMI260_H
#define IMU_BMI260_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================

BMI260 寄存器地址

================================================================ */
#define BMI260_REG_CHIP_ID 0x00
#define BMI260_CHIP_ID_VAL 0x27

#define BMI260_REG_ERR_REG 0x02
#define BMI260_REG_STATUS 0x03

#define BMI260_REG_DATA_ACCEL_XOUT_L 0x0C
#define BMI260_REG_DATA_GYRO_XOUT_L 0x12

#define BMI260_REG_SENSORTIME_0 0x18
#define BMI260_REG_EVENT 0x1B
#define BMI260_REG_INT_STATUS_0 0x1C
#define BMI260_REG_INT_STATUS_1 0x1D
#define BMI260_REG_INTERNAL_STATUS 0x21

#define BMI260_REG_ACCEL_CONFIG 0x40
#define BMI260_REG_ACCEL_RANGE 0x41
#define BMI260_REG_GYRO_CONFIG 0x42
#define BMI260_REG_GYRO_RANGE 0x43

#define BMI260_REG_INT1_IO_CTRL 0x53
#define BMI260_REG_INT2_IO_CTRL 0x54
#define BMI260_REG_INT_LATCH 0x55
#define BMI260_REG_INT_MAP_DATA 0x58

#define BMI260_REG_INIT_CTRL 0x59
#define BMI260_REG_INIT_ADDR_0 0x5B
#define BMI260_REG_INIT_ADDR_1 0x5C
#define BMI260_REG_INIT_DATA 0x5E

#define BMI260_REG_PWR_CONF 0x7C
#define BMI260_REG_PWR_CTRL 0x7D
#define BMI260_REG_CMD 0x7E

/* ================================================================

常量定义

================================================================ */
#define BMI260_CMD_SOFTRESET 0xB6

/* I2C 地址 */
#define BMI260_I2C_ADDR_PRIMARY 0x68
#define BMI260_I2C_ADDR_SECONDARY 0x69

/* INT1_IO_CTRL 位定义 */
#define BMI260_INT_EDGE_TRIGGERED (1 << 0)
#define BMI260_INT_ACTIVE_HIGH (1 << 1)
#define BMI260_INT_OPEN_DRAIN (1 << 2)
#define BMI260_INT_OUTPUT_EN (1 << 3)
#define BMI260_INT_INPUT_EN (1 << 4)

/* INT_MAP_DATA 位定义 */
#define BMI260_INT1_MAP_DRDY (1 << 2)

/* PWR_CTRL 位定义 */
#define BMI260_PWR_CTRL_AUX_EN (1 << 0)
#define BMI260_PWR_CTRL_GYR_EN (1 << 1)
#define BMI260_PWR_CTRL_ACC_EN (1 << 2)
#define BMI260_PWR_CTRL_TEMP_EN (1 << 3)

/* ================================================================

枚举类型

================================================================ */

typedef enum {
    BMI260_ACCEL_RANGE_2G = 0x00,
    BMI260_ACCEL_RANGE_4G = 0x01,
    BMI260_ACCEL_RANGE_8G = 0x02,
    BMI260_ACCEL_RANGE_16G = 0x03,
    BMI260_ACCEL_RANGE_MAX
} bmi260_accel_range_t;

typedef enum {
    BMI260_GYRO_RANGE_2000DPS = 0x00,
    BMI260_GYRO_RANGE_1000DPS = 0x01,
    BMI260_GYRO_RANGE_500DPS = 0x02,
    BMI260_GYRO_RANGE_250DPS = 0x03,
    BMI260_GYRO_RANGE_125DPS = 0x04,
    BMI260_GYRO_RANGE_MAX
} bmi260_gyro_range_t;

typedef enum {
    BMI260_ODR_0_78HZ = 0x01,
    BMI260_ODR_1_56HZ = 0x02,
    BMI260_ODR_3_12HZ = 0x03,
    BMI260_ODR_6_25HZ = 0x04,
    BMI260_ODR_12_5HZ = 0x05,
    BMI260_ODR_25HZ = 0x06,
    BMI260_ODR_50HZ = 0x07,
    BMI260_ODR_100HZ = 0x08,
    BMI260_ODR_200HZ = 0x09,
    BMI260_ODR_400HZ = 0x0A,
    BMI260_ODR_800HZ = 0x0B,
    BMI260_ODR_1600HZ = 0x0C,
} bmi260_odr_t;

/* ================================================================

数据结构

================================================================ */

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int64_t timestamp_us;
} bmi260_raw_data_t;

typedef struct {
    float ax, ay, az;  // 单位: g
    float gx, gy, gz;  // 单位: degree/s
    int64_t timestamp_us;
} bmi260_phys_data_t;

typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    uint8_t i2c_addr;
    int int_pin;
    bmi260_accel_range_t accel_range;
    bmi260_gyro_range_t gyro_range;
    bmi260_odr_t accel_odr;
    bmi260_odr_t gyro_odr;
} bmi260_config_t;

typedef void (*bmi260_data_cb_t)(const bmi260_raw_data_t* raw, const bmi260_phys_data_t* phys,
                                 void* arg);

/* ================================================================

公开 API

================================================================ */

esp_err_t bmi260_init(const bmi260_config_t* config);
esp_err_t bmi260_register_data_callback(bmi260_data_cb_t cb, void* arg);
esp_err_t bmi260_start_task(uint8_t task_priority, uint32_t stack_size);
esp_err_t bmi260_read_sync(bmi260_raw_data_t* raw, bmi260_phys_data_t* phys, uint32_t timeout_ms);
void bmi260_convert(const bmi260_raw_data_t* raw, bmi260_phys_data_t* phys);
esp_err_t bmi260_set_accel_range(bmi260_accel_range_t range);
esp_err_t bmi260_set_gyro_range(bmi260_gyro_range_t range);
bmi260_accel_range_t bmi260_get_accel_range(void);
bmi260_gyro_range_t bmi260_get_gyro_range(void);
bool bmi260_is_initialized(void);

/**

@brief 设置电源状态（节约功耗）

@param enable true为唤醒传感器，false为睡眠
*/
esp_err_t bmi260_set_power(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* IMU_BMI260_H */