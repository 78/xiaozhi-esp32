/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BMP280_H_
#define _BMP280_H_

#include "i2c_bus.h"

#define BMP280_I2C_ADDRESS_DEFAULT   (0x76)     /*The device's I2C address is either 0x76 or 0x77.*/
#define BMP280_DEFAULT_CHIPID        (0x58)

#define WRITE_BIT      I2C_MASTER_WRITE         /*!< I2C master write */
#define READ_BIT       I2C_MASTER_READ          /*!< I2C master read */
#define ACK_CHECK_EN   0x1                      /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0                      /*!< I2C master will not check ack from slave */
#define ACK_VAL        0x0                      /*!< I2C ack value */
#define NACK_VAL       0x1                      /*!< I2C nack value */

#define BMP280_REGISTER_DIG_T1              0x88
#define BMP280_REGISTER_DIG_T2              0x8A
#define BMP280_REGISTER_DIG_T3              0x8C

#define BMP280_REGISTER_DIG_P1              0x8E
#define BMP280_REGISTER_DIG_P2              0x90
#define BMP280_REGISTER_DIG_P3              0x92
#define BMP280_REGISTER_DIG_P4              0x94
#define BMP280_REGISTER_DIG_P5              0x96
#define BMP280_REGISTER_DIG_P6              0x98
#define BMP280_REGISTER_DIG_P7              0x9A
#define BMP280_REGISTER_DIG_P8              0x9C
#define BMP280_REGISTER_DIG_P9              0x9E

#define BMP280_REGISTER_DIG_H1              0xA1
#define BMP280_REGISTER_DIG_H2              0xE1
#define BMP280_REGISTER_DIG_H3              0xE3
#define BMP280_REGISTER_DIG_H4              0xE4
#define BMP280_REGISTER_DIG_H5              0xE5
#define BMP280_REGISTER_DIG_H6              0xE7

#define BMP280_REGISTER_CHIPID              0xD0
#define BMP280_REGISTER_VERSION             0xD1
#define BMP280_REGISTER_SOFTRESET           0xE0

#define BMP280_REGISTER_CAL26               0xE1  // R calibration stored in 0xE1-0xF0

#define BMP280_REGISTER_CONTROLHUMID        0xF2
#define BMP280_REGISTER_STATUS              0XF3
#define BMP280_REGISTER_CONTROL             0xF4
#define BMP280_REGISTER_CONFIG              0xF5
#define BMP280_REGISTER_PRESSUREDATA        0xF7
#define BMP280_REGISTER_TEMPDATA            0xFA
#define BMP280_REGISTER_HUMIDDATA           0xFD

typedef struct {
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;

    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;

    uint8_t dig_h1;
    int16_t dig_h2;
    uint8_t dig_h3;
    int16_t dig_h4;
    int16_t dig_h5;
    int8_t dig_h6;
} bmp280_data_t;

typedef enum {
    BMP280_SAMPLING_NONE = 0b000,
    BMP280_SAMPLING_X1 = 0b001,
    BMP280_SAMPLING_X2 = 0b010,
    BMP280_SAMPLING_X4 = 0b011,
    BMP280_SAMPLING_X8 = 0b100,
    BMP280_SAMPLING_X16 = 0b101
} bmp280_sensor_sampling;

typedef enum {
    BMP280_MODE_SLEEP = 0b00,
    BMP280_MODE_FORCED = 0b01,
    BMP280_MODE_NORMAL = 0b11
} bmp280_sensor_mode;

typedef enum {
    BMP280_FILTER_OFF = 0b000,
    BMP280_FILTER_X2 = 0b001,
    BMP280_FILTER_X4 = 0b010,
    BMP280_FILTER_X8 = 0b011,
    BMP280_FILTER_X16 = 0b100
} bmp280_sensor_filter;

// standby durations in ms
typedef enum {
    BMP280_STANDBY_MS_0_5 = 0b000,
    BMP280_STANDBY_MS_10 = 0b110,
    BMP280_STANDBY_MS_20 = 0b111,
    BMP280_STANDBY_MS_62_5 = 0b001,
    BMP280_STANDBY_MS_125 = 0b010,
    BMP280_STANDBY_MS_250 = 0b011,
    BMP280_STANDBY_MS_500 = 0b100,
    BMP280_STANDBY_MS_1000 = 0b101
} bmp280_standby_duration;

// The config register
typedef struct config {
    // inactive duration (standby time) in normal mode
    // 000 = 0.5 ms
    // 001 = 62.5 ms
    // 010 = 125 ms
    // 011 = 250 ms
    // 100 = 500 ms
    // 101 = 1000 ms
    // 110 = 10 ms
    // 111 = 20 ms
    unsigned int t_sb : 3;

    // filter settings
    // 000 = filter off
    // 001 = 2x filter
    // 010 = 4x filter
    // 011 = 8x filter
    // 100 and above = 16x filter
    unsigned int filter : 3;

    // unused - don't set
    unsigned int none : 1;
    unsigned int spi3w_en : 1;
} bmp280_config_t;

// The ctrl_meas register
typedef struct ctrl_meas {
    // temperature oversampling
    // 000 = skipped
    // 001 = x1
    // 010 = x2
    // 011 = x4
    // 100 = x8
    // 101 and above = x16
    unsigned int osrs_t : 3;

    // pressure oversampling
    // 000 = skipped
    // 001 = x1
    // 010 = x2
    // 011 = x4
    // 100 = x8
    // 101 and above = x16
    unsigned int osrs_p : 3;

    // device mode
    // 00       = sleep
    // 01 or 10 = forced
    // 11       = normal
    unsigned int mode : 2;
} bmp280_ctrl_meas_t;

// The ctrl_hum register
typedef struct ctrl_hum {
    // unused - don't set
    unsigned int none : 5;

    // pressure oversampling
    // 000 = skipped
    // 001 = x1
    // 010 = x2
    // 011 = x4
    // 100 = x8
    // 101 and above = x16
    unsigned int osrs_h : 3;
} bmp280_ctrl_hum_t;

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
    bmp280_data_t data_t;
    bmp280_config_t config_t;
    bmp280_ctrl_meas_t ctrl_meas_t;
    bmp280_ctrl_hum_t ctrl_hum_t;
    int32_t t_fine;
} bmp280_dev_t;

typedef void *bmp280_handle_t; /*handle of bmp280*/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   Create bmp280 handle_t
 *
 * @param  object handle of I2C
 * @param  device address
 *
 * @return
 *     - bmp280_handle_t
 */
bmp280_handle_t bmp280_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief   delete bmp280 handle_t
 *
 * @param  point to object handle of bmp280
 * @param  whether delete i2c bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t bmp280_delete(bmp280_handle_t *sensor);

/**
 * @brief   Get the value of BMP280_REGISTER_CONFIG register
 *
 * @param  sensor object handle of bmp280
 *
 * @return
 *    - unsigned int: the value of BMP280_REGISTER_CONFIG register
 */
unsigned int bmp280_getconfig(bmp280_handle_t sensor);

/**
 * @brief   Get the value of BMP280_REGISTER_CONTROL measure register
 *
 * @param  sensor object handle of bmp280
 *
 * @return
 *    - unsigned int the value of BMP280_REGISTER_CONTROL register
 */
unsigned int bmp280_getctrl_meas(bmp280_handle_t sensor);

/**
 * @brief   Get the value of BMP280_REGISTER_CONTROLHUMID measure register
 *
 * @param  sensor object handle of bmp280
 *
 * @return
 *    - unsigned int the value of BMP280_REGISTER_CONTROLHUMID register
 */
unsigned int bmp280_getctrl_hum(bmp280_handle_t sensor);

/**
 * @brief return true if chip is busy reading cal data
 *
 * @param  sensor object handle of bmp280
 *
 * @return
 *    - true chip is busy
 *    - false chip is idle or wrong
 */
bool bmp280_is_reading_calibration(bmp280_handle_t sensor);

/**
 * @brief Reads the factory-set coefficients
 *
 * @param  sensor object handle of bmp280
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t bmp280_read_coefficients(bmp280_handle_t sensor);

/**
 * @brief  setup sensor with gien parameters / settings
 *
 * @param  sensor object handle of bmp280
 * @param  Sensor working mode
 * @param  the sample of temperature measure
 * @param  the sample of pressure measure
 * @param  the sample of humidity measure
 * @param  Sensor filter multiples
 * @param  standby duration of sensor
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t bmp280_set_sampling(bmp280_handle_t sensor, bmp280_sensor_mode mode,
                              bmp280_sensor_sampling tempsampling,
                              bmp280_sensor_sampling presssampling,
                              bmp280_sensor_sampling humsampling, bmp280_sensor_filter filter,
                              bmp280_standby_duration duration);

/**
 * @brief init bmp280 device
 *
 * @param sensor object handle of bmp280
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t bmp280_default_init(bmp280_handle_t sensor);

/**
 * @brief  Take a new measurement (only possible in forced mode)
 * If we are in forced mode, the BMP sensor goes back to sleep after each
 * measurement and we need to set it to forced mode once at this point, so
 * it will take the next measurement and then return to sleep again.
 * In normal mode simply does new measurements periodically.
 *
 * @param  sensor object handle of bmp280
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t bmp280_take_forced_measurement(bmp280_handle_t sensor);

/**
 * @brief  Returns the temperature from the sensor
 *
 * @param sensor object handle of bmp280
 * @param temperature pointer to temperature
 * @return esp_err_t
 */
esp_err_t bmp280_read_temperature(bmp280_handle_t sensor, float *temperature);

/**
 * @brief  Returns the temperature from the sensor
 *
 * @param sensor object handle of bmp280
 * @param pressure pointer to pressure value
 * @return esp_err_t
 */
esp_err_t bmp280_read_pressure(bmp280_handle_t sensor, float *pressure);

/**
 * @brief  Returns the humidity from the sensor
 *
 * @param sensor object handle of bmp280
 * @param humidity pointer to humidity value
 * @return esp_err_t
 */
esp_err_t bmp280_read_humidity(bmp280_handle_t sensor, float *humidity);

/**
 * @brief Calculates the altitude (in meters) from the specified atmospheric
 *  pressure (in hPa), and sea-level pressure (in hPa).
 *
 * @param sensor object handle of bmp280
 * @param seaLevel: Sea-level pressure in hPa
 * @param altitude pointer to altitude value
 * @return esp_err_t
 */
esp_err_t bmp280_read_altitude(bmp280_handle_t sensor, float seaLevel, float *altitude);

/**
 * Calculates the pressure at sea level (in hPa) from the specified altitude
 * (in meters), and atmospheric pressure (in hPa).
 *
 * @param sensor object handle of bmp280
 * @param altitude      Altitude in meters
 * @param atmospheric   Atmospheric pressure in hPa
 * @param pressure pointer to pressure value
 * @return esp_err_t
 */
esp_err_t bmp280_calculates_pressure(bmp280_handle_t sensor, float altitude,
                                     float atmospheric, float *pressure);

#ifdef __cplusplus
}
#endif

#endif
