/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdio.h>
#include <string.h>
#include <sys/lock.h>
#include "esp_heap_caps.h"
#include "esp_video.h"
#include "esp_video_vfs.h"
#include "esp_video_ioctl_internal.h"

#define BUF_OFF(type, element_index)        (((uint32_t)type << 24) + element_index)
#define BUF_OFF_2_INDEX(buf_off)            ((buf_off) & 0x00ffffff)
#define BUF_OFF_2_TYPE(buf_off)             ((buf_off) >> 24)

static esp_err_t esp_video_ioctl_querycap(struct esp_video *video, struct v4l2_capability *cap)
{
    memset(cap, 0, sizeof(struct v4l2_capability));

    snprintf((char *)cap->driver, sizeof(cap->driver), "%s", video->dev_name);
    snprintf((char *)cap->card, sizeof(cap->card), "%s", video->dev_name);
    snprintf((char *)cap->bus_info, sizeof(cap->bus_info), "%s:%s", CONFIG_IDF_TARGET, video->dev_name);
    cap->version = (ESP_VIDEO_VER_MAJOR << 16) | (ESP_VIDEO_VER_MINOR << 8) | ESP_VIDEO_VER_PATCH;
    cap->capabilities = video->caps;
    if (video->caps & V4L2_CAP_DEVICE_CAPS) {
        cap->device_caps = video->device_caps;
    }

    return ESP_OK;
}

static esp_err_t esp_video_ioctl_g_fmt(struct esp_video *video, struct v4l2_format *fmt)
{
    return esp_video_get_format(video, fmt);
}

static esp_err_t esp_video_ioctl_enum_fmt(struct esp_video *video, struct v4l2_fmtdesc *fmt)
{
    esp_err_t ret;
    struct esp_video_format_desc desc;

    ret = esp_video_enum_format(video, fmt->type, fmt->index, &desc);
    if (ret == ESP_OK) {
        fmt->flags = 0;
        fmt->mbus_code = 0;
        fmt->pixelformat = desc.pixel_format;
        memcpy(fmt->description, desc.description, sizeof(fmt->description));
    }

    return ret;
}

static esp_err_t esp_video_ioctl_s_fmt(struct esp_video *video, struct v4l2_format *fmt)
{
    return esp_video_set_format(video, fmt);
}

static esp_err_t esp_video_ioctl_streamon(struct esp_video *video, int *arg)
{
    esp_err_t ret;
    enum v4l2_buf_type type = *(enum v4l2_buf_type *)arg;

    ret = esp_video_start_capture(video, type);

    return ret;
}

static esp_err_t esp_video_ioctl_streamoff(struct esp_video *video, int *arg)
{
    esp_err_t ret;
    enum v4l2_buf_type type = *(enum v4l2_buf_type *)arg;

    ret = esp_video_stop_capture(video, type);

    return ret;
}

static esp_err_t esp_video_ioctl_reqbufs(struct esp_video *video, struct v4l2_requestbuffers *req_bufs)
{
    esp_err_t ret;

    if ((req_bufs->memory != V4L2_MEMORY_MMAP) &&
            (req_bufs->memory != V4L2_MEMORY_USERPTR) ) {
        return ESP_ERR_INVALID_ARG;
    }

    if (req_bufs->count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_video_setup_buffer(video, req_bufs->type, req_bufs->memory, req_bufs->count);

    return ret;
}

static esp_err_t esp_video_ioctl_querybuf(struct esp_video *video, struct v4l2_buffer *vbuf)
{
    esp_err_t ret;
    struct esp_video_buffer_info info;

    ret = esp_video_get_buffer_info(video, vbuf->type, &info);
    if (ret != ESP_OK) {
        return ret;
    }

    if ((vbuf->memory != info.memory_type) || (vbuf->index >= info.count)) {
        return ESP_ERR_INVALID_ARG;
    }

    vbuf->length = info.size;
    if (vbuf->memory == V4L2_MEMORY_MMAP) {
        /* offset contains of stream ID and buffer index  */

        vbuf->m.offset = BUF_OFF(vbuf->type, vbuf->index);
    }

    return ESP_OK;
}

static esp_err_t esp_video_ioctl_mmap(struct esp_video *video, struct esp_video_ioctl_mmap *ioctl_mmap)
{
    esp_err_t ret;
    struct esp_video_buffer_info info;
    uint8_t type = BUF_OFF_2_TYPE(ioctl_mmap->offset);
    int index = BUF_OFF_2_INDEX(ioctl_mmap->offset);

    ret = esp_video_get_buffer_info(video, type, &info);
    if (ret != ESP_OK) {
        return ret;
    }

    if ((info.memory_type != V4L2_MEMORY_MMAP) ||
            (ioctl_mmap->length > info.size) ||
            (index >= info.count)) {
        return ESP_ERR_INVALID_ARG;
    }

    ioctl_mmap->mapped_ptr = esp_video_get_element_index_payload(video, type, index);

    return ESP_OK;
}

static esp_err_t esp_video_ioctl_qbuf(struct esp_video *video, struct v4l2_buffer *vbuf)
{
    esp_err_t ret;
    struct esp_video_buffer_info info;

    ret = esp_video_get_buffer_info(video, vbuf->type, &info);
    if (ret != ESP_OK) {
        return ret;
    }

    if ((vbuf->memory != info.memory_type) || (vbuf->index >= info.count)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (info.memory_type == V4L2_MEMORY_USERPTR) {
        if (!vbuf->m.userptr) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    if (info.memory_type == V4L2_MEMORY_MMAP) {
        ret = esp_video_queue_element_index(video, vbuf->type, vbuf->index);
    } else {
        ret = esp_video_queue_element_index_buffer(video, vbuf->type, vbuf->index, (uint8_t *)vbuf->m.userptr, vbuf->length);
    }

    return ret;
}

static esp_err_t esp_video_ioctl_dqbuf(struct esp_video *video, struct v4l2_buffer *vbuf)
{
    esp_err_t ret;
    uint32_t ticks = portMAX_DELAY;
    struct esp_video_buffer_info info;
    struct esp_video_buffer_element *element;

    ret = esp_video_get_buffer_info(video, vbuf->type, &info);
    if (ret != ESP_OK) {
        return ret;
    }

    if (vbuf->memory != info.memory_type) {
        return ESP_ERR_INVALID_ARG;
    }

    element = esp_video_recv_element(video, vbuf->type, ticks);
    if (!element) {
        return ESP_FAIL;
    }

    vbuf->flags     = 0;
    vbuf->index     = element->index;
    vbuf->bytesused = element->valid_size;
    if (!vbuf->bytesused) {
        vbuf->flags |= V4L2_BUF_FLAG_ERROR;
    } else {
        vbuf->flags |= V4L2_BUF_FLAG_DONE;
    }
    if (vbuf->memory != V4L2_MEMORY_USERPTR) {
        vbuf->m.userptr = (unsigned long)element->buffer;
        vbuf->flags |= V4L2_BUF_FLAG_MAPPED;
    }

    return ESP_OK;
}

static inline esp_err_t esp_video_ioctl_set_ext_ctrls(struct esp_video *video, const struct v4l2_ext_controls *controls)
{
    return esp_video_set_ext_controls(video, controls);
}

static inline esp_err_t esp_video_ioctl_get_ext_ctrls(struct esp_video *video, struct v4l2_ext_controls *controls)
{
    return esp_video_get_ext_controls(video, controls);
}

static inline esp_err_t esp_video_ioctl_query_ext_ctrls(struct esp_video *video, struct v4l2_query_ext_ctrl *qctrl)
{
    return esp_video_query_ext_control(video, qctrl);
}

static inline esp_err_t esp_video_ioctl_set_sensor_format(struct esp_video *video, const esp_cam_sensor_format_t *format)
{
    return esp_video_set_sensor_format(video, format);
}

static inline esp_err_t esp_video_ioctl_get_sensor_format(struct esp_video *video, esp_cam_sensor_format_t *format)
{
    return esp_video_get_sensor_format(video, format);
}

static inline esp_err_t esp_video_ioctl_query_menu(struct esp_video *video, struct v4l2_querymenu *qmenu)
{
    return esp_video_query_menu(video, qmenu);
}

esp_err_t esp_video_ioctl(struct esp_video *video, int cmd, va_list args)
{
    esp_err_t ret = ESP_OK;
    void *arg_ptr;

    assert(video);

    arg_ptr = va_arg(args, void *);
    if (!arg_ptr) {
        return ESP_ERR_INVALID_ARG;
    }

    switch (cmd) {
    case VIDIOC_QBUF:
        ret = esp_video_ioctl_qbuf(video, (struct v4l2_buffer *)arg_ptr);
        break;
    case VIDIOC_DQBUF:
        ret = esp_video_ioctl_dqbuf(video, (struct v4l2_buffer *)arg_ptr);
        break;
    case VIDIOC_QUERYCAP:
        ret = esp_video_ioctl_querycap(video, (struct v4l2_capability *)arg_ptr);
        break;
    case VIDIOC_ENUM_FMT:
        ret = esp_video_ioctl_enum_fmt(video, (struct v4l2_fmtdesc *)arg_ptr);
        break;
    case VIDIOC_G_FMT:
        ret = esp_video_ioctl_g_fmt(video, (struct v4l2_format *)arg_ptr);
        break;
    case VIDIOC_S_FMT:
        ret = esp_video_ioctl_s_fmt(video, (struct v4l2_format *)arg_ptr);
        break;
    case VIDIOC_STREAMON:
        ret = esp_video_ioctl_streamon(video, (int *)arg_ptr);
        break;
    case VIDIOC_STREAMOFF:
        ret = esp_video_ioctl_streamoff(video, (int *)arg_ptr);
        break;
    case VIDIOC_REQBUFS:
        ret = esp_video_ioctl_reqbufs(video, (struct v4l2_requestbuffers *)arg_ptr);
        break;
    case VIDIOC_QUERYBUF:
        ret = esp_video_ioctl_querybuf(video, (struct v4l2_buffer *)arg_ptr);
        break;
    case VIDIOC_MMAP:
        ret = esp_video_ioctl_mmap(video, (struct esp_video_ioctl_mmap *)arg_ptr);
        break;
    case VIDIOC_G_EXT_CTRLS:
        ret = esp_video_ioctl_get_ext_ctrls(video, (struct v4l2_ext_controls *)arg_ptr);
        break;
    case VIDIOC_S_EXT_CTRLS:
        ret = esp_video_ioctl_set_ext_ctrls(video, (const struct v4l2_ext_controls *)arg_ptr);
        break;
    case VIDIOC_QUERY_EXT_CTRL:
        ret = esp_video_ioctl_query_ext_ctrls(video, (struct v4l2_query_ext_ctrl *)arg_ptr);
        break;
    case VIDIOC_S_SENSOR_FMT:
        ret = esp_video_ioctl_set_sensor_format(video, (const esp_cam_sensor_format_t *)arg_ptr);
        break;
    case VIDIOC_G_SENSOR_FMT:
        ret = esp_video_ioctl_get_sensor_format(video, (esp_cam_sensor_format_t *)arg_ptr);
        break;
    case VIDIOC_QUERYMENU:
        ret = esp_video_ioctl_query_menu(video, (struct v4l2_querymenu *)arg_ptr);
        break;
    default:
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    return ret;
}
