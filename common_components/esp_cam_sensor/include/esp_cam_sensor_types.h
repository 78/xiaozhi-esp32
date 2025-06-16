/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_sccb_intf.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

typedef enum {
    ESP_CAM_SENSOR_PIXFORMAT_RGB565 = 1,
    ESP_CAM_SENSOR_PIXFORMAT_YUV422,
    ESP_CAM_SENSOR_PIXFORMAT_YUV420,
    ESP_CAM_SENSOR_PIXFORMAT_RGB888,
    ESP_CAM_SENSOR_PIXFORMAT_RGB444,
    ESP_CAM_SENSOR_PIXFORMAT_RGB555,
    ESP_CAM_SENSOR_PIXFORMAT_BGR888,
    ESP_CAM_SENSOR_PIXFORMAT_RAW8,
    ESP_CAM_SENSOR_PIXFORMAT_RAW10,
    ESP_CAM_SENSOR_PIXFORMAT_RAW12,
    ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE,
    ESP_CAM_SENSOR_PIXFORMAT_JPEG
} esp_cam_sensor_output_format_t;

#define ESP_CAM_SENSOR_STATS_FLAG_WB_GAIN           (1 <<  0)
#define ESP_CAM_SENSOR_STATS_FLAG_AGC_GAIN          (1 <<  1)

#define ESP_CAM_SENSOR_PARAM_TYPE_NUMBER            1
#define ESP_CAM_SENSOR_PARAM_TYPE_BITMASK           2
#define ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION       3
#define ESP_CAM_SENSOR_PARAM_TYPE_STRING            4
#define ESP_CAM_SENSOR_PARAM_TYPE_U8                5

#define ESP_CAM_SENSOR_PARAM_FLAG_READ_ONLY         (1 <<  0)
#define ESP_CAM_SENSOR_PARAM_FLAG_WRITE_ONLY        (1 <<  1)

#define ESP_CAM_SENSOR_NULL_POINTER_CHECK(tag, p)   ESP_RETURN_ON_FALSE((p), ESP_ERR_INVALID_ARG, tag, "input parameter '"#p"' is NULL")

#define ESP_CAM_SENSOR_ERR_BASE                     0x30000
#define ESP_CAM_SENSOR_ERR_NOT_DETECTED             (ESP_CAM_SENSOR_ERR_BASE + 1)
#define ESP_CAM_SENSOR_ERR_NOT_SUPPORTED            (ESP_CAM_SENSOR_ERR_BASE + 2)
#define ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT        (ESP_CAM_SENSOR_ERR_BASE + 3)
#define ESP_CAM_SENSOR_ERR_FAILED_SET_REG           (ESP_CAM_SENSOR_ERR_BASE + 4)
#define ESP_CAM_SENSOR_ERR_FAILED_GET_REG           (ESP_CAM_SENSOR_ERR_BASE + 5)
#define ESP_CAM_SENSOR_ERR_FAILED_RESET             (ESP_CAM_SENSOR_ERR_BASE + 6)

#define SENSOR_ISP_INFO_VERSION_DEFAULT             (1)

#define ESP_CAM_SENSOR_CLASS_SHIFT                  16  /*!< Camera sensor class left shift bits, and length of class is 8 bits */
#define ESP_CAM_SENSOR_ID_SHIFT                     0   /*!< Camera sensor class id left shift bits, and length of ID is 16 bits */
#define ESP_CAM_SENSOR_CLASS_ID(class, id)          (((class) << ESP_CAM_SENSOR_CLASS_SHIFT) | ((id) << ESP_CAM_SENSOR_ID_SHIFT)) /*!< Transform camera sensor class and ID to uint32_t type value */
#define ESP_CAM_SENSOR_CID_GET_CLASS(val)           (((val) >> ESP_CAM_SENSOR_CLASS_SHIFT) & 0xff) /*!< Get camera sensor class from uint32_t type value */
#define ESP_CAM_SENSOR_CID_GET_ID(val)              (((val) >> ESP_CAM_SENSOR_ID_SHIFT) & 0xffff) /*!< Get camera sensor class ID from uint32_t type value */

#define ESP_CAM_SENSOR_IOC_ID_SHIFT                 0
#define ESP_CAM_SENSOR_IOC_ARG_LEN_SHIFT            16
#define ESP_CAM_SENSOR_IOC(cmd, len)                (((cmd) << ESP_CAM_SENSOR_IOC_ID_SHIFT) | ((len) << ESP_CAM_SENSOR_IOC_ARG_LEN_SHIFT))
#define ESP_CAM_SENSOR_IOC_GET_ID(val)              (((val) >> ESP_CAM_SENSOR_IOC_ID_SHIFT) & 0xffff)
#define ESP_CAM_SENSOR_IOC_GET_ARG(val)             (((val) >> ESP_CAM_SENSOR_IOC_ARG_LEN_SHIFT) & 0xffff)

#define ESP_CAM_SENSOR_CID_CLASS_USER               1   /*!< Camera sensor user definition control ID class */
#define ESP_CAM_SENSOR_CID_CLASS_DEFAULT            2   /*!< Camera sensor default control ID class */
#define ESP_CAM_SENSOR_CID_CLASS_3A                 3   /*!< Camera sensor 3A control ID class */
#define ESP_CAM_SENSOR_CID_CLASS_LENS               4   /*!< Camera sensor lens control ID class */
#define ESP_CAM_SENSOR_CID_CLASS_LED                5   /*!< Camera sensor flash LED control ID class */

/**
 * @brief Camera sensor default class's control ID
 */
#define ESP_CAM_SENSOR_POWER                        ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x01)    /*!< Controller camera sensor power */
#define ESP_CAM_SENSOR_XCLK                         ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x02)    /*!< For sensors that require a clock provided by the base board */
#define ESP_CAM_SENSOR_SENSOR_MODE                  ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x03)    /*!< Night mode, sunshine mode, etc */
#define ESP_CAM_SENSOR_FPS                          ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x04)    /*!< Frame rate output by the sensor(Frames per second) */
#define ESP_CAM_SENSOR_BRIGHTNESS                   ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x05)
#define ESP_CAM_SENSOR_CONTRAST                     ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x06)
#define ESP_CAM_SENSOR_SATURATION                   ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x07)
#define ESP_CAM_SENSOR_HUE                          ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x08)
#define ESP_CAM_SENSOR_GAMMA                        ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x09)
#define ESP_CAM_SENSOR_HMIRROR                      ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x0a)
#define ESP_CAM_SENSOR_VFLIP                        ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x0b)
#define ESP_CAM_SENSOR_SHARPNESS                    ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x0c)
#define ESP_CAM_SENSOR_DENOISE                      ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x0d)
#define ESP_CAM_SENSOR_DPC                          ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x0e)    /*!< Dead Pixel Correction from the sensor */
#define ESP_CAM_SENSOR_JPEG_QUALITY                 ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x0f)
#define ESP_CAM_SENSOR_BLC                          ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x10)    /*!< Black level Correction from the sensor */
#define ESP_CAM_SENSOR_SPECIAL_EFFECT               ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x11)
#define ESP_CAM_SENSOR_LENC                         ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x12)    /*!< Len Shading Correction from the sensor */
#define ESP_CAM_SENSOR_SCENE                        ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_DEFAULT, 0x13)

/**
 * @brief Camera sensor 3A class's control ID
 */
#define ESP_CAM_SENSOR_AWB                          ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x01)   /*!< Auto white balance */
#define ESP_CAM_SENSOR_EXPOSURE_VAL                 ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x02)   /*!< Exposure target value */
#define ESP_CAM_SENSOR_DGAIN                        ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x03)   /*!< Dight gain */
#define ESP_CAM_SENSOR_ANGAIN                       ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x04)   /*!< Analog gain */
#define ESP_CAM_SENSOR_AE_CONTROL                   ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x05)   /*!< Automatic exposure control */
#define ESP_CAM_SENSOR_AGC                          ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x06)   /*!< Automatic gain control */
#define ESP_CAM_SENSOR_AF_AUTO                      ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x07)   /*!< Auto Focus */
#define ESP_CAM_SENSOR_AF_INIT                      ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x08)
#define ESP_CAM_SENSOR_AF_RELEASE                   ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x09)
#define ESP_CAM_SENSOR_AF_START                     ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x10)
#define ESP_CAM_SENSOR_AF_STOP                      ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x11)
#define ESP_CAM_SENSOR_AF_STATUS                    ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x12)  /*!< Auto focus status */
#define ESP_CAM_SENSOR_WB                           ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x13)  /*!< White balance mode */
#define ESP_CAM_SENSOR_3A_LOCK                      ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x14)  /*!< AF&AE&AWB sync lock*/
#define ESP_CAM_SENSOR_INT_TIME                     ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x15)  /*!< Integral time */
#define ESP_CAM_SENSOR_AE_LEVEL                     ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x16)  /*!< Automatic exposure level */
#define ESP_CAM_SENSOR_GAIN                         ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x17)  /*!< Absolute gain (analog gain + digital gain) */
#define ESP_CAM_SENSOR_STATS                        ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x18)  /*!< Camera sensor gain & wb statistical data */
#define ESP_CAM_SENSOR_AE_FLICKER                   ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x19)  /*!< Anti banding flicker */
#define ESP_CAM_SENSOR_GROUP_EXP_GAIN               ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x1a)  /*!< Pack a group of (exposure and gain)registers to be effective at a specific time */
#define ESP_CAM_SENSOR_EXPOSURE_US                  ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x1b)  /*!< Exposure time in us(microseconds) */
#define ESP_CAM_SENSOR_AUTO_N_PRESET_WB             ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_3A, 0x20)  /*!< Pre set white balance mode when automatic white balance is not enabled */

/**
 * @brief Camera sensor lens class's control ID
 */
#define ESP_CAM_SENSOR_LENS                         ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_LENS, 0x01)

/**
 * @brief Camera sensor flash LED class's control ID
 */
#define ESP_CAM_SENSOR_FLASH_LED                    ESP_CAM_SENSOR_CLASS_ID(ESP_CAM_SENSOR_CID_CLASS_LED, 0x01)

/**
 * @brief Camera sensor command
 */
#define ESP_CAM_SENSOR_IOC_HW_RESET                 ESP_CAM_SENSOR_IOC(0x01, 0)
#define ESP_CAM_SENSOR_IOC_SW_RESET                 ESP_CAM_SENSOR_IOC(0x02, 0)
#define ESP_CAM_SENSOR_IOC_S_TEST_PATTERN           ESP_CAM_SENSOR_IOC(0x03, sizeof(int))
#define ESP_CAM_SENSOR_IOC_S_STREAM                 ESP_CAM_SENSOR_IOC(0x04, sizeof(int))
#define ESP_CAM_SENSOR_IOC_S_SUSPEND                ESP_CAM_SENSOR_IOC(0x05, sizeof(int))
#define ESP_CAM_SENSOR_IOC_G_CHIP_ID                ESP_CAM_SENSOR_IOC(0x06, sizeof(esp_cam_sensor_id_t))
#define ESP_CAM_SENSOR_IOC_S_REG                    ESP_CAM_SENSOR_IOC(0x07, sizeof(esp_cam_sensor_reg_val_t))
#define ESP_CAM_SENSOR_IOC_G_REG                    ESP_CAM_SENSOR_IOC(0x08, sizeof(esp_cam_sensor_reg_val_t))
#define ESP_CAM_SENSOR_IOC_S_GAIN                   ESP_CAM_SENSOR_IOC(0x09, sizeof(uint8_t))

/*
 * @biref Camera sensor parameter description
 */
typedef struct esp_cam_sensor_param_desc {
    uint32_t id;                                    /*!< Camera sensor parameter ID */
    char *name;                                     /*!< Camera sensor parameter name */
    uint32_t type;                                  /*!< Camera sensor parameter type(number/bitmask/enum/string) */
    uint32_t flags;                                 /*!< Camera sensor parameter flags */

    union {
        struct {
            int32_t minimum;                        /*!< Camera sensor number type parameter supported minimum value */
            int32_t maximum;                        /*!< Camera sensor number type parameter supported maximum value */
            uint32_t step;                          /*!< Camera sensor number type parameter supported step value */
        } number;

        struct {
            uint32_t value;                         /*!< Camera sensor bitmask type parameter supported all masked value */
        } bitmask;

        struct {
            uint32_t count;                         /*!< Camera sensor enumeration type parameter supported elements count */
            const uint32_t *elements;               /*!< Camera sensor enumeration type parameter supported elements buffer pointer */
        } enumeration;

        struct {
            uint32_t size;                          /*!< size of the data type */
        } u8;
    };

    int32_t default_value;                          /*!< Camera sensor parameter default value */
} esp_cam_sensor_param_desc_t;

/**
 * @brief Sensor set/get register value parameters.
 */
typedef struct {
    uint32_t regaddr;   /*!< Register address */
    uint32_t value;     /*!< Register value */
} esp_cam_sensor_reg_val_t;

/**
 * @brief Structure to store camera sensor ID info.
 */
typedef struct {
    uint8_t midh; /*!< main ID high byte */
    uint8_t midl; /*!< main ID low byte */
    uint16_t pid; /*!< product ID */
    uint8_t ver;  /*!< version */
} esp_cam_sensor_id_t;

/**
 * @brief Camera sensor bayer pattern type.
 */
typedef enum {
    ESP_CAM_SENSOR_BAYER_RGGB = 0,
    ESP_CAM_SENSOR_BAYER_GRBG = 1,
    ESP_CAM_SENSOR_BAYER_GBRG = 2,
    ESP_CAM_SENSOR_BAYER_BGGR = 3,
    ESP_CAM_SENSOR_BAYER_MONO          /*!< No bayer pattern, just for MONO(Support Only Y output) sensor */
} esp_cam_sensor_bayer_pattern_t;

/**
 * @brief Output interface used by the camera sensor.
 */
typedef enum {
    ESP_CAM_SENSOR_DVP,      /*!< LCD_CAM DVP or Parally DVP(ISP-connected) port */
    ESP_CAM_SENSOR_MIPI_CSI, /*!< MIPI-CSI port */
} esp_cam_sensor_port_t;

/**
 * @brief Structure to store parameters required to initialize MIPI-CSI RX.
 */
typedef struct {
    uint32_t mipi_clk;     /*!< Frequency of MIPI-RX clock Lane, in Hz */
    uint32_t hs_settle;    /*!< HS-RX settle time */
    uint32_t lane_num;     /*!< data lane num */
    bool line_sync_en;     /*!< Send line short packet for each line */
} esp_cam_sensor_mipi_info_t;

/**
 * @brief Description of ISP related parameters corresponding to the specified format.
 *
 * @note For sensors that output RAW format, it is used to provide parameter information required by the ISP module.
 *       For modules with internal ISP modules, these parameters do not need to be provided.
 */
typedef struct {
    const uint32_t version;
    int pclk;
    int hts;               /*!< HTS = H_Size + H_Blank, also known as hmax */
    int vts;               /*!< VTS = V_Size + V_Blank, also known as vmax */
    uint32_t exp_def;      /*!< Exposure default */
    uint32_t gain_def;     /*!< Gain default */
    esp_cam_sensor_bayer_pattern_t bayer_type;
} esp_cam_sensor_isp_info_v1_t;

/**
 * @brief Description of ISP related parameters corresponding to the specified format.
 */
typedef union _cam_sensor_isp_info {
    esp_cam_sensor_isp_info_v1_t isp_v1_info;
} esp_cam_sensor_isp_info_t;

/**
 * @brief Description of camera sensor output format
 */
typedef struct _cam_sensor_format_struct {
    const char *name;                           /*!< String description for output format */
    esp_cam_sensor_output_format_t format;      /*!< Sensor output format */
    esp_cam_sensor_port_t port;                 /*!< Sensor output port type */
    int xclk;                                   /*!< Sensor input clock frequency */
    uint16_t width;                             /*!< Output windows width */
    uint16_t height;                            /*!< Output windows height */

    const void *regs;                           /*!< Regs to enable this format */
    int regs_size;
    uint8_t fps;                                /*!< frames per second */
    const esp_cam_sensor_isp_info_t *isp_info;  /*!< For sensor without internal ISP, set NULL if the sensor‘s internal ISP used. */
    esp_cam_sensor_mipi_info_t mipi_info;       /*!< MIPI RX init cfg */
    void *reserved;                             /*!< can be used to provide AE\AF\AWB info or Parameters of some related accessories（VCM、LED、IR）*/
} esp_cam_sensor_format_t;

/**
 * @brief Description of camera sensor supported capabilities
 */
typedef struct _cam_sensor_capability {
    /* Data format field */
    uint32_t fmt_raw : 1;
    uint32_t fmt_rgb565 : 1;
    uint32_t fmt_yuv : 1;
    uint32_t fmt_jpeg : 1;
} esp_cam_sensor_capability_t;

/**
 * @brief Description of output formats supported by the camera sensor driver
 */
typedef struct _cam_sensor_format_array {
    uint32_t count;
    const esp_cam_sensor_format_t *format_array;
} esp_cam_sensor_format_array_t;

typedef struct _esp_cam_sensor_ops esp_cam_sensor_ops_t;

/**
 * @brief Type of camera sensor device
 */
typedef struct {
    char *name;                                  /*!< String name of the sensor */
    esp_sccb_io_handle_t sccb_handle;            /*!< SCCB io handle that created by `sccb_new_i2c_io` */
    int8_t  xclk_pin;                            /*!< Sensor clock input pin, set to -1 not used */
    int8_t  reset_pin;                           /*!< Hardware reset pin, set to -1 if not used */
    int8_t  pwdn_pin;                            /*!< Power down pin, set to -1 if not used */
    esp_cam_sensor_port_t sensor_port;           /*!< Camera interface currently in use */
    const esp_cam_sensor_format_t *cur_format;   /*!< Current format */
    esp_cam_sensor_id_t id;                      /*!< Sensor ID. */
    uint8_t stream_status;                       /*!< Status of the sensor output stream. */
    const esp_cam_sensor_ops_t *ops;             /*!< Pointer to the camera sensor driver operation array. */
    void *priv;                                  /*!< Private data */
} esp_cam_sensor_device_t;

/**
 * @brief camera sensor driver operation array
 */
typedef struct _esp_cam_sensor_ops {
    /*!< Mainly used by ISP， and can be used to control other accessories on the camera module */
    int (*query_para_desc)(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc);
    int (*get_para_value)(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size);
    int (*set_para_value)(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size);

    /*!< Common */
    int (*query_support_formats)(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *parry);
    int (*query_support_capability)(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *arg);
    int (*set_format)(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format);
    int (*get_format)(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format);
    int (*priv_ioctl)(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg);
    int (*del)(esp_cam_sensor_device_t *dev);
} esp_cam_sensor_ops_t;

/**
 * @brief Configuration for camera sensor power on and sccb detect
 */
typedef struct {
    esp_sccb_io_handle_t sccb_handle;            /*!< the handle of the sccb bus bound to the sensor, returned by sccb_new_i2c_io */
    int8_t  reset_pin;                           /*!< reset pin, set to -1 if not used */
    int8_t  pwdn_pin;                            /*!< power down pin, set to -1 if not used */
    int8_t  xclk_pin;                            /*!< xclk pin, set to -1 if not used*/
    int32_t xclk_freq_hz;                        /*!< xclk freq， invalid when xclk = -1 */
    esp_cam_sensor_port_t sensor_port;           /*!< camera interface currently in use， DVP or MIPI */
} esp_cam_sensor_config_t;

/**
 * @brief Description of automatically detecting camera devices
 */
typedef struct {
    union {
        esp_cam_sensor_device_t *(*detect)(void *);   /*!< Pointer to the detect function */
        esp_cam_sensor_device_t *(*fn)(void *) __attribute__((deprecated("please use detect instead")));           /*!< Pointer to the detect function */
    };
    esp_cam_sensor_port_t port;
    uint16_t sccb_addr;
} esp_cam_sensor_detect_fn_t;

/**
 * @brief Description of cam sensor statistical data
 */
typedef struct {
    uint32_t flags;
    uint32_t seq;
    uint16_t agc_gain; /*!< AGC gain output to sensor */
    union {
        struct {
            uint8_t red_avg;
            uint8_t blue_avg;
            uint8_t green_avg;
        } wb_avg;
    };
} esp_cam_sensor_stats_t;

/**
 * @brief Description of cam sensor expousre val and total gain index when group hold is used.
 * Group hold refers to the packing of a group of registers to be effective at a specific time within a frame.
 * When the exposure time and gain need to be updated at the same time,
 * the group hold can be used to ensure that all of them take effect at the same time.
 */
typedef struct {
    uint32_t exposure_us; /*!< Exposure time in us */
    uint32_t gain_index;  /*!< the index of gain map table */
} esp_cam_sensor_gh_exp_gain_t;

#ifdef __cplusplus
}
#endif
