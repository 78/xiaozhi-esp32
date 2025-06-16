/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

// #undef LOG_LOCAL_LEVEL
// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_check.h"

#include "linux/videodev2.h"
#include "esp_video_pipeline_isp.h"
#include "esp_video_ioctl.h"
#include "esp_video_isp_ioctl.h"
#include "esp_ipa.h"
#include "esp_cam_sensor.h"

#define ISP_METADATA_BUFFER_COUNT   2
#define ISP_TASK_PRIORITY           11
#define ISP_TASK_STACK_SIZE         4096

#define UNUSED(x)                   (void)(x)

typedef struct esp_video_isp {
    int isp_fd;
    esp_video_isp_stats_t *isp_stats[ISP_METADATA_BUFFER_COUNT];

    int cam_fd;

    esp_ipa_pipeline_handle_t ipa_pipeline;

    esp_ipa_sensor_t sensor;
    uint32_t sensor_stats_seq;
    struct {
        uint8_t gain        : 1;
        uint8_t exposure    : 1;
        uint8_t stats       : 1;
        uint8_t awb         : 1;
    } sensor_attr;
} esp_video_isp_t;

static const char *TAG = "ISP";

/**
 * @brief Print ISP statistics data
 *
 * @param stats ISP statistics pointer.
 *
 * @return None
 */
static void print_stats_info(const esp_ipa_stats_t *stats)
{
#if LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG
    ESP_LOGD(TAG, "");
    ESP_LOGD(TAG, "Sequence: %llu", stats->seq);

    if (stats->flags & IPA_STATS_FLAGS_AWB) {
        ESP_LOGD(TAG, "Auto white balance:");
        for (int i = 0; i < ISP_AWB_REGIONS; i++) {
            const esp_ipa_stats_awb_t *awb_stats = &stats->awb_stats[i];

            ESP_LOGD(TAG, "  region:      %d", i);
            ESP_LOGD(TAG, "    counted:   %"PRIu32, awb_stats->counted);
            ESP_LOGD(TAG, "    sum_r:     %"PRIu32, awb_stats->sum_r);
            ESP_LOGD(TAG, "    sum_g:     %"PRIu32, awb_stats->sum_g);
            ESP_LOGD(TAG, "    sum_b:     %"PRIu32, awb_stats->sum_b);
        }
    }

    if (stats->flags & IPA_STATS_FLAGS_AE) {
        const esp_ipa_stats_ae_t *ae_stats = stats->ae_stats;

        ESP_LOGD(TAG, "Auto exposure:");
        for (int i = 0; i < ISP_AE_BLOCK_X_NUM; i++) {
            char print_buf[ISP_AE_BLOCK_X_NUM * 6];
            uint32_t offset = 0;

            for (int j = 0; j < ISP_AE_BLOCK_Y_NUM; j++) {
                int ret;

                ret = snprintf(print_buf + offset, sizeof(print_buf) - offset, " %3"PRIu32,
                               ae_stats[i * ISP_AE_BLOCK_Y_NUM + j].luminance);
                assert(ret > 0);
                offset += ret;
            }
            ESP_LOGD(TAG, "  [%s ]", print_buf);
        }
    }

    if (stats->flags & IPA_STATS_FLAGS_HIST) {
        const esp_ipa_stats_hist_t *hist_stats = stats->hist_stats;

        ESP_LOGD(TAG, "Histogram:");
        for (int i = 0; i < ISP_HIST_SEGMENT_NUMS; i++) {
            ESP_LOGD(TAG, "  %2d: %6"PRIu32, i, hist_stats[i].value);
        }
    }

    if (stats->flags & IPA_STATS_FLAGS_SHARPEN) {
        ESP_LOGD(TAG, "Sharpen high frequency pixel maximum value: %d", stats->sharpen_stats.value);
    }

    ESP_LOGD(TAG, "");
#endif
}

/**
 * @brief Print video device information.
 *
 * @param fd video device file description
 *
 * @return None
 */
static void print_dev_info(int fd)
{
#if LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG
    struct v4l2_capability capability;

    if (ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
        ESP_LOGE(TAG, "failed to get capability");
        return;
    }

    ESP_LOGD(TAG, "version: %d.%d.%d", (uint16_t)(capability.version >> 16),
             (uint8_t)(capability.version >> 8),
             (uint8_t)capability.version);
    ESP_LOGD(TAG, "driver:  %s", capability.driver);
    ESP_LOGD(TAG, "card:    %s", capability.card);
    ESP_LOGD(TAG, "bus:     %s", capability.bus_info);
    ESP_LOGD(TAG, "capabilities:");
    if (capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        ESP_LOGD(TAG, "\tVIDEO_CAPTURE");
    }
    if (capability.capabilities & V4L2_CAP_READWRITE) {
        ESP_LOGD(TAG, "\tREADWRITE");
    }
    if (capability.capabilities & V4L2_CAP_ASYNCIO) {
        ESP_LOGD(TAG, "\tASYNCIO");
    }
    if (capability.capabilities & V4L2_CAP_STREAMING) {
        ESP_LOGD(TAG, "\tSTREAMING");
    }
    if (capability.capabilities & V4L2_CAP_META_OUTPUT) {
        ESP_LOGD(TAG, "\tMETA_OUTPUT");
    }
    if (capability.capabilities & V4L2_CAP_DEVICE_CAPS) {
        ESP_LOGD(TAG, "device capabilities:");
        if (capability.device_caps & V4L2_CAP_VIDEO_CAPTURE) {
            ESP_LOGD(TAG, "\tVIDEO_CAPTURE");
        }
        if (capability.device_caps & V4L2_CAP_READWRITE) {
            ESP_LOGD(TAG, "\tREADWRITE");
        }
        if (capability.device_caps & V4L2_CAP_ASYNCIO) {
            ESP_LOGD(TAG, "\tASYNCIO");
        }
        if (capability.device_caps & V4L2_CAP_STREAMING) {
            ESP_LOGD(TAG, "\tSTREAMING");
        }
        if (capability.device_caps & V4L2_CAP_META_OUTPUT) {
            ESP_LOGD(TAG, "\tMETA_OUTPUT");
        }
    }
#endif
}

static void config_white_balance(esp_video_isp_t *isp, esp_ipa_metadata_t *metadata)
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];
    bool rc = metadata->flags & IPA_METADATA_FLAGS_RG;
    bool bg = metadata->flags & IPA_METADATA_FLAGS_BG;

    if (rc && bg) {
        esp_video_isp_wb_t wb = {
            .enable = true,
            .red_gain = metadata->red_gain,
            .blue_gain = metadata->blue_gain
        };

        controls.ctrl_class = V4L2_CTRL_CLASS_USER;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_USER_ESP_ISP_WB;
        control[0].p_u8     = (uint8_t *)&wb;
        if (ioctl(isp->isp_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set white balance");
        }
    } else if (rc) {
        controls.ctrl_class = V4L2_CTRL_CLASS_USER;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_RED_BALANCE;
        control[0].value    = metadata->red_gain * V4L2_CID_RED_BALANCE_DEN;
        if (ioctl(isp->isp_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set red balance");
        }
    } else if (bg) {
        controls.ctrl_class = V4L2_CTRL_CLASS_USER;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_BLUE_BALANCE;
        control[0].value    = metadata->blue_gain * V4L2_CID_BLUE_BALANCE_DEN;
        if (ioctl(isp->isp_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set blue balance");
        }
    }
}

static void config_exposure_time(esp_video_isp_t *isp, esp_ipa_metadata_t *metadata)
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    if (metadata->flags & IPA_METADATA_FLAGS_ET) {
        controls.ctrl_class = V4L2_CID_CAMERA_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_EXPOSURE_ABSOLUTE;
        control[0].value    = (int32_t)metadata->exposure / 100;
        if (ioctl(isp->cam_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set exposure time");
        } else {
            isp->sensor.cur_exposure = metadata->exposure;
        }
    }
}

static void config_pixel_gain(esp_video_isp_t *isp, esp_ipa_metadata_t *metadata)
{
    esp_err_t ret;
    int fd = isp->cam_fd;
    struct v4l2_querymenu qmenu;
    struct v4l2_query_ext_ctrl qctrl;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    if (metadata->flags & IPA_METADATA_FLAGS_GN) {
        int32_t gain_value = 0;
        int32_t index = -1;
        int32_t target_gain = 0;
        int32_t base_gain = 1;

        qctrl.id = V4L2_CID_GAIN;
        ret = ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
        if (ret) {
            ESP_LOGE(TAG, "failed to query gain");
            return;
        }

        for (int32_t i = qctrl.minimum; i < qctrl.maximum; i++) {
            int32_t gain0;
            int32_t gain1;

            qmenu.id = V4L2_CID_GAIN;
            qmenu.index = i;
            ret = ioctl(fd, VIDIOC_QUERYMENU, &qmenu);
            if (ret) {
                ESP_LOGE(TAG, "failed to query gain min menu");
                return;
            }
            gain0 = qmenu.value;

            if (i == qctrl.minimum) {
                gain_value = gain0 * metadata->gain;
                base_gain = gain0;
            }

            qmenu.id = V4L2_CID_GAIN;
            qmenu.index = i + 1;
            ret = ioctl(fd, VIDIOC_QUERYMENU, &qmenu);
            if (ret) {
                ESP_LOGE(TAG, "failed to query gain min menu");
                return;
            }
            gain1 = qmenu.value;

            if ((gain_value >= gain0) && (gain_value <= gain1)) {
                uint32_t len_1st = gain_value - gain0;
                uint32_t len_2nd = gain1 - gain_value;

                ESP_LOGD(TAG, "[%" PRIu32 ", %" PRIu32 "]", gain0, gain1);

                if (len_1st > len_2nd) {
                    index = i + 1;
                    target_gain = gain1;
                } else {
                    index = i;
                    target_gain = gain0;
                }
            }
        }

        if (index >= 0) {
            controls.ctrl_class = V4L2_CID_USER_CLASS;
            controls.count      = 1;
            controls.controls   = control;
            control[0].id       = V4L2_CID_GAIN;
            control[0].value    = index;
            if (ioctl(isp->cam_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
                ESP_LOGE(TAG, "failed to set pixel gain");
            } else {
                isp->sensor.cur_gain = (float)target_gain / base_gain;
            }
        } else {
            ESP_LOGE(TAG, "failed to find %0.4f", metadata->gain);
        }
    }
}

static void config_bayer_filter(esp_video_isp_t *isp, esp_ipa_metadata_t *metadata)
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];
    esp_video_isp_bf_t bf;

    if (metadata->flags & IPA_METADATA_FLAGS_BF) {
        bf.enable = true;
        bf.level = metadata->bf.level;
        for (int i = 0; i < ISP_BF_TEMPLATE_X_NUMS; i++) {
            for (int j = 0; j < ISP_BF_TEMPLATE_Y_NUMS; j++) {
                bf.matrix[i][j] = metadata->bf.matrix[i][j];
            }
        }

        controls.ctrl_class = V4L2_CID_USER_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_USER_ESP_ISP_BF;
        control[0].p_u8     = (uint8_t *)&bf;
        if (ioctl(isp->isp_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set bayer filter");
        }
    }
}

static void config_demosaic(esp_video_isp_t *isp, esp_ipa_metadata_t *metadata)
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];
    esp_video_isp_demosaic_t demosaic;

    if (metadata->flags & IPA_METADATA_FLAGS_DM) {
        demosaic.enable = true;
        demosaic.gradient_ratio = metadata->demosaic.gradient_ratio;

        controls.ctrl_class = V4L2_CID_USER_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_USER_ESP_ISP_DEMOSAIC;
        control[0].p_u8     = (uint8_t *)&demosaic;
        if (ioctl(isp->isp_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set demosaic");
        }
    }
}

static void config_sharpen(esp_video_isp_t *isp, esp_ipa_metadata_t *metadata)
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];
    esp_video_isp_sharpen_t sharpen;

    if (metadata->flags & IPA_METADATA_FLAGS_SH) {
        sharpen.enable = true;
        sharpen.h_thresh = metadata->sharpen.h_thresh;
        sharpen.l_thresh = metadata->sharpen.l_thresh;
        sharpen.h_coeff = metadata->sharpen.h_coeff;
        sharpen.m_coeff = metadata->sharpen.m_coeff;
        for (int i = 0; i < ISP_SHARPEN_TEMPLATE_X_NUMS; i++) {
            for (int j = 0; j < ISP_SHARPEN_TEMPLATE_Y_NUMS; j++) {
                sharpen.matrix[i][j] = metadata->sharpen.matrix[i][j];
            }
        }

        controls.ctrl_class = V4L2_CID_USER_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_USER_ESP_ISP_SHARPEN;
        control[0].p_u8     = (uint8_t *)&sharpen;
        if (ioctl(isp->isp_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set sharpen");
        }
    }
}

static void config_gamma(esp_video_isp_t *isp, esp_ipa_metadata_t *metadata)
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];
    esp_video_isp_gamma_t gamma;

    if (metadata->flags & IPA_METADATA_FLAGS_GAMMA) {
        gamma.enable = true;
        for (int i = 0; i < ISP_GAMMA_CURVE_POINTS_NUM; i++) {
            gamma.points[i].x = metadata->gamma.x[i];
            gamma.points[i].y = metadata->gamma.y[i];
        }

        controls.ctrl_class = V4L2_CID_USER_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_USER_ESP_ISP_GAMMA;
        control[0].p_u8     = (uint8_t *)&gamma;
        if (ioctl(isp->isp_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set GAMMA");
        }
    }
}

static void config_ccm(esp_video_isp_t *isp, esp_ipa_metadata_t *metadata)
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];
    esp_video_isp_ccm_t ccm;

    if (metadata->flags & IPA_METADATA_FLAGS_CCM) {
        ccm.enable = true;
        for (int i = 0; i < ISP_CCM_DIMENSION; i++) {
            for (int j = 0; j < ISP_CCM_DIMENSION; j++) {
                ccm.matrix[i][j] = metadata->ccm.matrix[i][j];
            }
        }

        controls.ctrl_class = V4L2_CID_USER_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_USER_ESP_ISP_CCM;
        control[0].p_u8     = (uint8_t *)&ccm;
        if (ioctl(isp->isp_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set CCM");
        }
    }
}

static void config_color(esp_video_isp_t *isp, esp_ipa_metadata_t *metadata)
{
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    if (metadata->flags & IPA_METADATA_FLAGS_BR) {
        controls.ctrl_class = V4L2_CID_USER_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_BRIGHTNESS;
        control[0].value    = metadata->brightness;
        if (ioctl(isp->isp_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set brightness");
        }
    }

    if (metadata->flags & IPA_METADATA_FLAGS_CN) {
        controls.ctrl_class = V4L2_CID_USER_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_CONTRAST;
        control[0].value    = metadata->contrast;
        if (ioctl(isp->isp_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set contrast");
        }
    }

    if (metadata->flags & IPA_METADATA_FLAGS_ST) {
        controls.ctrl_class = V4L2_CID_USER_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_SATURATION;
        control[0].value    = metadata->saturation;
        if (ioctl(isp->isp_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set saturation");
        }
    }

    if (metadata->flags & IPA_METADATA_FLAGS_HUE) {
        controls.ctrl_class = V4L2_CID_USER_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_HUE;
        control[0].value    = metadata->hue;
        if (ioctl(isp->isp_fd, VIDIOC_S_EXT_CTRLS, &controls) != 0) {
            ESP_LOGE(TAG, "failed to set hue");
        }
    }
}

static void config_isp_and_camera(esp_video_isp_t *isp, esp_ipa_metadata_t *metadata)
{
    if (!isp->sensor_attr.awb) {
        config_white_balance(isp, metadata);
    }

    config_bayer_filter(isp, metadata);
    config_demosaic(isp, metadata);
    config_sharpen(isp, metadata);
    config_gamma(isp, metadata);
    config_ccm(isp, metadata);
    config_color(isp, metadata);

    if (isp->sensor_attr.exposure) {
        config_exposure_time(isp, metadata);
    }
    if (isp->sensor_attr.gain) {
        config_pixel_gain(isp, metadata);
    }
}

static void isp_stats_to_ipa_stats(esp_video_isp_stats_t *isp_stat, esp_ipa_stats_t *ipa_stats)
{
    ipa_stats->flags = 0;
    ipa_stats->seq = isp_stat->seq;

    if (isp_stat->flags & ESP_VIDEO_ISP_STATS_FLAG_AE) {
        esp_ipa_stats_ae_t *ipa_ae = &ipa_stats->ae_stats[0];
        isp_ae_result_t *isp_ae = &isp_stat->ae.ae_result;

        for (int i = 0; i < ISP_AE_BLOCK_X_NUM; i++) {
            for (int j = 0; j < ISP_AE_BLOCK_Y_NUM; j++) {
                ipa_ae[i * ISP_AE_BLOCK_Y_NUM + j].luminance = isp_ae->luminance[i][j];
            }
        }
        ipa_stats->flags |= IPA_STATS_FLAGS_AE;
    }

    if (isp_stat->flags & ESP_VIDEO_ISP_STATS_FLAG_AWB) {
        esp_ipa_stats_awb_t *ipa_awb = &ipa_stats->awb_stats[0];
        isp_awb_stat_result_t *isp_awb = &isp_stat->awb.awb_result;

        ipa_awb->counted = isp_awb->white_patch_num;
        ipa_awb->sum_r = isp_awb->sum_r;
        ipa_awb->sum_g = isp_awb->sum_g;
        ipa_awb->sum_b = isp_awb->sum_b;
        ipa_stats->flags |= IPA_STATS_FLAGS_AWB;
    }

    if (isp_stat->flags & ESP_VIDEO_ISP_STATS_FLAG_HIST) {
        esp_ipa_stats_hist_t *ipa_hist = &ipa_stats->hist_stats[0];
        isp_hist_result_t *isp_hist = &isp_stat->hist.hist_result;

        for (int i = 0; i < ISP_HIST_SEGMENT_NUMS; i++) {
            ipa_hist[i].value = isp_hist->hist_value[i];
        }
        ipa_stats->flags |= IPA_STATS_FLAGS_HIST;
    }

    if (isp_stat->flags & ESP_VIDEO_ISP_STATS_FLAG_SHARPEN) {
        esp_ipa_stats_sharpen_t *ipa_sharpen = &ipa_stats->sharpen_stats;
        esp_isp_sharpen_evt_data_t *isp_sharpen = &isp_stat->sharpen;

        ipa_sharpen->value = isp_sharpen->high_freq_pixel_max;
        ipa_stats->flags |= IPA_STATS_FLAGS_SHARPEN;
    }
}

static void get_sensor_state(esp_video_isp_t *isp, int index)
{
    int ret;
    struct v4l2_format format;

    if (isp->sensor_attr.awb) {
        isp->isp_stats[index]->flags &= ~ESP_VIDEO_ISP_STATS_FLAG_AWB;
    }

    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(isp->cam_fd, VIDIOC_G_FMT, &format);
    if (ret == 0) {
        isp->sensor.width = format.fmt.pix.width;
        isp->sensor.height = format.fmt.pix.width;
    }

    if (isp->sensor_attr.stats) {
        struct v4l2_ext_controls controls;
        struct v4l2_ext_control control[1];
        esp_cam_sensor_stats_t sensor_stats;

        controls.ctrl_class = V4L2_CID_CAMERA_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_CAMERA_STATS;
        control[0].p_u8     = (uint8_t *)&sensor_stats;
        control[0].size     = sizeof(sensor_stats);
        ret = ioctl(isp->cam_fd, VIDIOC_G_EXT_CTRLS, &controls);
        if (ret == 0) {
            if (isp->sensor_stats_seq != sensor_stats.seq) {
                if (sensor_stats.flags & ESP_CAM_SENSOR_STATS_FLAG_AGC_GAIN) {
                    isp->sensor.cur_gain = sensor_stats.agc_gain;
                }

                if (sensor_stats.flags & ESP_CAM_SENSOR_STATS_FLAG_WB_GAIN) {
                    isp_awb_stat_result_t *awb = &isp->isp_stats[index]->awb.awb_result;

                    isp->isp_stats[index]->flags |= ESP_VIDEO_ISP_STATS_FLAG_AWB;
                    awb->white_patch_num = 1;
                    awb->sum_r = sensor_stats.wb_avg.red_avg;
                    awb->sum_g = sensor_stats.wb_avg.green_avg;
                    awb->sum_b = sensor_stats.wb_avg.blue_avg;
                }

                isp->sensor_stats_seq = sensor_stats.seq;
            }
        }
    }
}

static void isp_task(void *p)
{
    esp_err_t ret;
    struct v4l2_buffer buf;
    esp_ipa_stats_t ipa_stats;
    esp_ipa_metadata_t metadata;
    esp_video_isp_t *isp = (esp_video_isp_t *)p;

    while (1) {
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_META_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        if (ioctl(isp->isp_fd, VIDIOC_DQBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to receive video frame");
            continue;
        }

        get_sensor_state(isp, buf.index);

        isp_stats_to_ipa_stats(isp->isp_stats[buf.index], &ipa_stats);
        if (ioctl(isp->isp_fd, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "failed to queue video frame");
        }
        print_stats_info(&ipa_stats);

        metadata.flags = 0;
        ret = esp_ipa_pipeline_process(isp->ipa_pipeline, &ipa_stats, &isp->sensor, &metadata);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to process image algorithm");
            continue;
        }

        config_isp_and_camera(isp, &metadata);
    }

    vTaskDelete(NULL);
}

static esp_err_t init_cam_dev(const esp_video_isp_config_t *config, esp_video_isp_t *isp)
{
    int fd;
    esp_err_t ret;
    struct v4l2_query_ext_ctrl qctrl;
    struct v4l2_ext_controls controls;
    struct v4l2_ext_control control[1];

    fd = open(config->cam_dev, O_RDWR);
    ESP_RETURN_ON_FALSE(fd > 0, ESP_ERR_INVALID_ARG, TAG, "failed to open %s", config->cam_dev);
    print_dev_info(fd);

    qctrl.id = V4L2_CID_GAIN;
    ret = ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
    if (ret == 0) {
        controls.ctrl_class = V4L2_CID_USER_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_GAIN;
        control[0].value    = qctrl.default_value;
        ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls);
        ESP_GOTO_ON_FALSE(ret == 0, ESP_ERR_NOT_SUPPORTED, fail_0, TAG, "failed to set gain");

        isp->sensor.min_gain = 1.0;
        if (qctrl.type == V4L2_CTRL_TYPE_INTEGER) {
            isp->sensor.max_gain  = (float)qctrl.maximum / qctrl.minimum;
            isp->sensor.cur_gain  = (float)control[0].value / qctrl.minimum;
            isp->sensor.step_gain = (float)qctrl.step / qctrl.minimum;
        } else if (qctrl.type == V4L2_CTRL_TYPE_INTEGER_MENU) {
            int64_t min;
            struct v4l2_querymenu qmenu;

            qmenu.id = V4L2_CID_GAIN;
            qmenu.index = qctrl.minimum;
            ret = ioctl(fd, VIDIOC_QUERYMENU, &qmenu);
            ESP_GOTO_ON_FALSE(ret == 0, ESP_ERR_NOT_SUPPORTED, fail_0, TAG, "failed to query gain min menu");
            min = qmenu.value;

            qmenu.index = qctrl.maximum;
            ret = ioctl(fd, VIDIOC_QUERYMENU, &qmenu);
            ESP_GOTO_ON_FALSE(ret == 0, ESP_ERR_NOT_SUPPORTED, fail_0, TAG, "failed to query gain max menu");
            isp->sensor.max_gain = (float)qmenu.value / min;

            qmenu.index = control[0].value;
            ret = ioctl(fd, VIDIOC_QUERYMENU, &qmenu);
            ESP_GOTO_ON_FALSE(ret == 0, ESP_ERR_NOT_SUPPORTED, fail_0, TAG, "failed to query gain current menu");
            isp->sensor.cur_gain = (float)qmenu.value / min;

            isp->sensor.step_gain = 0.0;
        }

        isp->sensor_attr.gain = 1;

        ESP_LOGD(TAG, "Sensor gain:");
        ESP_LOGD(TAG, "  min:     %0.4f", isp->sensor.min_gain);
        ESP_LOGD(TAG, "  max:     %0.4f", isp->sensor.max_gain);
        ESP_LOGD(TAG, "  step:    %0.4f", isp->sensor.step_gain);
        ESP_LOGD(TAG, "  current: %0.4f", isp->sensor.cur_gain);
    } else {
        ESP_LOGD(TAG, "V4L2_CID_GAIN is not supported");
    }

    qctrl.id = V4L2_CID_EXPOSURE_ABSOLUTE;
    ret = ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
    if (ret == 0) {
        controls.ctrl_class = V4L2_CID_CAMERA_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_EXPOSURE_ABSOLUTE;
        control[0].value    = qctrl.default_value;
        ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &controls);
        ESP_GOTO_ON_FALSE(ret == 0, ESP_ERR_NOT_SUPPORTED, fail_0, TAG, "failed to set exposure time");

        isp->sensor.min_exposure = qctrl.minimum * 100;
        isp->sensor.max_exposure = qctrl.maximum * 100;
        isp->sensor.step_exposure = qctrl.step * 100;
        isp->sensor.cur_exposure = control[0].value * 100;

        isp->sensor_attr.exposure = 1;

        ESP_LOGD(TAG, "Exposure time:");
        ESP_LOGD(TAG, "  min:     %"PRIi64, qctrl.minimum);
        ESP_LOGD(TAG, "  max:     %"PRIi64, qctrl.maximum);
        ESP_LOGD(TAG, "  step:    %"PRIu64, qctrl.step);
        ESP_LOGD(TAG, "  current: %"PRIi32, control[0].value);
    } else {
        ESP_LOGD(TAG, "V4L2_CID_EXPOSURE_ABSOLUTE is not supported");
    }

    qctrl.id = V4L2_CID_CAMERA_STATS;
    ret = ioctl(fd, VIDIOC_QUERY_EXT_CTRL, &qctrl);
    if (ret == 0) {
        esp_cam_sensor_stats_t sensor_stats;

        controls.ctrl_class = V4L2_CID_CAMERA_CLASS;
        controls.count      = 1;
        controls.controls   = control;
        control[0].id       = V4L2_CID_CAMERA_STATS;
        control[0].p_u8     = (uint8_t *)&sensor_stats;
        control[0].size     = sizeof(sensor_stats);
        ret = ioctl(fd, VIDIOC_G_EXT_CTRLS, &controls);
        ESP_GOTO_ON_FALSE(ret == 0, ESP_ERR_NOT_SUPPORTED, fail_0, TAG, "failed to get sensor statistics");

        if (sensor_stats.flags & ESP_CAM_SENSOR_STATS_FLAG_WB_GAIN) {
            isp->sensor_attr.awb = 1;
        }

        isp->sensor_attr.stats = 1;
    } else {
        ESP_LOGD(TAG, "V4L2_CID_CAMERA_STATS is not supported");
    }

    isp->cam_fd = fd;

    return ESP_OK;

fail_0:
    close(fd);
    return ret;
}

static esp_err_t init_isp_dev(const esp_video_isp_config_t *config, esp_video_isp_t *isp)
{
    int fd;
    esp_err_t ret;
    struct v4l2_requestbuffers req;
    int type = V4L2_BUF_TYPE_META_CAPTURE;

    fd = open(config->isp_dev, O_RDWR);
    ESP_RETURN_ON_FALSE(fd > 0, ESP_ERR_INVALID_ARG, TAG, "failed to open %s", config->isp_dev);
    print_dev_info(fd);

    memset(&req, 0, sizeof(req));
    req.count  = ISP_METADATA_BUFFER_COUNT;
    req.type   = type;
    req.memory = V4L2_MEMORY_MMAP;
    ret = ioctl(fd, VIDIOC_REQBUFS, &req);
    ESP_GOTO_ON_FALSE(ret == 0, ESP_FAIL, fail_0, TAG, "failed to require buffer");

    for (int i = 0; i < ISP_METADATA_BUFFER_COUNT; i++) {
        struct v4l2_buffer buf;

        memset(&buf, 0, sizeof(buf));
        buf.type        = type;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        ret = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        ESP_GOTO_ON_FALSE(ret == 0, ESP_FAIL, fail_0, TAG, "failed to query buffer");

        isp->isp_stats[i] = (esp_video_isp_stats_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, buf.m.offset);
        ESP_GOTO_ON_FALSE(isp->isp_stats[i] != NULL, ESP_FAIL, fail_0, TAG, "failed to map buffer");

        ret = ioctl(fd, VIDIOC_QBUF, &buf);
        ESP_GOTO_ON_FALSE(ret == 0, ESP_FAIL, fail_0, TAG, "failed to queue buffer");
    }

    ret = ioctl(fd, VIDIOC_STREAMON, &type);
    ESP_GOTO_ON_FALSE(ret == 0, ESP_FAIL, fail_0, TAG, "failed to start stream");

    isp->isp_fd = fd;

    return ESP_OK;

fail_0:
    close(fd);
    return ret;
}

/**
 * @brief Initialize and start ISP system module.
 *
 * @param config ISP configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_pipeline_init(const esp_video_isp_config_t *config)
{
    esp_err_t ret;
    esp_video_isp_t *isp;
    esp_ipa_metadata_t metadata;

#if LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif

    if (!config || !config->isp_dev || !config->cam_dev ||
            !config->ipa_config) {
        ESP_LOGE(TAG, "failed to check ISP configuration");
        return ESP_ERR_INVALID_ARG;
    }

    isp = calloc(1, sizeof(esp_video_isp_t));
    ESP_RETURN_ON_FALSE(isp, ESP_ERR_NO_MEM, TAG, "failed to malloc isp");

    ESP_GOTO_ON_ERROR(esp_ipa_pipeline_create(config->ipa_config, &isp->ipa_pipeline),
                      fail_0, TAG, "failed to create IPA pipeline");

    ESP_GOTO_ON_ERROR(init_cam_dev(config, isp), fail_1, TAG, "failed to initialize camera device");
    ESP_GOTO_ON_ERROR(init_isp_dev(config, isp), fail_2, TAG, "failed to initialize ISP device");

    metadata.flags = 0;
    ESP_GOTO_ON_ERROR(esp_ipa_pipeline_init(isp->ipa_pipeline, &isp->sensor, &metadata),
                      fail_3, TAG, "failed to initialize IPA pipeline");
    config_isp_and_camera(isp, &metadata);

    ESP_GOTO_ON_FALSE(xTaskCreate(isp_task, "isp_task", ISP_TASK_STACK_SIZE, isp, ISP_TASK_PRIORITY, NULL) == pdPASS,
                      ESP_ERR_NO_MEM, fail_3, TAG, "failed to create ISP task");

    return ESP_OK;

fail_3:
    close(isp->isp_fd);
fail_2:
    close(isp->cam_fd);
fail_1:
    esp_ipa_pipeline_destroy(isp->ipa_pipeline);
fail_0:
    free(isp);
    return ret;
}
