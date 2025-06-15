/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_attr.h"

#include "esp_cam_ctlr_dvp.h"

#include "esp_video.h"
#include "esp_video_sensor.h"
#include "esp_video_device_internal.h"

#define DVP_NAME                    "DVP"

#define DVP_CTLR_ID                 0
#define DVP_DMA_BURST_SIZE          128

#define DVP_DMA_ALIGN_BYTES         64
#define DVP_MEM_CAPS                (MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED)

struct dvp_video {
    cam_ctlr_color_t in_color;

    esp_cam_ctlr_handle_t cam_ctrl_handle;

    esp_cam_sensor_device_t *cam_dev;
};

static const char *TAG = "dvp_video";

static esp_err_t dvp_get_input_frame_type(esp_cam_sensor_output_format_t sensor_format,
        cam_ctlr_color_t *in_color,
        uint32_t *v4l2_format,
        uint32_t *bpp)
{
    esp_err_t ret = ESP_OK;

    switch (sensor_format) {
    case ESP_CAM_SENSOR_PIXFORMAT_RGB565:
        *in_color = CAM_CTLR_COLOR_RGB565;
        *v4l2_format = V4L2_PIX_FMT_RGB565;
        *bpp = 16;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_YUV422:
        *in_color = CAM_CTLR_COLOR_YUV422;
        *v4l2_format = V4L2_PIX_FMT_YUV422P;
        *bpp = 16;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RGB888:
        *in_color = CAM_CTLR_COLOR_RGB888;
        *v4l2_format = V4L2_PIX_FMT_RGB24;
        *bpp = 24;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_JPEG:
        *in_color = 0;
        *v4l2_format = V4L2_PIX_FMT_JPEG;
        *bpp = 8;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW8:
        *in_color = CAM_CTLR_COLOR_RAW8;
        *v4l2_format = V4L2_PIX_FMT_SBGGR8;
        *bpp = 8;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW10:
        *in_color = CAM_CTLR_COLOR_RAW10;
        *v4l2_format = V4L2_PIX_FMT_SBGGR10;
        *bpp = 10;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_RAW12:
        *in_color = CAM_CTLR_COLOR_RAW12;
        *v4l2_format = V4L2_PIX_FMT_SBGGR12;
        *bpp = 12;
        break;
    case ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE:
        *in_color = CAM_CTLR_COLOR_GRAY8;
        *v4l2_format = V4L2_PIX_FMT_GREY;
        *bpp = 8;
        break;
    default:
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
}

static bool IRAM_ATTR dvp_video_on_trans_finished(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    struct esp_video *video = (struct esp_video *)user_data;

    ESP_LOGD(TAG, "size=%zu", trans->received_size);

    CAPTURE_VIDEO_DONE_BUF(video, trans->buffer, trans->received_size);

    return true;
}

static bool IRAM_ATTR dvp_video_on_get_new_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    struct esp_video_buffer_element *element;
    struct esp_video *video = (struct esp_video *)user_data;

    element = CAPTURE_VIDEO_GET_QUEUED_ELEMENT(video);
    if (!element) {
        return false;
    }

    trans->buffer = element->buffer;
    trans->buflen = ELEMENT_SIZE(element);

    return true;
}

static esp_err_t init_config(struct esp_video *video)
{
    esp_err_t ret;
    uint32_t in_bpp;
    uint32_t v4l2_format;
    esp_cam_sensor_format_t sensor_format;
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);
    esp_cam_sensor_device_t *cam_dev = dvp_video->cam_dev;

    ret = esp_cam_sensor_get_format(cam_dev, &sensor_format);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = dvp_get_input_frame_type(sensor_format.format, &dvp_video->in_color, &v4l2_format, &in_bpp);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to get DVP input frame type");
        return ret;
    }

    CAPTURE_VIDEO_SET_FORMAT(video,
                             sensor_format.width,
                             sensor_format.height,
                             v4l2_format);

    uint32_t buf_size = CAPTURE_VIDEO_GET_FORMAT_WIDTH(video) * CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video) * in_bpp / 8;

    ESP_LOGD(TAG, "buffer size=%" PRIu32, buf_size);

    CAPTURE_VIDEO_SET_BUF_INFO(video, buf_size, DVP_DMA_ALIGN_BYTES, DVP_MEM_CAPS);

    return ESP_OK;
}

static esp_err_t dvp_video_init(struct esp_video *video)
{
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);

    ESP_RETURN_ON_ERROR(esp_cam_sensor_set_format(dvp_video->cam_dev, NULL), TAG, "failed to set basic format");
    ESP_RETURN_ON_ERROR(init_config(video), TAG, "failed to initialize config");

    return ESP_OK;
}

static esp_err_t dvp_video_start(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);
    esp_cam_sensor_device_t *cam_dev = dvp_video->cam_dev;

    esp_cam_ctlr_dvp_config_t dvp_config = {
        .ctlr_id = DVP_CTLR_ID,
        .clk_src = CAM_CLK_SRC_DEFAULT,
        .h_res = CAPTURE_VIDEO_GET_FORMAT_WIDTH(video),
        .v_res = CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video),
        .dma_burst_size = DVP_DMA_BURST_SIZE,
        .input_data_color_type = dvp_video->in_color,
        .pin_dont_init = true,
        .pic_format_jpeg = CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(video) == V4L2_PIX_FMT_JPEG,
    };
    ret = esp_cam_new_dvp_ctlr(&dvp_config, &dvp_video->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to create DVP");
        return ret;
    }

    esp_cam_ctlr_evt_cbs_t cam_ctrl_cbs = {
        .on_get_new_trans = dvp_video_on_get_new_trans,
        .on_trans_finished = dvp_video_on_trans_finished
    };
    ret = esp_cam_ctlr_register_event_callbacks(dvp_video->cam_ctrl_handle, &cam_ctrl_cbs, video);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to register CAM ctlr event callback");
        goto exit_0;
    }

    ret = esp_cam_ctlr_enable(dvp_video->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to enable CAM ctlr");
        goto exit_0;
    }

    ret = esp_cam_ctlr_start(dvp_video->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start CAM ctlr");
        goto exit_1;
    }

    int flags = 1;
    ret = esp_cam_sensor_ioctl(cam_dev, ESP_CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to start sensor");
        goto exit_2;
    }

    return ESP_OK;

exit_2:
    esp_cam_ctlr_stop(dvp_video->cam_ctrl_handle);
exit_1:
    esp_cam_ctlr_disable(dvp_video->cam_ctrl_handle);
exit_0:
    esp_cam_ctlr_del(dvp_video->cam_ctrl_handle);
    dvp_video->cam_ctrl_handle = NULL;
    return ret;
}

static esp_err_t dvp_video_stop(struct esp_video *video, uint32_t type)
{
    esp_err_t ret;
    int flags = 0;
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);
    esp_cam_sensor_device_t *cam_dev = dvp_video->cam_dev;

    ret = esp_cam_sensor_ioctl(cam_dev, ESP_CAM_SENSOR_IOC_S_STREAM, &flags);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to disable sensor");
        return ret;
    }

    ret = esp_cam_ctlr_stop(dvp_video->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to stop CAM ctlr");
        return ret;
    }

    ret = esp_cam_ctlr_disable(dvp_video->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to disable CAM ctlr");
        return ret;
    }

    ret = esp_cam_ctlr_del(dvp_video->cam_ctrl_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to delete cam ctlr");
        return ret;
    }

    dvp_video->cam_ctrl_handle = NULL;

    return ret;
}

static esp_err_t dvp_video_deinit(struct esp_video *video)
{
    return ESP_OK;
}

static esp_err_t dvp_video_enum_format(struct esp_video *video, uint32_t type, uint32_t index, uint32_t *pixel_format)
{
    if (index >= 1) {
        return ESP_ERR_INVALID_ARG;
    }

    *pixel_format = CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(video);

    return ESP_OK;
}

static esp_err_t dvp_video_set_format(struct esp_video *video, const struct v4l2_format *format)
{
    const struct v4l2_pix_format *pix = &format->fmt.pix;

    if (pix->width != CAPTURE_VIDEO_GET_FORMAT_WIDTH(video) ||
            pix->height != CAPTURE_VIDEO_GET_FORMAT_HEIGHT(video) ||
            pix->pixelformat != CAPTURE_VIDEO_GET_FORMAT_PIXEL_FORMAT(video)) {
        ESP_LOGE(TAG, "format is not supported");
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t dvp_video_notify(struct esp_video *video, enum esp_video_event event, void *arg)
{
    return ESP_OK;
}

static esp_err_t dvp_video_set_ext_ctrl(struct esp_video *video, const struct v4l2_ext_controls *ctrls)
{
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);

    return esp_video_set_ext_ctrls_to_sensor(dvp_video->cam_dev, ctrls);
}

static esp_err_t dvp_video_get_ext_ctrl(struct esp_video *video, struct v4l2_ext_controls *ctrls)
{
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);

    return esp_video_get_ext_ctrls_from_sensor(dvp_video->cam_dev, ctrls);
}

static esp_err_t dvp_video_query_ext_ctrl(struct esp_video *video, struct v4l2_query_ext_ctrl *qctrl)
{
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);

    return esp_video_query_ext_ctrls_from_sensor(dvp_video->cam_dev, qctrl);
}

static esp_err_t dvp_video_set_sensor_format(struct esp_video *video, const esp_cam_sensor_format_t *format)
{
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);

    ESP_RETURN_ON_ERROR(esp_cam_sensor_set_format(dvp_video->cam_dev, format), TAG, "failed to set customer format");
    ESP_RETURN_ON_ERROR(init_config(video), TAG, "failed to initialize config");

    return ESP_OK;
}

static esp_err_t dvp_video_get_sensor_format(struct esp_video *video, esp_cam_sensor_format_t *format)
{
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);

    return esp_cam_sensor_get_format(dvp_video->cam_dev, format);
}

static esp_err_t dvp_video_query_menu(struct esp_video *video, struct v4l2_querymenu *qmenu)
{
    struct dvp_video *dvp_video = VIDEO_PRIV_DATA(struct dvp_video *, video);

    return esp_video_query_menu_from_sensor(dvp_video->cam_dev, qmenu);
}

static const struct esp_video_ops s_dvp_video_ops = {
    .init          = dvp_video_init,
    .deinit        = dvp_video_deinit,
    .start         = dvp_video_start,
    .stop          = dvp_video_stop,
    .enum_format   = dvp_video_enum_format,
    .set_format    = dvp_video_set_format,
    .notify        = dvp_video_notify,
    .set_ext_ctrl  = dvp_video_set_ext_ctrl,
    .get_ext_ctrl  = dvp_video_get_ext_ctrl,
    .query_ext_ctrl = dvp_video_query_ext_ctrl,
    .set_sensor_format = dvp_video_set_sensor_format,
    .get_sensor_format = dvp_video_get_sensor_format,
    .query_menu    = dvp_video_query_menu,
};

/**
 * @brief Create DVP video device
 *
 * @param cam_dev camera sensor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_dvp_video_device(esp_cam_sensor_device_t *cam_dev)
{
    struct esp_video *video;
    struct dvp_video *dvp_video;
    uint32_t device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_EXT_PIX_FORMAT | V4L2_CAP_STREAMING;
    uint32_t caps = device_caps | V4L2_CAP_DEVICE_CAPS;

    dvp_video = heap_caps_calloc(1, sizeof(struct dvp_video), MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!dvp_video) {
        return ESP_ERR_NO_MEM;
    }

    dvp_video->cam_dev = cam_dev;

    video = esp_video_create(DVP_NAME, ESP_VIDEO_DVP_DEVICE_ID, &s_dvp_video_ops, dvp_video, caps, device_caps);
    if (!video) {
        heap_caps_free(dvp_video);
        return ESP_FAIL;
    }

    return ESP_OK;
}
