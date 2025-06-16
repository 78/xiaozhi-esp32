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

#include "driver/jpeg_encode.h"

#include "esp_video.h"
#include "esp_video_device_internal.h"

#define JPEG_NAME                       "JPEG"

#define JPEG_DMA_ALIGN_BYTES            64
#define JPEG_MEM_CAPS                   (MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED)

#define JPEG_MAX_COMP_RATE              0.75

#define JPEG_VIDEO_MAX_COMP_QUALITY     100
#define JPEG_VIDEO_MIN_COMP_QUALITY     1
#define JPEG_VIDEO_COMP_QUALITY_STEP    1

#define JPEG_VIDEO_CHROMA_SUBSAMPLING   JPEG_DOWN_SAMPLING_YUV422
#define JPEG_VIDEO_COMP_QUALITY         80

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)                   sizeof(x) / sizeof((x)[0])
#endif

struct jpeg_video {
    bool jpeg_inited;
    jpeg_encoder_handle_t enc_handle;

    jpeg_enc_input_format_t src_type;
    jpeg_down_sampling_type_t sub_sample;
    uint8_t image_quality;
};

static const char *TAG = "jpeg_video";

static esp_err_t jpeg_get_input_format_from_v4l2(uint32_t v4l2_format, jpeg_enc_input_format_t *src_type, uint8_t *src_bpp, jpeg_down_sampling_type_t *sub_sample)
{
    esp_err_t ret = ESP_OK;

    switch (v4l2_format) {
    case V4L2_PIX_FMT_RGB565:
        *src_type = JPEG_ENCODE_IN_FORMAT_RGB565;
        *src_bpp = 16;
        *sub_sample = JPEG_DOWN_SAMPLING_YUV422;
        break;
    case V4L2_PIX_FMT_RGB24:
        *src_type = JPEG_ENCODE_IN_FORMAT_RGB888;
        *src_bpp = 24;
        *sub_sample = JPEG_DOWN_SAMPLING_YUV444;
        break;
    case V4L2_PIX_FMT_YUV422P:
        *src_type = JPEG_ENCODE_IN_FORMAT_YUV422;
        *src_bpp = 16;
        *sub_sample = JPEG_DOWN_SAMPLING_YUV422;
        break;
    case V4L2_PIX_FMT_GREY:
        *src_type = JPEG_ENCODE_IN_FORMAT_GRAY;
        *src_bpp = 8;
        *sub_sample = JPEG_DOWN_SAMPLING_GRAY;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static uint32_t jpeg_capture_size(uint32_t output_size)
{
    return BUF_ALIGN_SIZE((uint32_t)(output_size * JPEG_MAX_COMP_RATE), JPEG_DMA_ALIGN_BYTES);
}

static esp_err_t jpeg_video_m2m_process(struct esp_video *video, uint8_t *src, uint32_t src_size, uint8_t *dst, uint32_t dst_size, uint32_t *dst_out_size)
{
    esp_err_t ret;
    uint32_t jpeg_codeced_size;
    struct jpeg_video *jpeg_video = VIDEO_PRIV_DATA(struct jpeg_video *, video);
    jpeg_encode_cfg_t enc_config = {
        .src_type = jpeg_video->src_type,
        .sub_sample = jpeg_video->sub_sample,
        .image_quality = jpeg_video->image_quality,
        .width = M2M_VIDEO_GET_OUTPUT_FORMAT_WIDTH(video),
        .height = M2M_VIDEO_GET_OUTPUT_FORMAT_HEIGHT(video),
    };

    ret = jpeg_encoder_process(jpeg_video->enc_handle,
                               &enc_config,
                               src,
                               src_size,
                               dst,
                               dst_size,
                               &jpeg_codeced_size);
    if (ret == ESP_OK) {
        *dst_out_size = jpeg_codeced_size;
    }

    return ret;
}

static esp_err_t jpeg_video_init(struct esp_video *video)
{
    esp_err_t ret;
    struct jpeg_video *jpeg_video = VIDEO_PRIV_DATA(struct jpeg_video *, video);

    if (!jpeg_video->jpeg_inited) {
        jpeg_encode_engine_cfg_t encode_eng_cfg = {
            .intr_priority = 0,
            .timeout_ms = 40,
        };

        ret = jpeg_new_encoder_engine(&encode_eng_cfg, &jpeg_video->enc_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to create JPEG encoder");
            return ret;
        }
    }

    M2M_VIDEO_SET_CAPTURE_FORMAT(video, 0, 0, 0);
    M2M_VIDEO_SET_OUTPUT_FORMAT(video, 0, 0, 0);

    return ESP_OK;
}

static esp_err_t jpeg_video_deinit(struct esp_video *video)
{
    esp_err_t ret;
    struct jpeg_video *jpeg_video = VIDEO_PRIV_DATA(struct jpeg_video *, video);

    if (!jpeg_video->jpeg_inited) {
        ret = jpeg_del_encoder_engine(jpeg_video->enc_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to delete JPEG encoder");
            return ret;
        }

        jpeg_video->enc_handle = NULL;
    }

    return ESP_OK;
}

static esp_err_t jpeg_video_start(struct esp_video *video, uint32_t type)
{
    if ((M2M_VIDEO_GET_CAPTURE_FORMAT_WIDTH(video) != M2M_VIDEO_GET_OUTPUT_FORMAT_WIDTH(video)) ||
            (M2M_VIDEO_GET_CAPTURE_FORMAT_HEIGHT(video) != M2M_VIDEO_GET_OUTPUT_FORMAT_HEIGHT(video))) {
        ESP_LOGE(TAG, "width or height is invalid");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t jpeg_video_stop(struct esp_video *video, uint32_t type)
{
    return ESP_OK;
}

static esp_err_t jpeg_video_enum_format(struct esp_video *video, uint32_t type, uint32_t index, uint32_t *pixel_format)
{
    if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        static const uint32_t jpeg_capture_format[] = {
            V4L2_PIX_FMT_JPEG,
        };

        if (index >= ARRAY_SIZE(jpeg_capture_format)) {
            return ESP_ERR_INVALID_ARG;
        }

        *pixel_format = jpeg_capture_format[index];
    } else if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        static const uint32_t jpeg_output_format[] = {
            V4L2_PIX_FMT_RGB565,
            V4L2_PIX_FMT_RGB24,
            V4L2_PIX_FMT_YUV422P,
            V4L2_PIX_FMT_GREY,
        };

        if (index >= ARRAY_SIZE(jpeg_output_format)) {
            return ESP_ERR_INVALID_ARG;
        }

        *pixel_format = jpeg_output_format[index];
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

static esp_err_t jpeg_video_set_format(struct esp_video *video, const struct v4l2_format *format)
{
    esp_err_t ret;
    const struct v4l2_pix_format *pix = &format->fmt.pix;
    struct jpeg_video *jpeg_video = VIDEO_PRIV_DATA(struct jpeg_video *, video);

    if (format->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        uint32_t width = M2M_VIDEO_GET_OUTPUT_FORMAT_WIDTH(video);
        uint32_t height = M2M_VIDEO_GET_OUTPUT_FORMAT_HEIGHT(video);

        if ((pix->pixelformat != V4L2_PIX_FMT_JPEG) ||
                (width && (pix->width != width)) ||
                (height && (pix->height != height))) {
            ESP_LOGE(TAG, "pixel format or width or height is invalid");
            return ESP_ERR_INVALID_ARG;
        }

        uint32_t buf_size = jpeg_capture_size(M2M_VIDEO_OUTPUT_BUF_SIZE(video));
        if (!buf_size) {
            ESP_LOGE(TAG, "output buffer format should be set fistly");
            return ESP_ERR_INVALID_STATE;
        }

        ESP_LOGD(TAG, "capture buffer size=%" PRIu32, buf_size);

        M2M_VIDEO_SET_CAPTURE_FORMAT(video, width, height, pix->pixelformat);
        M2M_VIDEO_SET_CAPTURE_BUF_INFO(video, buf_size, JPEG_DMA_ALIGN_BYTES, JPEG_MEM_CAPS);
    } else if (format->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        uint8_t input_bpp;
        uint32_t width = M2M_VIDEO_GET_CAPTURE_FORMAT_WIDTH(video);
        uint32_t height = M2M_VIDEO_GET_CAPTURE_FORMAT_HEIGHT(video);

        if ((width && (pix->width != width)) ||
                (height && (pix->height != height))) {
            ESP_LOGE(TAG, "width or height is invalid");
            return ESP_ERR_INVALID_ARG;
        }

        ret = jpeg_get_input_format_from_v4l2(pix->pixelformat, &jpeg_video->src_type, &input_bpp, &jpeg_video->sub_sample);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "pixel format is invalid");
            return ret;
        }

        uint32_t buf_size = pix->width * pix->height * input_bpp / 8;

        ESP_LOGD(TAG, "output buffer size=%" PRIu32, buf_size);

        M2M_VIDEO_SET_OUTPUT_BUF_INFO(video, buf_size, JPEG_DMA_ALIGN_BYTES, JPEG_MEM_CAPS);
        M2M_VIDEO_SET_OUTPUT_FORMAT(video, width, height, pix->pixelformat);
    } else {
        return ESP_ERR_NOT_SUPPORTED;
    }

    return ESP_OK;
}

static esp_err_t jpeg_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    esp_err_t ret;

    if (event == ESP_VIDEO_M2M_TRIGGER) {
        uint32_t type = *(uint32_t *)arg;

        if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
            ret = esp_video_m2m_process(video,
                                        V4L2_BUF_TYPE_VIDEO_OUTPUT,
                                        V4L2_BUF_TYPE_VIDEO_CAPTURE,
                                        jpeg_video_m2m_process);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "failed to process M2M device data");
                return ret;
            }
        }
    }

    return ESP_OK;
}

static esp_err_t jpeg_video_set_ext_ctrl(struct esp_video *video, const struct v4l2_ext_controls *ctrls)
{
    esp_err_t ret = ESP_OK;
    struct jpeg_video *jpeg_video = VIDEO_PRIV_DATA(struct jpeg_video *, video);

    for (int i = 0; i < ctrls->count; i++) {
        struct v4l2_ext_control *ctrl = &ctrls->controls[i];

        switch (ctrl->id) {
        case V4L2_CID_JPEG_CHROMA_SUBSAMPLING:
            jpeg_video->sub_sample = ctrl->value;
            break;
        case V4L2_CID_JPEG_COMPRESSION_QUALITY:
            jpeg_video->image_quality = ctrl->value;
            break;
        default:
            ret = ESP_ERR_NOT_SUPPORTED;
            ESP_LOGE(TAG, "id=%" PRIx32 " is not supported", ctrl->id);
            break;
        }
    }

    return ret;
}

static esp_err_t jpeg_video_get_ext_ctrl(struct esp_video *video, struct v4l2_ext_controls *ctrls)
{
    esp_err_t ret = ESP_OK;
    struct jpeg_video *jpeg_video = VIDEO_PRIV_DATA(struct jpeg_video *, video);

    for (int i = 0; i < ctrls->count; i++) {
        struct v4l2_ext_control *ctrl = &ctrls->controls[i];

        switch (ctrl->id) {
        case V4L2_CID_JPEG_CHROMA_SUBSAMPLING:
            ctrl->value = jpeg_video->sub_sample;
            break;
        case V4L2_CID_JPEG_COMPRESSION_QUALITY:
            ctrl->value = jpeg_video->image_quality;
            break;
        default:
            ret = ESP_ERR_NOT_SUPPORTED;
            ESP_LOGE(TAG, "id=%" PRIx32 " is not supported", ctrl->id);
            break;
        }
    }

    return ret;
}

static esp_err_t jpeg_video_query_ext_ctrl(struct esp_video *video, struct v4l2_query_ext_ctrl *qctrl)
{
    esp_err_t ret = ESP_OK;

    switch (qctrl->id) {
    case V4L2_CID_JPEG_CHROMA_SUBSAMPLING:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER_MENU;
        qctrl->elem_size = sizeof(uint8_t);
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->dims[0] = qctrl->elem_size;
        qctrl->default_value = JPEG_VIDEO_CHROMA_SUBSAMPLING;
        break;
    case V4L2_CID_JPEG_COMPRESSION_QUALITY:
        qctrl->type = V4L2_CTRL_TYPE_INTEGER;
        qctrl->maximum = JPEG_VIDEO_MAX_COMP_QUALITY;
        qctrl->minimum = JPEG_VIDEO_MIN_COMP_QUALITY;
        qctrl->step = JPEG_VIDEO_COMP_QUALITY_STEP;
        qctrl->elems = 1;
        qctrl->nr_of_dims = 0;
        qctrl->default_value = JPEG_VIDEO_COMP_QUALITY;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        ESP_LOGE(TAG, "id=%" PRIx32 " is not supported", qctrl->id);
        break;
    }

    return ret;
}

static const struct esp_video_ops s_jpeg_video_ops = {
    .init           = jpeg_video_init,
    .deinit         = jpeg_video_deinit,
    .start          = jpeg_video_start,
    .stop           = jpeg_video_stop,
    .enum_format    = jpeg_video_enum_format,
    .set_format     = jpeg_video_set_format,
    .notify         = jpeg_video_notify,
    .set_ext_ctrl   = jpeg_video_set_ext_ctrl,
    .get_ext_ctrl   = jpeg_video_get_ext_ctrl,
    .query_ext_ctrl = jpeg_video_query_ext_ctrl,
};

/**
 * @brief Create JPEG video device
 *
 * @param enc_handle JPEG encoder driver handle,
 *      - NULL, JPEG video device will create JPEG encoder driver handle by itself
 *      - Not null, JPEG video device will use this handle instead of creating JPEG encoder driver handle
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_jpeg_video_device(jpeg_encoder_handle_t enc_handle)
{
    struct esp_video *video;
    struct jpeg_video *jpeg_video;
    uint32_t device_caps = V4L2_CAP_VIDEO_M2M | V4L2_CAP_EXT_PIX_FORMAT | V4L2_CAP_STREAMING;
    uint32_t caps = device_caps | V4L2_CAP_DEVICE_CAPS;

    jpeg_video = heap_caps_calloc(1, sizeof(struct jpeg_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!jpeg_video) {
        return ESP_ERR_NO_MEM;
    }

    if (enc_handle) {
        jpeg_video->jpeg_inited = true;
        jpeg_video->enc_handle = enc_handle;
    }
    jpeg_video->sub_sample = JPEG_VIDEO_CHROMA_SUBSAMPLING;
    jpeg_video->image_quality = JPEG_VIDEO_COMP_QUALITY;

    video = esp_video_create(JPEG_NAME, ESP_VIDEO_JPEG_DEVICE_ID, &s_jpeg_video_ops, jpeg_video, caps, device_caps);
    if (!video) {
        heap_caps_free(jpeg_video);
        return ESP_FAIL;
    }

    return ESP_OK;
}
