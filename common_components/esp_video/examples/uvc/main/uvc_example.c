/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "linux/videodev2.h"
#include "esp_video_init.h"
#include "esp_video_device.h"
#include "usb_device_uvc.h"
#include "uvc_frame_config.h"

#if CONFIG_EXAMPLE_CAM_SENSOR_MIPI_CSI
#define CAM_DEV_PATH        ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#elif CONFIG_EXAMPLE_CAM_SENSOR_DVP
#define CAM_DEV_PATH        ESP_VIDEO_DVP_DEVICE_NAME
#endif

#if CONFIG_FORMAT_MJPEG_CAM1
#define ENCODE_DEV_PATH     ESP_VIDEO_JPEG_DEVICE_NAME
#define UVC_OUTPUT_FORMAT   V4L2_PIX_FMT_JPEG
#elif CONFIG_FORMAT_H264_CAM1
#if CONFIG_EXAMPLE_H264_MAX_QP <= CONFIG_EXAMPLE_H264_MIN_QP
#error "CONFIG_EXAMPLE_H264_MAX_QP should larger than CONFIG_EXAMPLE_H264_MIN_QP"
#endif

#define ENCODE_DEV_PATH     ESP_VIDEO_H264_DEVICE_NAME
#define UVC_OUTPUT_FORMAT   V4L2_PIX_FMT_H264
#endif

#define BUFFER_COUNT        2

typedef struct uvc {
    int cap_fd;
    uint32_t format;
    uint8_t *cap_buffer[BUFFER_COUNT];

    int m2m_fd;
    uint8_t *m2m_cap_buffer;

    uvc_fb_t fb;
} uvc_t;

static const char *TAG = "example";

#if CONFIG_EXAMPLE_CAM_SENSOR_MIPI_CSI
static const esp_video_init_csi_config_t csi_config[] = {
    {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port      = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT,
                .scl_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN,
                .sda_pin   = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN,
            },
            .freq = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ,
        },
        .reset_pin = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN,
        .pwdn_pin  = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN,
    },
};
#endif

#if CONFIG_EXAMPLE_CAM_SENSOR_DVP
static const esp_video_init_dvp_config_t dvp_config[] = {
    {
        .sccb_config = {
            .init_sccb = true,
            .i2c_config = {
                .port      = CONFIG_EXAMPLE_DVP_SCCB_I2C_PORT,
                .scl_pin   = CONFIG_EXAMPLE_DVP_SCCB_I2C_SCL_PIN,
                .sda_pin   = CONFIG_EXAMPLE_DVP_SCCB_I2C_SDA_PIN,
            },
            .freq      = CONFIG_EXAMPLE_DVP_SCCB_I2C_FREQ,
        },
        .reset_pin = CONFIG_EXAMPLE_DVP_CAM_SENSOR_RESET_PIN,
        .pwdn_pin  = CONFIG_EXAMPLE_DVP_CAM_SENSOR_PWDN_PIN,
        .dvp_pin = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                CONFIG_EXAMPLE_DVP_D0_PIN, CONFIG_EXAMPLE_DVP_D1_PIN, CONFIG_EXAMPLE_DVP_D2_PIN, CONFIG_EXAMPLE_DVP_D3_PIN,
                CONFIG_EXAMPLE_DVP_D4_PIN, CONFIG_EXAMPLE_DVP_D5_PIN, CONFIG_EXAMPLE_DVP_D6_PIN, CONFIG_EXAMPLE_DVP_D7_PIN,
            },
            .vsync_io = CONFIG_EXAMPLE_DVP_VSYNC_PIN,
            .de_io = CONFIG_EXAMPLE_DVP_DE_PIN,
            .pclk_io = CONFIG_EXAMPLE_DVP_PCLK_PIN,
            .xclk_io = CONFIG_EXAMPLE_DVP_XCLK_PIN,
        },
        .xclk_freq = CONFIG_EXAMPLE_DVP_XCLK_FREQ,
    },
};
#endif

static const esp_video_init_config_t cam_config = {
#if CONFIG_EXAMPLE_CAM_SENSOR_MIPI_CSI
    .csi      = csi_config,
#endif
#if CONFIG_EXAMPLE_CAM_SENSOR_DVP
    .dvp      = dvp_config,
#endif
};

static void print_video_device_info(const struct v4l2_capability *capability)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", (uint16_t)(capability->version >> 16),
             (uint8_t)(capability->version >> 8),
             (uint8_t)capability->version);
    ESP_LOGI(TAG, "driver:  %s", capability->driver);
    ESP_LOGI(TAG, "card:    %s", capability->card);
    ESP_LOGI(TAG, "bus:     %s", capability->bus_info);
    ESP_LOGI(TAG, "capabilities:");
    if (capability->capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        ESP_LOGI(TAG, "\tVIDEO_CAPTURE");
    }
    if (capability->capabilities & V4L2_CAP_READWRITE) {
        ESP_LOGI(TAG, "\tREADWRITE");
    }
    if (capability->capabilities & V4L2_CAP_ASYNCIO) {
        ESP_LOGI(TAG, "\tASYNCIO");
    }
    if (capability->capabilities & V4L2_CAP_STREAMING) {
        ESP_LOGI(TAG, "\tSTREAMING");
    }
    if (capability->capabilities & V4L2_CAP_META_OUTPUT) {
        ESP_LOGI(TAG, "\tMETA_OUTPUT");
    }
    if (capability->capabilities & V4L2_CAP_DEVICE_CAPS) {
        ESP_LOGI(TAG, "device capabilities:");
        if (capability->device_caps & V4L2_CAP_VIDEO_CAPTURE) {
            ESP_LOGI(TAG, "\tVIDEO_CAPTURE");
        }
        if (capability->device_caps & V4L2_CAP_READWRITE) {
            ESP_LOGI(TAG, "\tREADWRITE");
        }
        if (capability->device_caps & V4L2_CAP_ASYNCIO) {
            ESP_LOGI(TAG, "\tASYNCIO");
        }
        if (capability->device_caps & V4L2_CAP_STREAMING) {
            ESP_LOGI(TAG, "\tSTREAMING");
        }
        if (capability->device_caps & V4L2_CAP_META_OUTPUT) {
            ESP_LOGI(TAG, "\tMETA_OUTPUT");
        }
    }
}

static esp_err_t init_capture_video(uvc_t *uvc)
{
    int fd;
    struct v4l2_capability capability;

    fd = open(CAM_DEV_PATH, O_RDONLY);
    assert(fd >= 0);

    ESP_ERROR_CHECK(ioctl(fd, VIDIOC_QUERYCAP, &capability));
    print_video_device_info(&capability);

    uvc->cap_fd = fd;

    return 0;
}

static esp_err_t init_codec_video(uvc_t *uvc)
{
    int fd;
    const char *devpath = ENCODE_DEV_PATH;
    struct v4l2_capability capability;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    fd = open(devpath, O_RDONLY);
    assert(fd >= 0);

    ESP_ERROR_CHECK(ioctl(fd, VIDIOC_QUERYCAP, &capability));
    print_video_device_info(&capability);

#if CONFIG_FORMAT_MJPEG_CAM1
    controls.ctrl_class = V4L2_CID_JPEG_CLASS;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_JPEG_COMPRESSION_QUALITY;
    control[0].value    = CONFIG_EXAMPLE_JPEG_COMPRESSION_QUALITY;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to set JPEG compression quality");
    }
#elif CONFIG_FORMAT_H264_CAM1
    controls.ctrl_class = V4L2_CID_CODEC_CLASS;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_MPEG_VIDEO_H264_I_PERIOD;
    control[0].value    = CONFIG_EXAMPLE_H264_I_PERIOD;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to set H.264 intra frame period");
    }

    controls.ctrl_class = V4L2_CID_CODEC_CLASS;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_MPEG_VIDEO_BITRATE;
    control[0].value    = CONFIG_EXAMPLE_H264_BITRATE;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to set H.264 bitrate");
    }

    controls.ctrl_class = V4L2_CID_CODEC_CLASS;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_MPEG_VIDEO_H264_MIN_QP;
    control[0].value    = CONFIG_EXAMPLE_H264_MIN_QP;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to set H.264 minimum quality");
    }

    controls.ctrl_class = V4L2_CID_CODEC_CLASS;
    controls.count      = 1;
    controls.controls   = control;
    control[0].id       = V4L2_CID_MPEG_VIDEO_H264_MAX_QP;
    control[0].value    = CONFIG_EXAMPLE_H264_MAX_QP;
    if (ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
        ESP_LOGW(TAG, "failed to set H.264 maximum quality");
    }
#endif

    uvc->format = UVC_OUTPUT_FORMAT;
    uvc->m2m_fd = fd;

    return 0;
}

static esp_err_t video_start_cb(uvc_format_t uvc_format, int width, int height, int rate, void *cb_ctx)
{
    int type;
    struct v4l2_buffer buf;
    struct v4l2_format format;
    struct v4l2_requestbuffers req;
    uvc_t *uvc = (uvc_t *)cb_ctx;
    uint32_t capture_fmt = 0;

    ESP_LOGD(TAG, "UVC start");

    if (uvc->format == V4L2_PIX_FMT_JPEG) {
        int fmt_index = 0;
        const uint32_t jpeg_input_formats[] = {
            V4L2_PIX_FMT_RGB565,
            V4L2_PIX_FMT_YUV422P,
            V4L2_PIX_FMT_RGB24,
            V4L2_PIX_FMT_GREY
        };
        int jpeg_input_formats_num = sizeof(jpeg_input_formats) / sizeof(jpeg_input_formats[0]);

        while (!capture_fmt) {
            struct v4l2_fmtdesc fmtdesc = {
                .index = fmt_index++,
                .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            };

            if (ioctl(uvc->cap_fd, VIDIOC_ENUM_FMT, &fmtdesc) != 0) {
                break;
            }

            for (int i = 0; i < jpeg_input_formats_num; i++) {
                if (jpeg_input_formats[i] == fmtdesc.pixelformat) {
                    capture_fmt = jpeg_input_formats[i];
                    break;
                }
            }
        }

        if (!capture_fmt) {
            ESP_LOGI(TAG, "The camera sensor output pixel format is not supported by JPEG");
            return ESP_ERR_NOT_SUPPORTED;
        }
    } else {
        capture_fmt = V4L2_PIX_FMT_YUV420;
    }

    /* Configure camera interface capture stream */

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = capture_fmt;
    ESP_ERROR_CHECK(ioctl(uvc->cap_fd, VIDIOC_S_FMT, &format));

    memset(&req, 0, sizeof(req));
    req.count  = BUFFER_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(uvc->cap_fd, VIDIOC_REQBUFS, &req));

    for (int i = 0; i < BUFFER_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        ESP_ERROR_CHECK (ioctl(uvc->cap_fd, VIDIOC_QUERYBUF, &buf));

        uvc->cap_buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                             MAP_SHARED, uvc->cap_fd, buf.m.offset);
        assert(uvc->cap_buffer[i]);

        ESP_ERROR_CHECK(ioctl(uvc->cap_fd, VIDIOC_QBUF, &buf));
    }

    /* Configure codec output stream */

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = capture_fmt;
    ESP_ERROR_CHECK(ioctl(uvc->m2m_fd, VIDIOC_S_FMT, &format));

    memset(&req, 0, sizeof(req));
    req.count  = 1;
    req.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    req.memory = V4L2_MEMORY_USERPTR;
    ESP_ERROR_CHECK(ioctl(uvc->m2m_fd, VIDIOC_REQBUFS, &req));

    /* Configure codec capture stream */

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = uvc->format;
    ESP_ERROR_CHECK(ioctl(uvc->m2m_fd, VIDIOC_S_FMT, &format));

    memset(&req, 0, sizeof(req));
    req.count  = 1;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(uvc->m2m_fd, VIDIOC_REQBUFS, &req));

    memset(&buf, 0, sizeof(buf));
    buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory      = V4L2_MEMORY_MMAP;
    buf.index       = 0;
    ESP_ERROR_CHECK (ioctl(uvc->m2m_fd, VIDIOC_QUERYBUF, &buf));

    uvc->m2m_cap_buffer = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                          MAP_SHARED, uvc->m2m_fd, buf.m.offset);
    assert(uvc->m2m_cap_buffer);

    ESP_ERROR_CHECK(ioctl(uvc->m2m_fd, VIDIOC_QBUF, &buf));

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_ERROR_CHECK(ioctl(uvc->m2m_fd, VIDIOC_STREAMON, &type));
    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ESP_ERROR_CHECK(ioctl(uvc->m2m_fd, VIDIOC_STREAMON, &type));

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_ERROR_CHECK(ioctl(uvc->cap_fd, VIDIOC_STREAMON, &type));

    return ESP_OK;
}

static void video_stop_cb(void *cb_ctx)
{
    int type;
    uvc_t *uvc = (uvc_t *)cb_ctx;

    ESP_LOGD(TAG, "UVC stop");

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(uvc->cap_fd, VIDIOC_STREAMOFF, &type);

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    ioctl(uvc->m2m_fd, VIDIOC_STREAMOFF, &type);
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(uvc->m2m_fd, VIDIOC_STREAMOFF, &type);
}

static uvc_fb_t *video_fb_get_cb(void *cb_ctx)
{
    int64_t us;
    uvc_t *uvc = (uvc_t *)cb_ctx;
    struct v4l2_format format;
    struct v4l2_buffer cap_buf;
    struct v4l2_buffer m2m_out_buf;
    struct v4l2_buffer m2m_cap_buf;

    ESP_LOGD(TAG, "UVC get");

    memset(&cap_buf, 0, sizeof(cap_buf));
    cap_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cap_buf.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(uvc->cap_fd, VIDIOC_DQBUF, &cap_buf));

    memset(&m2m_out_buf, 0, sizeof(m2m_out_buf));
    m2m_out_buf.index  = 0;
    m2m_out_buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    m2m_out_buf.memory = V4L2_MEMORY_USERPTR;
    m2m_out_buf.m.userptr = (unsigned long)uvc->cap_buffer[cap_buf.index];
    m2m_out_buf.length = cap_buf.bytesused;
    ESP_ERROR_CHECK(ioctl(uvc->m2m_fd, VIDIOC_QBUF, &m2m_out_buf));

    memset(&m2m_cap_buf, 0, sizeof(m2m_cap_buf));
    m2m_cap_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    m2m_cap_buf.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(uvc->m2m_fd, VIDIOC_DQBUF, &m2m_cap_buf));

    ESP_ERROR_CHECK(ioctl(uvc->cap_fd, VIDIOC_QBUF, &cap_buf));
    ESP_ERROR_CHECK(ioctl(uvc->m2m_fd, VIDIOC_DQBUF, &m2m_out_buf));

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ESP_ERROR_CHECK(ioctl(uvc->m2m_fd, VIDIOC_G_FMT, &format));

    uvc->fb.buf = uvc->m2m_cap_buffer;
    uvc->fb.len = m2m_cap_buf.bytesused;
    uvc->fb.width = format.fmt.pix.width;
    uvc->fb.height = format.fmt.pix.height;
    uvc->fb.format = format.fmt.pix.pixelformat == V4L2_PIX_FMT_JPEG ? UVC_FORMAT_JPEG : UVC_FORMAT_H264;

    us = esp_timer_get_time();
    uvc->fb.timestamp.tv_sec = us / 1000000UL;;
    uvc->fb.timestamp.tv_usec = us % 1000000UL;

    return &uvc->fb;
}

static void video_fb_return_cb(uvc_fb_t *fb, void *cb_ctx)
{
    struct v4l2_buffer m2m_cap_buf;
    uvc_t *uvc = (uvc_t *)cb_ctx;

    ESP_LOGD(TAG, "UVC return");

    m2m_cap_buf.index  = 0;
    m2m_cap_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    m2m_cap_buf.memory = V4L2_MEMORY_MMAP;
    ESP_ERROR_CHECK(ioctl(uvc->m2m_fd, VIDIOC_QBUF, &m2m_cap_buf));
}

static esp_err_t init_uvc(uvc_t *uvc)
{
    int index = 0;
    uvc_device_config_t config = {
        .start_cb     = video_start_cb,
        .fb_get_cb    = video_fb_get_cb,
        .fb_return_cb = video_fb_return_cb,
        .stop_cb      = video_stop_cb,
        .cb_ctx       = (void *)uvc,
    };

    config.uvc_buffer_size = UVC_FRAMES_INFO[index][0].width * UVC_FRAMES_INFO[index][0].height;
    config.uvc_buffer = malloc(config.uvc_buffer_size);
    assert(config.uvc_buffer);

    ESP_LOGI(TAG, "Format List");
    ESP_LOGI(TAG, "\tFormat(1) = %s", uvc->format == V4L2_PIX_FMT_JPEG ? "MJPEG" : "H.264");
    ESP_LOGI(TAG, "Frame List");
    ESP_LOGI(TAG, "\tFrame(1) = %d * %d @%dfps", UVC_FRAMES_INFO[index][0].width, UVC_FRAMES_INFO[index][0].height, UVC_FRAMES_INFO[index][0].rate);

    ESP_ERROR_CHECK(uvc_device_config(index, &config));
    ESP_ERROR_CHECK(uvc_device_init());

    return ESP_OK;
}

void app_main(void)
{
    uvc_t *uvc = calloc(1, sizeof(uvc_t));
    assert(uvc);

    ESP_ERROR_CHECK(esp_video_init(&cam_config));
    ESP_ERROR_CHECK(init_capture_video(uvc));
    ESP_ERROR_CHECK(init_codec_video(uvc));
    ESP_ERROR_CHECK(init_uvc(uvc));
}
