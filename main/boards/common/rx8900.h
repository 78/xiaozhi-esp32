/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _RX8900_H_
#define _RX8900_H_

#include "i2c_bus.h"
#include <time.h>

#define RX8900_I2C_ADDRESS_DEFAULT   (0x32)     /*The device's I2C address is either 0x76 or 0x77.*/

#define WRITE_BIT      I2C_MASTER_WRITE         /*!< I2C master write */
#define READ_BIT       I2C_MASTER_READ          /*!< I2C master read */
#define ACK_CHECK_EN   0x1                      /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS  0x0                      /*!< I2C master will not check ack from slave */
#define ACK_VAL        0x0                      /*!< I2C ack value */
#define NACK_VAL       0x1                      /*!< I2C nack value */

#define RX8900_REG_TIME         0x00
#define RX8900_REG_ALARM1       0x07
#define RX8900_REG_ALARM2       0x0B
#define RX8900_EXT_REG          0x0D
#define RX8900_REG_CONTROL      0x0F
#define RX8900_REG_STATUS       0x0E
#define RX8900_REG_TEMP         0x17

#define RX8900_CON_EOSC         0x80
#define RX8900_CON_BBSQW        0x40
#define RX8900_CON_CONV         0x20
#define RX8900_CON_RS2          0x10
#define RX8900_CON_RS1          0x08
#define RX8900_CON_INTCN        0x04
#define RX8900_CON_A2IE         0x02
#define RX8900_CON_A1IE         0x01

#define RX8900_STA_OSF          0x80
#define RX8900_STA_32KHZ        0x08
#define RX8900_STA_BSY          0x04
#define RX8900_STA_A2F          0x02
#define RX8900_STA_A1F          0x01

typedef enum {
  RX8900_WEEK_SUNDAY = 0,
  RX8900_WEEK_MONDAY,
  RX8900_WEEK_TUESDAY,
  RX8900_WEEK_WEDNESDAY,
  RX8900_WEEK_THURSDAY,
  RX8900_WEEK_FRIDAY,
  RX8900_WEEK_SATURDAY
} rx8900_sensor_week;

typedef struct {
    i2c_bus_device_handle_t i2c_dev;
    int32_t t_fine;
} rx8900_dev_t;

typedef void *rx8900_handle_t; /*handle of rx8900*/

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief   Create rx8900 handle_t
 *
 * @param  object handle of I2C
 * @param  device address
 *
 * @return
 *     - rx8900_handle_t
 */
rx8900_handle_t rx8900_create(i2c_bus_handle_t bus, uint8_t dev_addr);

/**
 * @brief   delete rx8900 handle_t
 *
 * @param  point to object handle of rx8900
 * @param  whether delete i2c bus
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t rx8900_delete(rx8900_handle_t *sensor);

/**
 * @brief init rx8900 device
 *
 * @param sensor object handle of rx8900
 *
 * @return
 *    - ESP_OK Success
 *    - ESP_FAIL Fail
 */
esp_err_t rx8900_default_init(rx8900_handle_t sensor);

/**
 * @brief  Returns the temperature from the sensor
 *
 * @param sensor object handle of rx8900
 * @param temperature pointer to temperature
 * @return esp_err_t
 */
esp_err_t rx8900_read_temperature(rx8900_handle_t sensor, float *temperature);

/**
 * @brief  Reads the time from the RX8900 sensor.
 *
 * @param sensor RX8900 sensor handle.
 * @param tm_local Pointer to store the read time.
 * @return esp_err_t indicating success (ESP_OK) or failure.
 */
esp_err_t rx8900_read_time(rx8900_handle_t sensor, struct tm *tm_local);


/**
 * @brief  Writes the time to the RX8900 sensor.
 *
 * @param sensor RX8900 sensor handle.
 * @param tm_local Pointer to the time to write.
 * @return esp_err_t indicating success (ESP_OK) or failure.
 */
esp_err_t rx8900_write_time(rx8900_handle_t sensor, struct tm *tm_local);

#ifdef __cplusplus
}
#endif

#endif
