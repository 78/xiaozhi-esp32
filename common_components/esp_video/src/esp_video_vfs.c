/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/lock.h>
#include <sys/errno.h>
#include <sys/param.h>
#include "linux/videodev2.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_dev.h"
#include "esp_video_vfs.h"
#include "esp_video_ioctl_internal.h"

static int esp_err_to_errno(esp_err_t err)
{
    switch (err) {
    case ESP_OK:
        return 0;
    case ESP_ERR_NO_MEM:
        errno = ENOMEM;
        return -1;
    case ESP_ERR_INVALID_SIZE:
    case ESP_ERR_INVALID_ARG:
        errno = EINVAL;
        return -1;
    case ESP_ERR_INVALID_STATE:
    case ESP_ERR_NOT_FINISHED:
        errno = EBUSY;
        return -1;
    case ESP_ERR_NOT_FOUND:
        errno = ENODEV;
        return -1;
    case ESP_ERR_NOT_SUPPORTED:
        errno = ESRCH;
        return -1;
    case ESP_ERR_TIMEOUT:
        errno = ETIMEDOUT;
        return -1;
    default:
        errno = EPERM;
        return -1;
    }
}

static int esp_video_vfs_open(void *ctx, const char *path, int flags, int mode)
{
    struct esp_video *video = (struct esp_video *)ctx;

    /* Open video here to initialize software resource and hardware */

    video = esp_video_open(video->dev_name);
    if (!video) {
        errno = ENOENT;
        return -1;
    }

    return video->id;
}

static ssize_t esp_video_vfs_write(void *ctx, int fd, const void *data, size_t size)
{
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0 && data && size);
    assert(video);

    errno = EPERM;

    return -1;
}

static ssize_t esp_video_vfs_read(void *ctx, int fd, void *data, size_t size)
{
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0 && data && size);
    assert(video);

    errno = EPERM;

    return -1;
}

static int esp_video_vfs_fstat(void *ctx, int fd, struct stat *st)
{
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0 && st);
    assert(video);

    memset(st, 0, sizeof(*st));

    return 0;
}

static int esp_video_vfs_close(void *ctx, int fd)
{
    esp_err_t ret;
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0);
    assert(video);

    ret = esp_video_close(video);

    return esp_err_to_errno(ret);
}

static int esp_video_vfs_fcntl(void *ctx, int fd, int cmd, int arg)
{
    int ret;
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0);
    assert(video);

    switch (cmd) {
    case F_GETFL:
        ret = O_RDONLY;
        break;
    default:
        ret = -1;
        errno = ENOSYS;
        break;
    }

    return ret;
}

static int esp_video_vfs_fsync(void *ctx, int fd)
{
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0);
    assert(video);

    return 0;
}

static int esp_video_vfs_ioctl(void *ctx, int fd, int cmd, va_list args)
{
    esp_err_t ret;
    struct esp_video *video = (struct esp_video *)ctx;

    assert(fd >= 0);
    assert(video);

    ret = esp_video_ioctl(video, cmd, args);

    return esp_err_to_errno(ret);
}

static const esp_vfs_t s_esp_video_vfs = {
    .flags   = ESP_VFS_FLAG_CONTEXT_PTR,
    .open_p  = esp_video_vfs_open,
    .close_p = esp_video_vfs_close,
    .write_p = esp_video_vfs_write,
    .read_p  = esp_video_vfs_read,
    .fcntl_p = esp_video_vfs_fcntl,
    .fsync_p = esp_video_vfs_fsync,
    .fstat_p = esp_video_vfs_fstat,
    .ioctl_p = esp_video_vfs_ioctl
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
esp_err_t esp_video_vfs_dev_register(const char *name, struct esp_video *video)
{
    esp_err_t ret;
    char *vfs_name;

    ret = asprintf(&vfs_name, "/dev/%s", name);
    if (ret <= 0) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_vfs_register(vfs_name, &s_esp_video_vfs, video);
    free(vfs_name);

    return ret;
}

/**
 * @brief Unregister video device from VFS system.
 *
 * @param name Video device name
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_vfs_dev_unregister(const char *name)
{
    esp_err_t ret;
    char *vfs_name;

    ret = asprintf(&vfs_name, "/dev/%s", name);
    if (ret <= 0) {
        return ESP_ERR_NO_MEM;
    }

    ret = esp_vfs_unregister(vfs_name);
    free(vfs_name);

    return ret;
}
