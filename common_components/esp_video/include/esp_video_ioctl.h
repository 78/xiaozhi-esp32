/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "linux/videodev2.h"
#include "esp_cam_sensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VIDIOC_S_SENSOR_FMT _IOWR('V',  BASE_VIDIOC_PRIVATE + 1, esp_cam_sensor_format_t)
#define VIDIOC_G_SENSOR_FMT _IOWR('V',  BASE_VIDIOC_PRIVATE + 2, esp_cam_sensor_format_t)

#define V4L2_CID_CAMERA_AE_LEVEL        (V4L2_CID_CAMERA_CLASS_BASE + 40)
#define V4L2_CID_CAMERA_STATS           (V4L2_CID_CAMERA_CLASS_BASE + 41)

#ifdef __cplusplus
}
#endif
