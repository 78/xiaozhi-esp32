/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "esp_check.h"
#include "esp_cam_sensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Query the supported data types of extended control parameters.
 *
 * @param[in] dev Camera sensor device handle that created by `sensor_detect`.
 * @param[out] qdesc The pointer to hold the extended control parameters.
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Error in the passed arguments.
 *      - ESP_ERR_NOT_SUPPORTED: The sensor driver does not support this operation.
 */
esp_err_t esp_cam_sensor_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc);

/**
 * @brief Get the current value of the control parameter.
 *
 * @param[in] dev Camera sensor device handle that created by `sensor_detect`.
 * @param[in] id Camera sensor parameter ID.
 * @param[out] arg Camera sensor parameter setting data pointer.
 * @param[in] size Camera sensor parameter setting data size.
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Error in the passed arguments.
 *      - ESP_ERR_NOT_SUPPORTED: The sensor driver does not support this operation.
 */
esp_err_t esp_cam_sensor_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size);

/**
 * @brief Set the value of the control parameter.
 *
 * @param[in] dev Camera sensor device handle that created by `sensor_detect`.
 * @param[in] id Camera sensor parameter ID.
 * @param[in] arg Camera sensor parameter setting data pointer.
 * @param[in] size Camera sensor parameter setting data size.
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Error in the passed arguments.
 *      - ESP_ERR_NOT_SUPPORTED: The sensor driver does not support this operation.
 */
esp_err_t esp_cam_sensor_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size);

/**
 * @brief Get the camera sensor's capabilities, see esp_cam_sensor_capability_t.
 *
 * @param[in] dev Camera sensor device handle that created by `sensor_detect`.
 * @param[out] caps The pointer to hold the description of device caps.
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Error in the passed arguments.
 *      - ESP_ERR_NOT_SUPPORTED: The sensor driver does not support this operation.
 */
esp_err_t esp_cam_sensor_get_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *caps);

/**
 * @brief Get driver information supported by the camera driver.
 *
 * @param[in] dev Camera sensor device handle that created by `sensor_detect`.
 * @param[out] format_arry The pointer to hold the description of the currently supported output format.
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Error in the passed arguments.
 *      - ESP_ERR_NOT_SUPPORTED: The sensor driver does not support this operation.
 */
esp_err_t esp_cam_sensor_query_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *format_arry);

/**
 * @brief Set the output format of the camera sensor.
 *
 * @note  If format is NULL, the camera sensor will load the default configuration based on the configured interface.
 *        See MIPI_IF_FORMAT_INDEX_DAFAULT and DVP_IF_FORMAT_INDEX_DAFAULT.
 *
 * @note  Query the currently supported output formats by calling esp_cam_sensor_query_format.
 *
 * @param[in] dev Camera sensor device handle that created by `sensor_detect`.
 * @param[in] format The pointer to hold the description of the currently supported output format.
 *
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Error in the passed arguments.
 *      - ESP_ERR_NOT_SUPPORTED: The sensor driver does not support this operation.
 *      - ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT: An error occurred while writing data over the SCCB bus
 */
esp_err_t esp_cam_sensor_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format);

/**
 * @brief Get the current camera sensor output format.
 *
 * @param[in] dev Camera sensor device handle that created by `sensor_detect`.
 * @param[out] format The pointer to hold the description of the currently output format.
 * @return
 *      - ESP_OK: Success
 *      - ESP_FAIL: The sensor driver has not been configured the output format yet.
 *      - ESP_ERR_INVALID_ARG: Error in the passed arguments.
 *      - ESP_ERR_NOT_SUPPORTED: The sensor driver does not support this operation.
 */
esp_err_t esp_cam_sensor_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format);

/**
 * @brief Perform an ioctl request on the camera sensor.
 *
 * @param[in] dev Camera sensor device handle that created by `sensor_detect`.
 * @param[in] cmd The ioctl command, see esp_cam_sensor.h, for example ESP_CAM_SENSOR_IOC_S_STREAM.
 * @param[in] arg The argument accompanying the ioctl command.
 * @return
 *      - ESP_OK: Success
 *      - ESP_ERR_INVALID_ARG: Error in the passed arguments.
 *      - ESP_ERR_NOT_SUPPORTED: The sensor driver does not support this cmd or arg.
 */
esp_err_t esp_cam_sensor_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg);

/**
 * @brief Get the module name of the current camera device.
 *
 * @param[in] dev Camera sensor device handle that created by `sensor_detect`.
 * @return
 *      - Camera module name on success, or "NULL"
 */
const char *esp_cam_sensor_get_name(esp_cam_sensor_device_t *dev);

/**
 * @brief Delete camera device
 *
 * @param[in] dev Camera sensor device handle that created by `sensor_detect`.
 * @return
 *        - ESP_OK: If Camera sensor is successfully deleted.
 */
esp_err_t esp_cam_sensor_del_dev(esp_cam_sensor_device_t *dev);

#ifdef __cplusplus
}
#endif
