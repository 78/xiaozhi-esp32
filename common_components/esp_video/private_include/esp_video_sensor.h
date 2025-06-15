/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_video.h"
#include "esp_cam_sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Set control value to camera sensor device
 *
 * @param cam_dev  Camera sensor device pointer
 * @param controls V4L2 external controls pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_set_ext_ctrls_to_sensor(esp_cam_sensor_device_t *cam_dev, const struct v4l2_ext_controls *controls);

/**
 * @brief Get control value from camera sensor device
 *
 * @param cam_dev  Camera sensor device pointer
 * @param controls V4L2 external controls pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_get_ext_ctrls_from_sensor(esp_cam_sensor_device_t *cam_dev, struct v4l2_ext_controls *controls);

/**
 * @brief Get control description from camera sensor device
 *
 * @param cam_dev  Camera sensor device pointer
 * @param qctrl    V4L2 external controls query description pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_query_ext_ctrls_from_sensor(esp_cam_sensor_device_t *cam_dev, struct v4l2_query_ext_ctrl *qctrl);

/**
 * @brief Query menu value from camera sensor device
 *
 * @param cam_dev  Camera sensor device pointer
 * @param qmenu    Menu value buffer pointer
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_query_menu_from_sensor(esp_cam_sensor_device_t *cam_dev, struct v4l2_querymenu *qmenu);

#ifdef __cplusplus
}
#endif
