/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "esp_cam_ctlr_dvp.h"
#include "driver/jpeg_encode.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SCCB initialization configuration
 */
typedef struct esp_video_init_sccb_config {
    bool init_sccb;                             /*!< true:  SCCB I2C is not initialized and esp_video_init function will initialize SCCB I2C with given parameters i2c_config.
                                                     false: SCCB I2C is initialized and esp_video_init function can use i2c_handle directly */
    union {
        struct {
            uint8_t port;                       /*!< SCCB I2C port */
            uint8_t scl_pin;                    /*!< SCCB I2C SCL pin */
            uint8_t sda_pin;                    /*!< SCCB I2C SDA pin */
        } i2c_config;

        i2c_master_bus_handle_t i2c_handle;     /*!< SCCB I2C handle */
    };

    uint32_t freq;                              /*!< SCCB I2C frequency */
} esp_video_init_sccb_config_t;

/**
 * @brief MIPI CSI initialization and camera sensor connection configuration
 */
typedef struct esp_video_init_csi_config {
    esp_video_init_sccb_config_t sccb_config;   /*!< Camera sensor SCCB configuration */

    int8_t  reset_pin;                          /*!< Camera sensor reset pin, if hardware has no reset pin, set reset_pin to be -1 */
    int8_t  pwdn_pin;                           /*!< Camera sensor power down pin, if hardware has no power down pin, set pwdn_pin to be -1 */
} esp_video_init_csi_config_t;

/**
 * @brief DVP initialization and camera sensor connection configuration
 */
typedef struct esp_video_init_dvp_config {
    esp_video_init_sccb_config_t sccb_config;   /*!< Camera sensor SCCB configuration */

    int8_t  reset_pin;                          /*!< Camera sensor reset pin, if hardware has no reset pin, set reset_pin to be -1 */
    int8_t  pwdn_pin;                           /*!< Camera sensor power down pin, if hardware has no power down pin, set pwdn_pin to be -1 */

    esp_cam_ctlr_dvp_pin_config_t dvp_pin;      /*!< DVP pin configuration */

    uint32_t xclk_freq;                         /*!< DVP hardware output clock frequency */
} esp_video_init_dvp_config_t;

/**
 * @brief JPEG initialization configuration
 */
typedef struct esp_video_init_jpeg_config {
    jpeg_encoder_handle_t enc_handle;           /*!< JPEG encoder driver handle:
                                                     - NULL, JPEG video device will create JPEG encoder driver handle by itself
                                                     - Not null, JPEG video device will use this handle instead of creating JPEG encoder driver handle */
} esp_video_init_jpeg_config_t;

/**
 * @brief Video hardware initialization configuration
 */
typedef struct esp_video_init_config {
    const esp_video_init_csi_config_t *csi;     /*!< MIPI CSI initialization configuration */
    const esp_video_init_dvp_config_t *dvp;     /*!< DVP initialization configuration array */
    const esp_video_init_jpeg_config_t *jpeg;   /*!< JPEG initialization configuration */
} esp_video_init_config_t;

/**
 * @brief Initialize video hardware and software, including I2C, MIPI CSI and so on.
 *
 * @param config video hardware configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_init(const esp_video_init_config_t *config);

#ifdef __cplusplus
}
#endif
