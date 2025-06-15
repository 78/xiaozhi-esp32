/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"
#include "esp_cam_sensor_types.h"
#include "driver/jpeg_encode.h"
#include "esp_video_device.h"
#include "hal/cam_ctlr_types.h"
#include "linux/videodev2.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ISP video device configuration
 */
#if CONFIG_SOC_ISP_LSC_SUPPORTED && (CONFIG_ESP32P4_REV_MIN_FULL >= 100)
#define ESP_VIDEO_ISP_DEVICE_LSC    1       /*!< ISP video device enable LSC */
#endif

/**
 * @brief MIPI-CSI state
 */
typedef struct esp_video_csi_state {
    uint32_t lane_bitrate_mbps;             /*!< MIPI-CSI data lane bitrate in Mbps */
    uint8_t lane_num;                       /*!< MIPI-CSI data lane number */
    cam_ctlr_color_t in_color;              /*!< MIPI-CSI input(from camera sensor) data color format */
    cam_ctlr_color_t out_color;             /*!< MIPI-CSI output(based on ISP output) data color format */
    uint8_t out_bpp;                        /*!< MIPI-CSI output data color format bit per pixel */
    bool line_sync;                         /*!< true: line has start and end packet; false. line has no start and end packet */
    bool bypass_isp;                        /*!< true: ISP directly output data from input port with processing. false: ISP output processed data by pipeline  */
    color_raw_element_order_t bayer_order;  /*!< Bayer order of raw data */
} esp_video_csi_state_t;

/**
 * @brief Create MIPI CSI video device
 *
 * @param cam_dev camera sensor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
esp_err_t esp_video_create_csi_video_device(esp_cam_sensor_device_t *cam_dev);
#endif

/**
 * @brief Create DVP video device
 *
 * @param cam_dev camera sensor device
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
esp_err_t esp_video_create_dvp_video_device(esp_cam_sensor_device_t *cam_dev);
#endif

/**
 * @brief Create H.264 video device
 *
 * @param hw_codec true: hardware H.264, false: software H.264(has not supported)
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
#ifdef CONFIG_ESP_VIDEO_ENABLE_H264_VIDEO_DEVICE
esp_err_t esp_video_create_h264_video_device(bool hw_codec);
#endif

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
#ifdef CONFIG_ESP_VIDEO_ENABLE_JPEG_VIDEO_DEVICE
esp_err_t esp_video_create_jpeg_video_device(jpeg_encoder_handle_t enc_handle);
#endif

#if CONFIG_ESP_VIDEO_ENABLE_ISP
/**
 * @brief Start ISP process based on MIPI-CSI state
 *
 * @param state MIPI-CSI state object
 * @param state MIPI-CSI V4L2 capture format
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_start_by_csi(const esp_video_csi_state_t *state, const struct v4l2_format *format);

/**
 * @brief Stop ISP process
 *
 * @param state MIPI-CSI state object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_stop(const esp_video_csi_state_t *state);

/**
 * @brief Enumerate ISP supported output pixel format
 *
 * @param index        Enumerated number index
 * @param pixel_format Supported output pixel format
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_enum_format(uint32_t index, uint32_t *pixel_format);

/**
 * @brief Check if input format is valid
 *
 * @param format V4L2 format object
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_isp_check_format(const struct v4l2_format *format);

#if CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE
/**
 * @brief Create ISP video device
 *
 * @param None
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_create_isp_video_device(void);
#endif
#endif

#ifdef __cplusplus
}
#endif
