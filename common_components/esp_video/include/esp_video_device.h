/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Camera data interface video device
 */
#define ESP_VIDEO_MIPI_CSI_DEVICE_ID        0
#define ESP_VIDEO_MIPI_CSI_DEVICE_NAME      "/dev/video0"

#define ESP_VIDEO_ISP_DVP_DEVICE_ID         1
#define ESP_VIDEO_ISP_DVP_DEVICE_NAME       "/dev/video1"

#define ESP_VIDEO_DVP_DEVICE_ID             2
#define ESP_VIDEO_DVP_DEVICE_NAME           "/dev/video2"

/**
 * @brief Codec video device
 */
#define ESP_VIDEO_JPEG_DEVICE_ID            10
#define ESP_VIDEO_JPEG_DEVICE_NAME          "/dev/video10"

#define ESP_VIDEO_H264_DEVICE_ID            11
#define ESP_VIDEO_H264_DEVICE_NAME          "/dev/video11"

/**
 * @brief ISP video device
 */
#define ESP_VIDEO_ISP1_DEVICE_ID            20
#define ESP_VIDEO_ISP1_DEVICE_NAME          "/dev/video20"

#ifdef __cplusplus
}
#endif
