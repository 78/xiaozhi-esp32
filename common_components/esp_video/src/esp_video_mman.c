/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <sys/ioctl.h>
#include <sys/mman.h>
#include "esp_video_vfs.h"

/**
 * @brief This is only used for video device to map video buffer to user side.
 *
 * @param addr   Video ignores this argument, it's recommended to fixedly set NULL
 * @param length Mapped buffer length should <= video buffer length
 * @param prot   Video ignores this argument, it's recommended to fixedly set (PROT_READ | PROT_WRITE)
 * @param flags  Video ignores this argument, it's recommended to fixedly set MAP_SHARED
 * @param fd     Video device file description
 * @param offset Video buffer offset
 *
 * @return
 *      - Mapped video buffer pointer on success
 *      - NULL if failed
 */
void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset)
{
    int ret;
    struct esp_video_ioctl_mmap ioctl_mmap;

    ioctl_mmap.length = length;
    ioctl_mmap.offset = offset;
    ret = ioctl(fd, VIDIOC_MMAP, &ioctl_mmap);
    if (ret != 0) {
        return NULL;
    }

    return ioctl_mmap.mapped_ptr;
}

/**
 * @brief Free mapped video buffer
 *
 * @param addr   Mapped video buffer pointer
 * @param length Mapped buffer length should <= video buffer length
 *
 * @return
 *      - 0 on success
 *      - -1 if failed
 */
int munmap(void *addr, size_t length)
{
    return 0;
}
