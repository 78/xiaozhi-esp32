/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_attr.h"

#include "esp_h264_enc_single_hw.h"
#include "esp_h264_enc_single_sw.h"
#include "esp_h264_enc_single.h"

#include "esp_video.h"
#include "esp_video_device_internal.h"

#define H264_NAME                   "H.264"

#define H264_DMA_ALIGN_BYTES        64
#define H264_MEM_CAPS               (MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED)

#define H264_VIDEO_DEVICE_GOP       30
#define H264_VIDEO_DEVICE_MIN_QP    25
#define H264_VIDEO_DEVICE_MAX_QP    26
#define H264_VIDEO_DEVICE_BITRATE   10000000

#define H264_VIDEO_MAX_I_PERIOD     120
#define H264_VIDEO_MIN_I_PERIOD     1
#define H264_VIDEO_I_PERIOD_STEP    1

#define H264_VIDEO_MAX_BITRATE      2500000
#define H264_VIDEO_MIN_BITRATE      25000
#define H264_VIDEO_BITRATE_STEP     25000

#define H264_VIDEO_MAX_QP           51
#define H264_VIDEO_MIN_QP           0
#define H264_VIDEO_QP_STEP          1

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)   sizeof(x) / sizeof((x)[0])
#endif

struct h264_video {
    bool hw_codec;

    esp_h264_raw_format_t input_format;
    uint8_t gop;
    uint8_t min_qp;
    uint8_t max_qp;
    uint32_t bitrate;
    esp_h264_enc_handle_t enc_handle;
};

static const char *TAG = "h.264_video";

static esp_err_t errno_h264_to_std(esp_h264_err_t h264_err)
{
    switch (h264_err) {
    case ESP_H264_ERR_OK:
        return ESP_OK;
    case ESP_H264_ERR_ARG:
        return ESP_ERR_INVALID_ARG;
    case ESP_H264_ERR_MEM:
        return ESP_ERR_NO_MEM;
    case ESP_H264_ERR_UNSUPPORTED:
        return ESP_ERR_NOT_SUPPORTED;
    case ESP_H264_ERR_TIMEOUT:
        return ESP_ERR_TIMEOUT;
    case ESP_H264_ERR_OVERFLOW:
        return ESP_ERR_INVALID_SIZE;
    default:
        return ESP_FAIL;
    }
}

static esp_err_t h264_get_input_format_from_v4l2(uint32_t v4l2_format, esp_h264_raw_format_t *input_format, uint8_t *input_bpp)
{
    esp_err_t ret = ESP_OK;

    switch (v4l2_format) {
    case V4L2_PIX_FMT_YUV420:
        *input_format = ESP_H264_RAW_FMT_O_UYY_E_VYY;
        *input_bpp = 12;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static esp_err_t h264_video_m2m_process(struct esp_video *video, uint8_t *src, uint32_t src_size, uint8_t *dst, uint32_t dst_size, uint32_t *dst_out_size)
{
    esp_h264_err_t h264_err;
    esp_h264_enc_in_frame_t in_frame = {
        .raw_data = {
            .buffer = src,
            .len = src_size,
        }
    };
    esp_h264_enc_out_frame_t out_frame = {
        .raw_data = {
            .buffer = dst,
            .len = dst_size,
        }
    };
    struct h264_video *h264_video = VIDEO_PRIV_DATA(struct h264_video *, video);

    h264_err = esp_h264_enc_process(h264_video->enc_handle, &in_frame, &out_frame);
    if (h264_err == ESP_H264_ERR_OK) {
        *dst_out_size = out_frame.length;
    }

    return errno_h264_to_std(h264_err);
}

static esp_err_t h264_video_init(struct esp_video *video)
{
    M2M_VIDEO_SET_CAPTURE_FORMAT(video, 0, 0, 0);
    M2M_VIDEO_SET_OUTPUT_FORMAT(video, 0, 0, 0);

    return ESP_OK;
}

static esp_err_t h264_video_deinit(struct esp_video *video)
{
    return ESP_OK;
}

static esp_err_t h264_video_start(struct esp_video *video, uint32_t type)
{
    esp_h264_err_t h264_err = ESP_H264_ERR_UNSUPPORTED;
    struct h264_video *h264_video = VIDEO_PRIV_DATA(struct h264_video *, video);

    if ((M2M_VIDEO_GET_CAPTURE_FORMAT_WIDTH(video) != M2M_VIDEO_GET_OUTPUT_FORMAT_WIDTH(video)) ||
            (M2M_VIDEO_GET_CAPTURE_FORMAT_HEIGHT(video) != M2M_VIDEO_GET_OUTPUT_FORMAT_HEIGHT(video))) {
        ESP_LOGE(TAG, "width or height is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        esp_h264_enc_cfg_hw_t config = {
            .pic_type = h264_video->input_format,
            .gop = h264_video->gop,
            .fps = h264_video->gop,
            .res = {
                .width = M2M_VIDEO_GET_OUTPUT_FORMAT_WIDTH(video),
                .height = M2M_VIDEO_GET_OUTPUT_FORMAT_HEIGHT(video),
            },
            .rc = {
                .bitrate = h264_video->bitrate,
                .qp_min = h264_video->min_qp,
                .qp_max = h264_video->max_qp,
            }
        };

        if (h264_video->hw_codec) {
            h264_err = esp_h264_enc_hw_new(&config, &h264_video->enc_handle);
        } else {
            h264_err = ESP_H264_ERR_UNSUPPORTED;
        }

        if (h264_err != ESP_H264_ERR_OK) {
            ESP_LOGE(TAG, "failed to create H.264 encoder");
            return errno_h264_to_std(h264_err);
        }

        h264_err = esp_h264_enc_open(h264_video->enc_handle);
        if (h264_err != ESP_H264_ERR_OK) {
            esp_h264_enc_del(h264_video->enc_handle);
            h264_video->enc_handle = NULL;

            ESP_LOGE(TAG, "failed to open H.264 encoder");
            return errno_h264_to_std(h264_err);
        }
    }

    return ESP_OK;
}

static esp_err_t h264_video_stop(struct esp_video *video, uint32_t type)
{
    esp_h264_err_t h264_err;
    struct h264_video *h264_video = VIDEO_PRIV_DATA(struct h264_video *, video);

    if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        h264_err = esp_h264_enc_close(h264_video->enc_handle);
        if (h264_err != ESP_H264_ERR_OK) {
            ESP_LOGE(TAG, "failed to close H.264 encoder");
            return errno_h264_to_std(h264_err);
        }

        h264_err = esp_h264_enc_del(h264_video->enc_handle);
        if (h264_err != ESP_H264_ERR_OK) {
            ESP_LOGE(TAG, "failed to delete H.264 encoder");
            return errno_h264_to_std(h264_err);
        }
        h264_video->enc_handle = NULL;
    }

    return ESP_OK;
}

static esp_err_t h264_video_enum_format(struct esp_video *video, uint32_t type, uint32_t index, uint32_t *pixel_format)
{
    if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        static const uint32_t h264_capture_format[] = {
            V4L2_PIX_FMT_H264,
        };

        if (index >= ARRAY_SIZE(h264_capture_format)) {
            return ESP_ERR_INVALID_ARG;
        }

        *pixel_format = h264_capture_format[index];
    } else if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        static const uint32_t h264_output_format[] = {
            V4L2_PIX_FMT_YUV420,
        };

        if (index >= ARRAY_SIZE(h264_output_format)) {
            return ESP_ERR_INVALID_ARG;
        }

        *pixel_format = h264_output_format[index];
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

static esp_err_t h264_video_set_format(struct esp_video *video, const struct v4l2_format *format)
{
    esp_err_t ret;
    const struct v4l2_pix_format *pix = &format->fmt.pix;
    struct h264_video *h264_video = VIDEO_PRIV_DATA(struct h264_video *, video);

    if (format->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        uint32_t width = M2M_VIDEO_GET_OUTPUT_FORMAT_WIDTH(video);
        uint32_t height = M2M_VIDEO_GET_OUTPUT_FORMAT_HEIGHT(video);

        if ((pix->pixelformat != V4L2_PIX_FMT_H264) ||
                (width && (pix->width != width)) ||
                (height && (pix->height != height))) {
            ESP_LOGE(TAG, "pixel format or width or height is invalid");
            return ESP_ERR_INVALID_ARG;
        }

        uint32_t buf_size = pix->width * pix->height * 8 / 2;

        ESP_LOGD(TAG, "capture buffer size=%" PRIu32, buf_size);

        M2M_VIDEO_SET_CAPTURE_BUF_INFO(video, buf_size, H264_DMA_ALIGN_BYTES, H264_MEM_CAPS);
        M2M_VIDEO_SET_CAPTURE_FORMAT(video, width, height, pix->pixelformat);
    } else if (format->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        uint8_t input_bpp;
        uint32_t width = M2M_VIDEO_GET_CAPTURE_FORMAT_WIDTH(video);
        uint32_t height = M2M_VIDEO_GET_CAPTURE_FORMAT_HEIGHT(video);

        if ((width && (pix->width != width)) ||
                (height && (pix->height != height))) {
            ESP_LOGE(TAG, "width or height is invalid");
            return ESP_ERR_INVALID_ARG;
        }

        ret = h264_get_input_format_from_v4l2(pix->pixelformat, &h264_video->input_format, &input_bpp);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "pixel format is invalid");
            return ret;
        }

        uint32_t buf_size = pix->width * pix->height * input_bpp / 8;

        ESP_LOGD(TAG, "output buffer size=%" PRIu32, buf_size);

        M2M_VIDEO_SET_OUTPUT_BUF_INFO(video, buf_size, H264_DMA_ALIGN_BYTES, H264_MEM_CAPS);
        M2M_VIDEO_SET_OUTPUT_FORMAT(video, width, height, pix->pixelformat);
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

static esp_err_t h264_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    esp_err_t ret;

    if (event == ESP_VIDEO_M2M_TRIGGER) {
        uint32_t type = *(uint32_t *)arg;

        if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            ret = esp_video_m2m_process(video,
                                        V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                        V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                        h264_video_m2m_process);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "failed to process M2M device data");
                return ret;
            }
        }
    }

    return ESP_OK;
}

static esp_err_t h264_video_set_ext_ctrl(struct esp_video *video, const struct v4l2_ext_controls *ctrls)
{
    esp_err_t ret = ESP_OK;
    struct h264_video *h264_video = VIDEO_PRIV_DATA(struct h264_video *, video);

    for (int i = 0; i < ctrls->count; i++) {
        struct v4l2_ext_control *ctrl = &ctrls->controls[i];

        switch (ctrl->id) {
        case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
            h264_video->gop = ctrl->value;
            break;
        case V4L2_CID_MPEG_VIDEO_BITRATE:
            h264_video->bitrate = ctrl->value;
            break;
        case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
            h264_video->min_qp = ctrl->value;
            break;
        case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
            h264_video->max_qp = ctrl->value;
            break;
        default:
            ret = ESP_ERR_NOT_SUPPORTED;
            ESP_LOGE(TAG, "id=%" PRIx32 " is not supported", ctrl->id);
            break;
        }
    }

    return ret;
}

static esp_err_t h264_video_get_ext_ctrl(struct esp_video *video, struct v4l2_ext_controls *ctrls)
{
    esp_err_t ret = ESP_OK;
    struct h264_video *h264_video = VIDEO_PRIV_DATA(struct h264_video *, video);

    for (int i = 0; i < ctrls->count; i++) {
        struct v4l2_ext_control *ctrl = &ctrls->controls[i];

        switch (ctrl->id) {
        case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
            ctrl->value = h264_video->gop;
            break;
        case V4L2_CID_MPEG_VIDEO_BITRATE:
            ctrl->value = h264_video->bitrate;
            break;
        case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
            ctrl->value = h264_video->min_qp;
            break;
        case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
            ctrl->value = h264_video->max_qp;
            break;
        default:
            ret = ESP_ERR_NOT_SUPPORTED;
            ESP_LOGE(TAG, "id=%" PRIx32 " is not supported", ctrl->id);
            break;
        }
    }

    return ret;
}

static esp_err_t h264_video_query_ext_ctrl(struct esp_video *video, struct v4l2_query_ext_ctrl *qctrl)
{
    esp_err_t ret = ESP_OK;

    switch (qctrl->id) {
    case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->maximum = H264_VIDEO_MAX_I_PERIOD;
        qctrl->minimum = H264_VIDEO_MIN_I_PERIOD;
        qctrl->step = H264_VIDEO_I_PERIOD_STEP;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = H264_VIDEO_DEVICE_GOP;
        break;
    case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER_MENU;
        qctrl->elem_size = sizeof(uint8_t);
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->dims[0] = qctrl->elem_size;
        qctrl->default_value = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
        break;
    case V4L2_CID_MPEG_VIDEO_BITRATE:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->maximum = H264_VIDEO_MAX_BITRATE;
        qctrl->minimum = H264_VIDEO_MIN_BITRATE;
        qctrl->step = H264_VIDEO_BITRATE_STEP;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = H264_VIDEO_DEVICE_BITRATE;
        break;
    case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->maximum = H264_VIDEO_MAX_QP;
        qctrl->minimum = H264_VIDEO_MIN_QP;
        qctrl->step = H264_VIDEO_QP_STEP;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = H264_VIDEO_DEVICE_MIN_QP;
        break;
    case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->maximum = H264_VIDEO_MAX_QP;
        qctrl->minimum = H264_VIDEO_MIN_QP;
        qctrl->step = H264_VIDEO_QP_STEP;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = H264_VIDEO_DEVICE_MAX_QP;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        ESP_LOGE(TAG, "id=%" PRIx32 " is not supported", qctrl->id);
        break;
    }

    return ret;
}

static const struct esp_video_ops s_h264_video_ops = {
    .init           = h264_video_init,
    .deinit         = h264_video_deinit,
    .start          = h264_video_start,
    .stop           = h264_video_stop,
    .enum_format    = h264_video_enum_format,
    .set_format     = h264_video_set_format,
    .notify         = h264_video_notify,
    .set_ext_ctrl   = h264_video_set_ext_ctrl,
    .get_ext_ctrl   = h264_video_get_ext_ctrl,
    .query_ext_ctrl = h264_video_query_ext_ctrl,
};

/**
 * @brief Create H.264 video device
 *
 * @param hw_codec true: hardware H.264, false: software H.264(has not supported)
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_h264_video_device(bool hw_codec)
{
    struct esp_video *video;
    struct h264_video *h264_video;
    uint32_t device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_EXT_PIX_FORMAT | V4L2_CAP_STREAMING;
    uint32_t caps = device_caps | V4L2_CAP_DEVICE_CAPS;

    if (hw_codec == false) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    h264_video = heap_caps_calloc(1, sizeof(struct h264_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!h264_video) {
        return ESP_ERR_NO_MEM;
    }

    h264_video->hw_codec = hw_codec;
    h264_video->gop = H264_VIDEO_DEVICE_GOP;
    h264_video->min_qp = H264_VIDEO_DEVICE_MIN_QP;
    h264_video->max_qp = H264_VIDEO_DEVICE_MAX_QP;
    h264_video->bitrate = H264_VIDEO_DEVICE_BITRATE;

    video = esp_video_create(H264_NAME, ESP_VIDEO_H264_DEVICE_ID, &s_h264_video_ops, h264_video, caps, device_caps);
    if (!video) {
        heap_caps_free(h264_video);
        return ESP_FAIL;
    }

    return ESP_OK;
}
