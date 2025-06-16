/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "linux/ioctl.h"
#include "esp_vfs.h"
#include "esp_video.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VIDIOC_MMAP     _IOWR('V', 192, struct esp_video_ioctl_mmap)

struct esp_video_ioctl_mmap {
    size_t length;
    off_t offset;

    void *mapped_ptr;
};

/**
 * @brief Register video device into VFS system.
 *
 * @param name Video device name
 * @param video Video object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_vfs_dev_register(const char *name, struct esp_video *video);

/**
 * @brief Unregister video device from VFS system.
 *
 * @param name Video device name
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_vfs_dev_unregister(const char *name);

#ifdef __cplusplus
}
#endif
