/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief MPU6050 driver
 */

 #pragma once

 #ifdef __cplusplus
 extern "C"
 {
 #endif
 
 #include "i2c_bus.h"
 #include "driver/gpio.h"
 
 #define MPU6050_I2C_ADDRESS 0x68u   /*!< I2C address with AD0 pin low */
 #define MPU6050_I2C_ADDRESS_1 0x69u /*!< I2C address with AD0 pin high */
 #define MPU6050_WHO_AM_I_VAL 0x68u
 
     typedef enum
     {
         ACCE_FS_2G = 0,  /*!< Accelerometer full scale range is +/- 2g */
         ACCE_FS_4G = 1,  /*!< Accelerometer full scale range is +/- 4g */
         ACCE_FS_8G = 2,  /*!< Accelerometer full scale range is +/- 8g */
         ACCE_FS_16G = 3, /*!< Accelerometer full scale range is +/- 16g */
     } mpu6050_acce_fs_t;
 
     typedef enum
     {
         GYRO_FS_250DPS = 0,  /*!< Gyroscope full scale range is +/- 250 degree per sencond */
         GYRO_FS_500DPS = 1,  /*!< Gyroscope full scale range is +/- 500 degree per sencond */
         GYRO_FS_1000DPS = 2, /*!< Gyroscope full scale range is +/- 1000 degree per sencond */
         GYRO_FS_2000DPS = 3, /*!< Gyroscope full scale range is +/- 2000 degree per sencond */
     } mpu6050_gyro_fs_t;
 
     typedef enum
     {
         INTERRUPT_PIN_ACTIVE_HIGH = 0, /*!< The mpu6050 sets its INT pin HIGH on interrupt */
         INTERRUPT_PIN_ACTIVE_LOW = 1   /*!< The mpu6050 sets its INT pin LOW on interrupt */
     } mpu6050_int_pin_active_level_t;
 
     typedef enum
     {
         INTERRUPT_PIN_PUSH_PULL = 0, /*!< The mpu6050 configures its INT pin as push-pull */
         INTERRUPT_PIN_OPEN_DRAIN = 1 /*!< The mpu6050 configures its INT pin as open drain*/
     } mpu6050_int_pin_mode_t;
 
     typedef enum
     {
         INTERRUPT_LATCH_50US = 0,         /*!< The mpu6050 produces a 50 microsecond pulse on interrupt */
         INTERRUPT_LATCH_UNTIL_CLEARED = 1 /*!< The mpu6050 latches its INT pin to its active level, until interrupt is cleared */
     } mpu6050_int_latch_t;
 
     typedef enum
     {
         INTERRUPT_CLEAR_ON_ANY_READ = 0,   /*!< INT_STATUS register bits are cleared on any register read */
         INTERRUPT_CLEAR_ON_STATUS_READ = 1 /*!< INT_STATUS register bits are cleared only by reading INT_STATUS value*/
     } mpu6050_int_clear_t;
 
     typedef struct
     {
         gpio_num_t interrupt_pin;                     /*!< GPIO connected to mpu6050 INT pin       */
         mpu6050_int_pin_active_level_t active_level;  /*!< Active level of mpu6050 INT pin         */
         mpu6050_int_pin_mode_t pin_mode;              /*!< Push-pull or open drain mode for INT pin*/
         mpu6050_int_latch_t interrupt_latch;          /*!< The interrupt pulse behavior of INT pin */
         mpu6050_int_clear_t interrupt_clear_behavior; /*!< Interrupt status clear behavior         */
     } mpu6050_int_config_t;
 
     extern const uint8_t MPU6050_DATA_RDY_INT_BIT;      /*!< DATA READY interrupt bit               */
     extern const uint8_t MPU6050_I2C_MASTER_INT_BIT;    /*!< I2C MASTER interrupt bit               */
     extern const uint8_t MPU6050_FIFO_OVERFLOW_INT_BIT; /*!< FIFO Overflow interrupt bit            */
     extern const uint8_t MPU6050_MOT_DETECT_INT_BIT;    /*!< MOTION DETECTION interrupt bit         */
     extern const uint8_t MPU6050_ALL_INTERRUPTS;        /*!< All interrupts supported by mpu6050    */
 
     typedef struct
     {
         int16_t raw_acce_x;
         int16_t raw_acce_y;
         int16_t raw_acce_z;
     } mpu6050_raw_acce_value_t;
 
     typedef struct
     {
         int16_t raw_gyro_x;
         int16_t raw_gyro_y;
         int16_t raw_gyro_z;
     } mpu6050_raw_gyro_value_t;
 
     typedef struct
     {
         float acce_x;
         float acce_y;
         float acce_z;
     } mpu6050_acce_value_t;
 
     typedef struct
     {
         float gyro_x;
         float gyro_y;
         float gyro_z;
     } mpu6050_gyro_value_t;
 
     typedef struct
     {
         float temp;
     } mpu6050_temp_value_t;
 
     typedef struct
     {
         float roll;
         float pitch;
     } complimentary_angle_t;
 
     typedef void *mpu6050_handle_t;
 
     typedef gpio_isr_t mpu6050_isr_t;
 
     /**
      * @brief Create and init sensor object and return a sensor handle
      *
      * @param port I2C port number
      * @param dev_addr I2C device address of sensor
      *
      * @return
      *     - NULL Fail
      *     - Others Success
      */
     mpu6050_handle_t mpu6050_create(i2c_bus_handle_t bus, uint8_t dev_addr);
 
     /**
      * @brief Delete and release a sensor object
      *
      * @param sensor object handle of mpu6050
      */
     void mpu6050_delete(mpu6050_handle_t sensor);
 
     /**
      * @brief Get device identification of MPU6050
      *
      * @param sensor object handle of mpu6050
      * @param deviceid a pointer of device ID
      *
      * @return
      *     - ESP_OK Success
      *     - ESP_FAIL Fail
      */
     esp_err_t mpu6050_get_deviceid(mpu6050_handle_t sensor, uint8_t *const deviceid);
 
     /**
      * @brief Wake up MPU6050
      *
      * @param sensor object handle of mpu6050
      *
      * @return
      *     - ESP_OK Success
      *     - ESP_FAIL Fail
      */
     esp_err_t mpu6050_wake_up(mpu6050_handle_t sensor);
 
     /**
      * @brief Enter sleep mode
      *
      * @param sensor object handle of mpu6050
      *
      * @return
      *     - ESP_OK Success
      *     - ESP_FAIL Fail
      */
     esp_err_t mpu6050_sleep(mpu6050_handle_t sensor);
 
     /**
      * @brief Set accelerometer and gyroscope full scale range
      *
      * @param sensor object handle of mpu6050
      * @param acce_fs accelerometer full scale range
      * @param gyro_fs gyroscope full scale range
      *
      * @return
      *     - ESP_OK Success
      *     - ESP_FAIL Fail
      */
     esp_err_t mpu6050_config(mpu6050_handle_t sensor, const mpu6050_acce_fs_t acce_fs, const mpu6050_gyro_fs_t gyro_fs);
 
     /**
      * @brief Get accelerometer sensitivity
      *
      * @param sensor object handle of mpu6050
      * @param acce_sensitivity accelerometer sensitivity
      *
      * @return
      *     - ESP_OK Success
      *     - ESP_FAIL Fail
      */
     esp_err_t mpu6050_get_acce_sensitivity(mpu6050_handle_t sensor, float *const acce_sensitivity);
 
     /**
      * @brief Get gyroscope sensitivity
      *
      * @param sensor object handle of mpu6050
      * @param gyro_sensitivity gyroscope sensitivity
      *
      * @return
      *     - ESP_OK Success
      *     - ESP_FAIL Fail
      */
     esp_err_t mpu6050_get_gyro_sensitivity(mpu6050_handle_t sensor, float *const gyro_sensitivity);
 
     /**
      * @brief Configure INT pin behavior and setup target GPIO.
      *
      * @param sensor object handle of mpu6050
      * @param interrupt_configuration mpu6050 INT pin configuration parameters
      *
      * @return
      *      - ESP_OK Success
      *      - ESP_ERR_INVALID_ARG A parameter is NULL or incorrect
      *      - ESP_FAIL Failed to configure INT pin on mpu6050
      */
     esp_err_t mpu6050_config_interrupts(mpu6050_handle_t sensor, const mpu6050_int_config_t *const interrupt_configuration);
 
     /**
      * @brief Register an Interrupt Service Routine to handle mpu6050 interrupts.
      *
      * @param sensor object handle of mpu6050
      * @param isr function to handle interrupts produced by mpu6050
      *
      * @return
      *      - ESP_OK Success
      *      - ESP_ERR_INVALID_ARG A parameter is NULL or not valid
      *      - ESP_FAIL Failed to register ISR
      */
     esp_err_t mpu6050_register_isr(mpu6050_handle_t sensor, const mpu6050_isr_t isr);
 
     /**
      * @brief Enable specific interrupts from mpu6050
      *
      * @param sensor object handle of mpu6050
      * @param interrupt_sources bit mask with interrupt sources to enable
      *
      * This function does not disable interrupts not set in interrupt_sources. To disable
      * specific mpu6050 interrupts, use mpu6050_disable_interrupts().
      *
      * To enable all mpu6050 interrupts, pass MPU6050_ALL_INTERRUPTS as the argument
      * for interrupt_sources.
      *
      * @return
      *      - ESP_OK Success
      *      - ESP_ERR_INVALID_ARG A parameter is NULL or not valid
      *      - ESP_FAIL Failed to enable interrupt sources on mpu6050
      */
     esp_err_t mpu6050_enable_interrupts(mpu6050_handle_t sensor, uint8_t interrupt_sources);
 
     /**
      * @brief Disable specific interrupts from mpu6050
      *
      * @param sensor object handle of mpu6050
      * @param interrupt_sources bit mask with interrupt sources to disable
      *
      * This function does not enable interrupts not set in interrupt_sources. To enable
      * specific mpu6050 interrupts, use mpu6050_enable_interrupts().
      *
      * To disable all mpu6050 interrupts, pass MPU6050_ALL_INTERRUPTS as the
      * argument for interrupt_sources.
      *
      * @return
      *      - ESP_OK Success
      *      - ESP_ERR_INVALID_ARG A parameter is NULL or not valid
      *      - ESP_FAIL Failed to enable interrupt sources on mpu6050
      */
     esp_err_t mpu6050_disable_interrupts(mpu6050_handle_t sensor, uint8_t interrupt_sources);
 
     /**
      * @brief Get the interrupt status of mpu6050
      *
      * @param sensor object handle of mpu6050
      * @param out_intr_status[out] bit mask that is assigned a value representing the interrupts triggered by the mpu6050
      *
      * This function can be used by the mpu6050 ISR to determine the source of
      * the mpu6050 interrupt that it is handling.
      *
      * After this function returns, the bits set in out_intr_status are
      * the sources of the latest interrupt triggered by the mpu6050. For example,
      * if MPU6050_DATA_RDY_INT_BIT is set in out_intr_status, the last interrupt
      * from the mpu6050 was a DATA READY interrupt.
      *
      * The behavior of the INT_STATUS register of the mpu6050 may change depending on
      * the value of mpu6050_int_clear_t used on interrupt configuration.
      *
      * @return
      *      - ESP_OK Success
      *      - ESP_ERR_INVALID_ARG A parameter is NULL or not valid
      *      - ESP_FAIL Failed to retrieve interrupt status from mpu6050
      */
     esp_err_t mpu6050_get_interrupt_status(mpu6050_handle_t sensor, uint8_t *const out_intr_status);
 
     /**
      * @brief Determine if the last mpu6050 interrupt was due to data ready.
      *
      * @param interrupt_status mpu6050 interrupt status, obtained by invoking mpu6050_get_interrupt_status()
      *
      * @return
      *      - 0: The interrupt was not produced due to data ready
      *      - Any other positive integer: Interrupt was a DATA_READY interrupt
      */
     extern uint8_t mpu6050_is_data_ready_interrupt(uint8_t interrupt_status);
 
     /**
      * @brief Determine if the last mpu6050 interrupt was an I2C master interrupt.
      *
      * @param interrupt_status mpu6050 interrupt status, obtained by invoking mpu6050_get_interrupt_status()
      *
      * @return
      *      - 0: The interrupt is not an I2C master interrupt
      *      - Any other positive integer: Interrupt was an I2C master interrupt
      */
     extern uint8_t mpu6050_is_i2c_master_interrupt(uint8_t interrupt_status);
 
     /**
      * @brief Determine if the last mpu6050 interrupt was triggered by a fifo overflow.
      *
      * @param interrupt_status mpu6050 interrupt status, obtained by invoking mpu6050_get_interrupt_status()
      *
      * @return
      *      - 0: The interrupt is not a fifo overflow interrupt
      *      - Any other positive integer: Interrupt was triggered by a fifo overflow
      */
     extern uint8_t mpu6050_is_fifo_overflow_interrupt(uint8_t interrupt_status);
 
     /**
      * @brief Read raw accelerometer measurements
      *
      * @param sensor object handle of mpu6050
      * @param raw_acce_value raw accelerometer measurements
      *
      * @return
      *     - ESP_OK Success
      *     - ESP_FAIL Fail
      */
     esp_err_t mpu6050_get_raw_acce(mpu6050_handle_t sensor, mpu6050_raw_acce_value_t *const raw_acce_value);
 
     /**
      * @brief Read raw gyroscope measurements
      *
      * @param sensor object handle of mpu6050
      * @param raw_gyro_value raw gyroscope measurements
      *
      * @return
      *     - ESP_OK Success
      *     - ESP_FAIL Fail
      */
     esp_err_t mpu6050_get_raw_gyro(mpu6050_handle_t sensor, mpu6050_raw_gyro_value_t *const raw_gyro_value);
 
     /**
      * @brief Read accelerometer measurements
      *
      * @param sensor object handle of mpu6050
      * @param acce_value accelerometer measurements
      *
      * @return
      *     - ESP_OK Success
      *     - ESP_FAIL Fail
      */
     esp_err_t mpu6050_get_acce(mpu6050_handle_t sensor, mpu6050_acce_value_t *const acce_value);
 
     /**
      * @brief Read gyro values
      *
      * @param sensor object handle of mpu6050
      * @param gyro_value gyroscope measurements
      *
      * @return
      *     - ESP_OK Success
      *     - ESP_FAIL Fail
      */
     esp_err_t mpu6050_get_gyro(mpu6050_handle_t sensor, mpu6050_gyro_value_t *const gyro_value);
 
     /**
      * @brief Read temperature values
      *
      * @param sensor object handle of mpu6050
      * @param temp_value temperature measurements
      *
      * @return
      *     - ESP_OK Success
      *     - ESP_FAIL Fail
      */
     esp_err_t mpu6050_get_temp(mpu6050_handle_t sensor, mpu6050_temp_value_t *const temp_value);
 
     /**
      * @brief Use complimentory filter to calculate roll and pitch
      *
      * @param sensor object handle of mpu6050
      * @param acce_value accelerometer measurements
      * @param gyro_value gyroscope measurements
      * @param complimentary_angle complimentary angle
      *
      * @return
      *     - ESP_OK Success
      *     - ESP_FAIL Fail
      */
     esp_err_t mpu6050_complimentory_filter(mpu6050_handle_t sensor, const mpu6050_acce_value_t *const acce_value,
                                            const mpu6050_gyro_value_t *const gyro_value, complimentary_angle_t *const complimentary_angle);
 
     /**
      * @brief Enable motion detection on MPU6050.
      *
      * Configures the MPU6050 to detect motion based on a specified threshold and duration.
      * When motion acceleration exceeds the threshold for the set duration, an interrupt is triggered.
      *
      * @param sensor Handle to the MPU6050 sensor instance.
      * @param threshold Motion detection sensitivity (0 - 255). Higher value means less sensitive.
      * @param duration Motion duration (0 - 255) required to trigger the interrupt.
      *
      * @return
      *     - ESP_OK: Success
      *     - ESP_FAIL: Failure
      */
     esp_err_t mpu6050_enable_motiondetection(mpu6050_handle_t sensor, uint8_t threshold, uint8_t duration);
 
 #ifdef __cplusplus
 }
 #endif
 