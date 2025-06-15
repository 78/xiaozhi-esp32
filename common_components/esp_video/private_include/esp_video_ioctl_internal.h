/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"
#include "esp_video.h"
#include "esp_video_ioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief video device ioctl
 *
 * @param video video object
 * @param cmd ioctl cmd which is defined in include/linux/videodev2.h
 * @param args the args list of the ioctl cmd
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_ioctl(struct esp_video *video, int cmd, va_list args);

#ifdef __cplusplus
}
#endif
