/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "sc035hgs_settings.h"
#include "sc035hgs.h"

/*
 * SC035HGS camera sensor gain control.
 */
typedef struct {
    uint8_t again_fine; // analog gain fine
    uint8_t again_coarse; // analog gain coarse
    uint8_t dgain_fine; // digital gain fine
    uint8_t dgain_coarse; // digital gain coarse
} sc035hgs_gain_t;

typedef struct {
    uint32_t exposure_val;
    uint32_t gain_index; // current gain index

    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} sc035hgs_para_t;

struct sc035hgs_cam {
    sc035hgs_para_t sc035hgs_para;
};

#define SC035HGS_IO_MUX_LOCK(mux)
#define SC035HGS_IO_MUX_UNLOCK(mux)
#define SC035HGS_ENABLE_OUT_XCLK(pin,clk)
#define SC035HGS_DISABLE_OUT_XCLK(pin)

#define SC035HGS_FETCH_EXP_H(val)     (((val) >> 4) & 0xFF)
#define SC035HGS_FETCH_EXP_L(val)     (((val) & 0xF) << 4)
#define SC035HGS_GROUP_HOLD_START   0X00
#define SC035HGS_GROUP_HOLD_LUNCH   0x30

#define SC035HGS_PID         0x0031
#define SC035HGS_SENSOR_NAME "SC035HGS"
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define SC035HGS_SUPPORT_NUM CONFIG_CAMERA_SC035HGS_MAX_SUPPORT

static const char *TAG = "sc035hgs";

// total gain = analog_gain x digital_gain x 1000(To avoid decimal points, the final abs_gain is multiplied by 1000.)
static const uint32_t sc035hgs_total_gain_val_map[] = {
    //1x
    1000,
    1062,
    1125,
    1187,
    1250,
    1312,
    1375,
    1437,
    1500,
    1562,
    1625,
    1687,
    1750,
    1812,
    1875,
    1937,
    //2x
    2000,
    2125,
    2250,
    2375,
    2500,
    2625,
    2750,
    2875,
    3000,
    3125,
    3250,
    3375,
    3500,
    3625,
    3750,
    3875,
    //4x
    4000,
    4250,
    4500,
    4750,
    5000,
    5250,
    5500,
    5750,
    6000,
    6250,
    6500,
    6750,
    7000,
    7250,
    7500,
    7750,
    //8x
    8000,
    8500,
    9000,
    9500,
    10000,
    10500,
    11000,
    11500,
    12000,
    12500,
    13000,
    13500,
    14000,
    14500,
    15000,
    15500,
    //16x
    16468,
    17437,
    18406,
    19375,
    20343,
    21312,
    22281,
    23250,
    24218,
    25187,
    26156,
    27125,
    28093,
    29062,
    30031,
    31000,
    32937,
};

// SC035HGS Gain map format: [ANG_FINE(0x3e09), ANG_COARSE(0x3e08), DIG_FINE(0x3e07), DIG_COARSE(0x3e06)]
static const sc035hgs_gain_t sc035hgs_gain_map[] = {
    // 1x
    {0x10, 0x00, 0x80, 0x00},
    {0x11, 0x00, 0x80, 0x00},
    {0x12, 0x00, 0x80, 0x00},
    {0x13, 0x00, 0x80, 0x00},
    {0x14, 0x00, 0x80, 0x00},
    {0x15, 0x00, 0x80, 0x00},
    {0x16, 0x00, 0x80, 0x00},
    {0x17, 0x00, 0x80, 0x00},
    {0x18, 0x00, 0x80, 0x00},
    {0x19, 0x00, 0x80, 0x00},
    {0x1a, 0x00, 0x80, 0x00},
    {0x1b, 0x00, 0x80, 0x00},
    {0x1c, 0x00, 0x80, 0x00},
    {0x1d, 0x00, 0x80, 0x00},
    {0x1e, 0x00, 0x80, 0x00},
    {0x1f, 0x00, 0x80, 0x00},
    // 2x
    {0x10, 0x01, 0x80, 0x00},
    {0x11, 0x01, 0x80, 0x00},
    {0x12, 0x01, 0x80, 0x00},
    {0x13, 0x01, 0x80, 0x00},
    {0x14, 0x01, 0x80, 0x00},
    {0x15, 0x01, 0x80, 0x00},
    {0x16, 0x01, 0x80, 0x00},
    {0x17, 0x01, 0x80, 0x00},
    {0x18, 0x01, 0x80, 0x00},
    {0x19, 0x01, 0x80, 0x00},
    {0x1a, 0x01, 0x80, 0x00},
    {0x1b, 0x01, 0x80, 0x00},
    {0x1c, 0x01, 0x80, 0x00},
    {0x1d, 0x01, 0x80, 0x00},
    {0x1e, 0x01, 0x80, 0x00},
    {0x1f, 0x01, 0x80, 0x00},
    // 4x
    {0x10, 0x03, 0x80, 0x00},
    {0x11, 0x03, 0x80, 0x00},
    {0x12, 0x03, 0x80, 0x00},
    {0x13, 0x03, 0x80, 0x00},
    {0x14, 0x03, 0x80, 0x00},
    {0x15, 0x03, 0x80, 0x00},
    {0x16, 0x03, 0x80, 0x00},
    {0x17, 0x03, 0x80, 0x00},
    {0x18, 0x03, 0x80, 0x00},
    {0x19, 0x03, 0x80, 0x00},
    {0x1a, 0x03, 0x80, 0x00},
    {0x1b, 0x03, 0x80, 0x00},
    {0x1c, 0x03, 0x80, 0x00},
    {0x1d, 0x03, 0x80, 0x00},
    {0x1e, 0x03, 0x80, 0x00},
    {0x1f, 0x03, 0x80, 0x00},
    // 8x
    {0x10, 0x07, 0x80, 0x00},
    {0x11, 0x07, 0x80, 0x00},
    {0x12, 0x07, 0x80, 0x00},
    {0x13, 0x07, 0x80, 0x00},
    {0x14, 0x07, 0x80, 0x00},
    {0x15, 0x07, 0x80, 0x00},
    {0x16, 0x07, 0x80, 0x00},
    {0x17, 0x07, 0x80, 0x00},
    {0x18, 0x07, 0x80, 0x00},
    {0x19, 0x07, 0x80, 0x00},
    {0x1a, 0x07, 0x80, 0x00},
    {0x1b, 0x07, 0x80, 0x00},
    {0x1c, 0x07, 0x80, 0x00},
    {0x1d, 0x07, 0x80, 0x00},
    {0x1e, 0x07, 0x80, 0x00},
    {0x1f, 0x07, 0x80, 0x00},
    // 16x
    {0x1f, 0x07, 0x88, 0x00}, // 16.46875
    {0x1f, 0x07, 0x90, 0x00}, // 17.4375
    {0x1f, 0x07, 0x98, 0x00}, // 18.40625
    {0x1f, 0x07, 0xa0, 0x00}, // 19.375
    {0x1f, 0x07, 0xa8, 0x00}, // 20.34375
    {0x1f, 0x07, 0xb0, 0x00}, // 21.3125
    {0x1f, 0x07, 0xb8, 0x00}, // 22.28125
    {0x1f, 0x07, 0xc0, 0x00}, // 23.25
    {0x1f, 0x07, 0xc8, 0x00}, // 24.21875
    {0x1f, 0x07, 0xd0, 0x00}, // 25.1875
    {0x1f, 0x07, 0xd8, 0x00}, // 26.15625
    {0x1f, 0x07, 0xe0, 0x00}, // 27.125
    {0x1f, 0x07, 0xe8, 0x00}, // 28.09375
    {0x1f, 0x07, 0xf0, 0x00}, // 29.0625
    {0x1f, 0x07, 0xf8, 0x00}, // 30.03125
    {0x1f, 0x07, 0x80, 0x01}, // 31.0000
    {0x1f, 0x07, 0x88, 0x01}, // 32.9375
};

static const esp_cam_sensor_isp_info_t sc035hgs_isp_info[] = {
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 50056704,
            .vts = 0x394,
            .hts = 0x470,
            .gain_def = 0,
            .exp_def = 0x18f,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 45000000,
            .vts = 0x2ab,
            .hts = 0x36e,
            .gain_def = 0,
            .exp_def = 0x18f,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    }
};

static const esp_cam_sensor_format_t sc035hgs_format_info[] = {
    {
        .name = "MIPI_1lane_20Minput_raw10_640x480_48fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 20000000,
        .width = 640,
        .height = 480,
        .regs = mipi_20Minput_1lane_640x480_raw10_48fps,
        .regs_size = ARRAY_SIZE(mipi_20Minput_1lane_640x480_raw10_48fps),
        .fps = 48,
        .isp_info = &sc035hgs_isp_info[0],
        .mipi_info = {
            .mipi_clk = 500000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_1lane_20Minput_raw10_640x480_120fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 640,
        .height = 480,
        .regs = mipi_24Minput_1lane_640x480_raw10_linear_120fps,
        .regs_size = ARRAY_SIZE(mipi_24Minput_1lane_640x480_raw10_linear_120fps),
        .fps = 120,
        .isp_info = &sc035hgs_isp_info[1],
        .mipi_info = {
            .mipi_clk = 425000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
};

static esp_err_t sc035hgs_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t sc035hgs_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

static esp_err_t sc035hgs_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = sc035hgs_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = sc035hgs_write(sccb_handle, reg, value);
    return ret;
}

/* write a array of registers  */
static esp_err_t sc035hgs_write_array(esp_sccb_io_handle_t sccb_handle, sc035hgs_reginfo_t *regarray, size_t regs_size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && (i < regs_size)) {
        if (regarray[i].reg != SC035HGS_REG_DELAY) {
            ret = sc035hgs_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "Set array done[i=%d]", i);
    return ret;
}

static esp_err_t sc035hgs_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return sc035hgs_set_reg_bits(dev->sccb_handle, 0X4501, 3, 1, enable ? 0x01 : 0x00);
}

static esp_err_t sc035hgs_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t sc035hgs_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = sc035hgs_set_reg_bits(dev->sccb_handle, 0x0103, 0, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t sc035hgs_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = sc035hgs_read(dev->sccb_handle, SC035HGS_REG_ID_HIGH, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = sc035hgs_read(dev->sccb_handle, SC035HGS_REG_ID_LOW, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t sc035hgs_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;

    ret = sc035hgs_write(dev->sccb_handle, SC035HGS_REG_SLEEP_MODE, enable ? 0x01 : 0x00);
    if (enable) {
        ret |= sc035hgs_write(dev->sccb_handle, 0x4418, 0x0a);
        ret |= sc035hgs_write(dev->sccb_handle, 0x363d, 0x10);
        ret |= sc035hgs_write(dev->sccb_handle, 0x4419, 0x80);
    }

    dev->stream_status = enable;
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t sc035hgs_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return sc035hgs_set_reg_bits(dev->sccb_handle, 0x3221, 1, 2, enable ? 0x03 : 0x00);
}

static esp_err_t sc035hgs_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return sc035hgs_set_reg_bits(dev->sccb_handle, 0x3221, 5, 2, enable ? 0x03 : 0x00);
}

static esp_err_t sc035hgs_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0xf;
        qdesc->number.maximum = dev->cur_format->isp_info->isp_v1_info.vts - 6; // max = VTS-6 = height+vblank-6, so when update vblank, exposure_max must be updated
        qdesc->number.step = 1;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.exp_def;
        break;
    case ESP_CAM_SENSOR_GAIN:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_ENUMERATION;
        qdesc->enumeration.count = ARRAY_SIZE(sc035hgs_total_gain_val_map);
        qdesc->enumeration.elements = sc035hgs_total_gain_val_map;
        qdesc->default_value = dev->cur_format->isp_info->isp_v1_info.gain_def; // gain index
        break;
    case ESP_CAM_SENSOR_VFLIP:
    case ESP_CAM_SENSOR_HMIRROR:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    default: {
        ESP_LOGD(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t sc035hgs_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct sc035hgs_cam *cam_sc035hgs = (struct sc035hgs_cam *)dev->priv;
    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        *(uint32_t *)arg = cam_sc035hgs->sc035hgs_para.exposure_val;
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        *(uint32_t *)arg = cam_sc035hgs->sc035hgs_para.gain_index;
        break;
    }
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t sc035hgs_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct sc035hgs_cam *cam_sc035hgs = (struct sc035hgs_cam *)dev->priv;

    switch (id) {
    case ESP_CAM_SENSOR_EXPOSURE_VAL: {
        uint32_t u32_val = *(uint32_t *)arg;
        ESP_LOGD(TAG, "set exposure 0x%" PRIx32, u32_val);
        // Todo, convert to uint seconds, sc035hgs exposure time use 1/16 * LTime as step, and EXP Rows = {0x3e01, 0x3e02} + 0x3226
        ret = sc035hgs_write(dev->sccb_handle, SC035HGS_REG_GROUP_HOLD, SC035HGS_GROUP_HOLD_START);
        ret |= sc035hgs_write(dev->sccb_handle, SC035HGS_REG_SHUTTER_TIME_H, SC035HGS_FETCH_EXP_H(u32_val));
        ret |= sc035hgs_write(dev->sccb_handle, SC035HGS_REG_SHUTTER_TIME_L, SC035HGS_FETCH_EXP_L(u32_val));
        ret |= sc035hgs_write(dev->sccb_handle, SC035HGS_REG_GROUP_HOLD, SC035HGS_GROUP_HOLD_LUNCH);
        if (ret == ESP_OK) {
            cam_sc035hgs->sc035hgs_para.exposure_val = u32_val;
        }
        break;
    }
    case ESP_CAM_SENSOR_GAIN: {
        uint32_t u32_val = *(uint32_t *)arg;
        ESP_LOGD(TAG, "again_fine %" PRIx8 ", again_coarse %" PRIx8 ", dgain_fine %" PRIx8 ", dgain_coarse %" PRIx8, sc035hgs_gain_map[u32_val].again_fine,
                 sc035hgs_gain_map[u32_val].again_coarse,
                 sc035hgs_gain_map[u32_val].dgain_fine,
                 sc035hgs_gain_map[u32_val].dgain_coarse);

        ret = sc035hgs_set_reg_bits(dev->sccb_handle,
                                    SC035HGS_REG_FINE_AGAIN, 2, 3,
                                    sc035hgs_gain_map[u32_val].again_fine);
        ret |= sc035hgs_write(dev->sccb_handle,
                              SC035HGS_REG_COARSE_AGAIN,
                              sc035hgs_gain_map[u32_val].again_coarse);
        ret |= sc035hgs_set_reg_bits(dev->sccb_handle,
                                     SC035HGS_REG_FINE_DGAIN, 0, 2,
                                     sc035hgs_gain_map[u32_val].dgain_fine);
        ret |= sc035hgs_write(dev->sccb_handle,
                              SC035HGS_REG_COARSE_DGAIN,
                              sc035hgs_gain_map[u32_val].dgain_coarse);
        if (ret == ESP_OK) {
            cam_sc035hgs->sc035hgs_para.gain_index = u32_val;
        }
        break;
    }
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;

        ret = sc035hgs_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;

        ret = sc035hgs_set_mirror(dev, *value);
        break;
    }
    default: {
        ESP_LOGE(TAG, "set id=%" PRIx32 " is not supported", id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static esp_err_t sc035hgs_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(sc035hgs_format_info);
    formats->format_array = &sc035hgs_format_info[0];
    return ESP_OK;
}

static esp_err_t sc035hgs_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_yuv = 1;
    return 0;
}

static esp_err_t sc035hgs_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_OK;
    struct sc035hgs_cam *cam_sc035hgs = (struct sc035hgs_cam *)dev->priv;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &sc035hgs_format_info[CONFIG_CAMERA_SC035HGS_MIPI_IF_FORMAT_INDEX_DAFAULT];
    }

    ret = sc035hgs_write_array(dev->sccb_handle, (sc035hgs_reginfo_t *)format->regs, format->regs_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;
    // init para
    cam_sc035hgs->sc035hgs_para.exposure_val = dev->cur_format->isp_info->isp_v1_info.exp_def;
    cam_sc035hgs->sc035hgs_para.gain_index = dev->cur_format->isp_info->isp_v1_info.gain_def;

    return ret;
}

static esp_err_t sc035hgs_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, format);

    esp_err_t ret = ESP_FAIL;

    if (dev->cur_format != NULL) {
        memcpy(format, dev->cur_format, sizeof(esp_cam_sensor_format_t));
        ret = ESP_OK;
    }
    return ret;
}

static esp_err_t sc035hgs_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    SC035HGS_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = sc035hgs_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = sc035hgs_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc035hgs_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = sc035hgs_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = sc035hgs_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = sc035hgs_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = sc035hgs_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    SC035HGS_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t sc035hgs_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC035HGS_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        // carefully, logic is inverted compared to reset pin
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->reset_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t sc035hgs_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        SC035HGS_DISABLE_OUT_XCLK(dev->xclk_pin);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
    }

    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t sc035hgs_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del sc035hgs (%p)", dev);
    if (dev) {
        if (dev->priv) {
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t sc035hgs_ops = {
    .query_para_desc = sc035hgs_query_para_desc,
    .get_para_value = sc035hgs_get_para_value,
    .set_para_value = sc035hgs_set_para_value,
    .query_support_formats = sc035hgs_query_support_formats,
    .query_support_capability = sc035hgs_query_support_capability,
    .set_format = sc035hgs_set_format,
    .get_format = sc035hgs_get_format,
    .priv_ioctl = sc035hgs_priv_ioctl,
    .del = sc035hgs_delete
};

esp_cam_sensor_device_t *sc035hgs_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct sc035hgs_cam *cam_sc035hgs;

    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_sc035hgs = heap_caps_calloc(1, sizeof(struct sc035hgs_cam), MALLOC_CAP_DEFAULT);
    if (!cam_sc035hgs) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    dev->name = (char *)SC035HGS_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &sc035hgs_ops;
    dev->priv = cam_sc035hgs;
    dev->cur_format = &sc035hgs_format_info[CONFIG_CAMERA_SC035HGS_MIPI_IF_FORMAT_INDEX_DAFAULT];

    // Configure sensor power, clock, and SCCB port
    if (sc035hgs_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (sc035hgs_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != SC035HGS_PID) {
        ESP_LOGE(TAG, "Camera sensor is not SC035HGS, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    sc035hgs_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_SC035HGS_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(sc035hgs_detect, ESP_CAM_SENSOR_MIPI_CSI, SC035HGS_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return sc035hgs_detect(config);
}
#endif
