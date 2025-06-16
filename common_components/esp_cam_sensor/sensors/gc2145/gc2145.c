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
#include "gc2145_settings.h"

#include "gc2145.h"

/*
 * GC2145 camera sensor register pages definition.
 */
typedef enum {
    GC2145_PAGE0,
    GC2145_PAGE1,
    GC2145_PAGE2,
    GC2145_PAGE3,
} gc2145_page_t;

/*
 * GC2145 camera sensor wb mode.
 */
typedef enum {
    GC2145_WB_AUTO,
    GC2145_WB_CLOUD,
    GC2145_WB_DAYLIGHT,
    GC2145_WB_INCANDESCENCE,
    GC2145_WB_TUNGSTEN,
    GC2145_WB_FLUORESCENT,
    GC2145_WB_MANUAL,
} gc2145_wb_mode_t;

/*
 * GC2145 camera sensor special effect.
 */
typedef enum {
    GC2145_EFFECT_ENC_NORMAL,
    GC2145_EFFECT_ENC_GRAYSCALE,
    GC2145_EFFECT_ENC_SEPIA,
    GC2145_EFFECT_ENC_SEPIAGREEN,
    GC2145_EFFECT_ENC_SEPIABLUE,
    GC2145_EFFECT_ENC_COLORINV,
} gc2145_spec_effect_mode_t;

/*
 * GC2145 camera sensor banding mode.
 */
typedef enum {
    GC2145_BANDING_OFF,
    GC2145_BANDING_50HZ,
    GC2145_BANDING_60HZ,
    GC2145_BANDING_AUTO
} gc2145_banding_mode_t;

/*
 * GC2145 camera sensor scene mode.
 */
typedef enum {
    GC2145_SCENE_MODE_NORMAL,
    GC2145_SCENE_MODE_NIGHT,
    GC2145_SCENE_MODE_LANDSCAPE,
    GC2145_SCENE_MODE_PORTRAIT,
} gc2145_scene_mode_t;

#define GC2145_IO_MUX_LOCK(mux)
#define GC2145_IO_MUX_UNLOCK(mux)
#define GC2145_ENABLE_OUT_XCLK(pin,clk)
#define GC2145_DISABLE_OUT_XCLK(pin)
#define GC2145_AEC_TARGET_DEFAULT (0x7b)

#define GC2145_PID         0x2145
#define GC2145_SENSOR_NAME "GC2145"

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define GC2145_SUPPORT_NUM CONFIG_CAMERA_GC2145_MAX_SUPPORT

static const char *TAG = "gc2145";

static const esp_cam_sensor_format_t gc2145_format_info[] = {
    {
        .name = "MIPI_1lane_24Minput_RGB565_1600x1200_7fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RGB565,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1600,
        .height = 1200,
        .regs = gc2145_mipi_1lane_24Minput_1600x1200_rgb565_7fps,
        .regs_size = ARRAY_SIZE(gc2145_mipi_1lane_24Minput_1600x1200_rgb565_7fps),
        .fps = 7,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = 336000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_1lane_24Minput_RGB565_800x600_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RGB565,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 800,
        .height = 600,
        .regs = gc2145_mipi_1lane_24Minput_800x600_rgb565_30fps,
        .regs_size = ARRAY_SIZE(gc2145_mipi_1lane_24Minput_800x600_rgb565_30fps),
        .fps = 30,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = 336000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_YUV422_640x480_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 640,
        .height = 480,
        .regs = gc2145_DVP_8bit_20Minput_640x480_yuv422_15fps_windowing,
        .regs_size = ARRAY_SIZE(gc2145_DVP_8bit_20Minput_640x480_yuv422_15fps_windowing),
        .fps = 15,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_YUV422_1600x1200_13fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 1600,
        .height = 1200,
        .regs = gc2145_DVP_8bit_20Minput_1600x1200_yuv422_13fps,
        .regs_size = ARRAY_SIZE(gc2145_DVP_8bit_20Minput_1600x1200_yuv422_13fps),
        .fps = 13,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_YUV422_800x600_20fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 800,
        .height = 600,
        .regs = gc2145_DVP_8bit_20Minput_800x600_yuv422_20fps,
        .regs_size = ARRAY_SIZE(gc2145_DVP_8bit_20Minput_800x600_yuv422_20fps),
        .fps = 20,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
};

static esp_err_t gc2145_read(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a8v8(sccb_handle, reg, read_buf);
}

static esp_err_t gc2145_write(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a8v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t gc2145_write_array(esp_sccb_io_handle_t sccb_handle, gc2145_reginfo_t *regarray, size_t regs_size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && (i < regs_size)) {
        if (regarray[i].reg != GC2145_REG_DELAY) {
            ret = gc2145_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "Set array done[i=%d]", i);
    return ret;
}

static esp_err_t gc2145_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t offset, uint8_t mask, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t c_value, new_value;

    ret = gc2145_read(sccb_handle, reg, &c_value);
    if (ret != ESP_OK) {
        return ret;
    }
    new_value = (c_value & ~(mask << offset)) | ((value & mask) << offset);

    ret = gc2145_write(sccb_handle, reg, new_value);
    return ret;
}

static esp_err_t gc2145_select_page(esp_cam_sensor_device_t *dev, uint8_t page)
{
    return gc2145_write(dev->sccb_handle, GC2145_REG_RESET_RELATED, page);
}

static esp_err_t gc2145_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    ESP_LOGW(TAG, "Test image support in UXGA");
    esp_err_t ret = gc2145_select_page(dev, 0x00);
    ret |=  gc2145_write(dev->sccb_handle, GC2145_REG_P0_DEBUG_MODE2, enable ? 0x08 : 0x00);
    return ret;
}

static esp_err_t gc2145_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t gc2145_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = gc2145_select_page(dev, 0x00);
    ret |= gc2145_set_reg_bits(dev->sccb_handle, GC2145_REG_RESET_RELATED, 7, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t gc2145_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;
    ret = gc2145_select_page(dev, 0x00);
    ret = gc2145_read(dev->sccb_handle, GC2145_REG_CHIP_ID_HIGH, &pid_h);
    ret |= gc2145_read(dev->sccb_handle, GC2145_REG_CHIP_ID_LOW, &pid_l);

    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = pid_h << 8 | pid_l;

    return ret;
}

static esp_err_t gc2145_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t val = 0;
    ret = gc2145_select_page(dev, 0x00);

    if (dev->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        if (dev->cur_format->mipi_info.lane_num == 1) {
            val = enable ? 0x94 : 0x84; // Notes that this pata used in 1 lane mode.
            ret |= gc2145_write(dev->sccb_handle, 0xfe, 0x03);
            ret |= gc2145_write(dev->sccb_handle, 0x10, val);
            ESP_LOGD(TAG, "1lane stream=%d", enable);
        }
    } else {
        val = enable ? 0x0f : 0;
        ret |= gc2145_write(dev->sccb_handle, 0xf2, val);
    }

    if (ret == ESP_OK) {
        dev->stream_status = enable;
    }
    ESP_LOGD(TAG, "Stream=%d", enable);

    return ret;
}

static esp_err_t gc2145_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = gc2145_select_page(dev, 0x00);
    /*
    GC2145_REG_P0_ANALOG_MODE1： 0x14:normal、0x15:H_MIRROR、0x16:V_MIRROR、0x17:HV_Mirror
    */
    ret |= gc2145_set_reg_bits(dev->sccb_handle, GC2145_REG_P0_ANALOG_MODE1, 0, 0x01, enable != 0);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Set h-mirror to: %d", enable);
    }
    return ret;
}

static esp_err_t gc2145_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = gc2145_select_page(dev, 0x00);
    ret |= gc2145_set_reg_bits(dev->sccb_handle, GC2145_REG_P0_ANALOG_MODE1, 1, 0x01, enable != 0);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Set vflip to: %d", enable);
    }

    return ret;
}

/*V4L2_CID_BRIGHTNESS*/
static esp_err_t gc2145_set_brightness(esp_cam_sensor_device_t *dev, int target)
{
    esp_err_t ret = gc2145_select_page(dev, 0x02);
    ret |=  gc2145_write(dev->sccb_handle, 0xd5, target & 0xff);
    return ret;
}

/*V4L2_CID_CONTRAST*/
static esp_err_t gc2145_set_contrast(esp_cam_sensor_device_t *dev, int target)
{
    esp_err_t ret = gc2145_select_page(dev, 0x02);
    ret |=  gc2145_write(dev->sccb_handle, 0xd3, target & 0xff);
    return ret;
}

/*V4L2_CID_SATURATION*/
static esp_err_t gc2145_set_saturation(esp_cam_sensor_device_t *dev, int target)
{
    esp_err_t ret = gc2145_select_page(dev, 0x02);
    ret |=  gc2145_write(dev->sccb_handle, 0xd1, target & 0xff);
    ret |=  gc2145_write(dev->sccb_handle, 0xd2, target & 0xff);
    return ret;
}

/*V4L2_CID_DO_WHITE_BALANCE or V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE*/
static esp_err_t gc2145_set_wb_mode(esp_cam_sensor_device_t *dev, int val)
{
    uint8_t temp;
    esp_err_t ret = gc2145_select_page(dev, 0x00);
    ret = gc2145_read(dev->sccb_handle, 0x82, &temp);

    switch (val) {
    case GC2145_WB_AUTO:/* auto */
        ret |=  gc2145_write(dev->sccb_handle, 0xb3, 0x58);
        ret |=  gc2145_write(dev->sccb_handle, 0xb4, 0x40);
        ret |=  gc2145_write(dev->sccb_handle, 0xb5, 0x50);
        ret |=  gc2145_write(dev->sccb_handle, 0x82, temp | 0x2);
        break;
    case GC2145_WB_CLOUD: /* cloud */
        ret |=  gc2145_write(dev->sccb_handle, 0x82, temp & (~0x02));
        ret |=  gc2145_write(dev->sccb_handle, 0xb3, 0x58);
        ret |=  gc2145_write(dev->sccb_handle, 0xb4, 0x40);
        ret |=  gc2145_write(dev->sccb_handle, 0xb5, 0x50);
        break;
    case GC2145_WB_DAYLIGHT:
        ret |=  gc2145_write(dev->sccb_handle, 0x82, temp & (~0x02));
        ret |=  gc2145_write(dev->sccb_handle, 0xb3, 0x70);
        ret |=  gc2145_write(dev->sccb_handle, 0xb4, 0x40);
        ret |=  gc2145_write(dev->sccb_handle, 0xb5, 0x50);
        break;
    case GC2145_WB_INCANDESCENCE:
        ret |=  gc2145_write(dev->sccb_handle, 0x82, temp & (~0x02));
        ret |=  gc2145_write(dev->sccb_handle, 0xb3, 0x50);
        ret |=  gc2145_write(dev->sccb_handle, 0xb4, 0x40);
        ret |=  gc2145_write(dev->sccb_handle, 0xb5, 0xa8);
        break;
    case GC2145_WB_TUNGSTEN:
        ret |=  gc2145_write(dev->sccb_handle, 0x82, temp & (~0x02));
        ret |=  gc2145_write(dev->sccb_handle, 0xb3, 0xa0);
        ret |=  gc2145_write(dev->sccb_handle, 0xb4, 0x45);
        ret |=  gc2145_write(dev->sccb_handle, 0xb5, 0x40);
        break;
    case GC2145_WB_FLUORESCENT:
        ret |=  gc2145_write(dev->sccb_handle, 0x82, temp & (~0x02));
        ret |=  gc2145_write(dev->sccb_handle, 0xb3, 0x72);
        ret |=  gc2145_write(dev->sccb_handle, 0xb4, 0x40);
        ret |=  gc2145_write(dev->sccb_handle, 0xb5, 0x5b);
        break;
    case GC2145_WB_MANUAL:
        /* TODO */
        break;
    default:
        break;
    }
    return ret;
}

/*V4L2_CID_SHARPNESS*/
static esp_err_t gc2145_set_sharpness(esp_cam_sensor_device_t *dev, int target)
{
    esp_err_t ret = gc2145_select_page(dev, 0x02);
    ret |=  gc2145_write(dev->sccb_handle, 0x97, target & 0xff);
    return ret;
}

/*
V4L2_CID_EXPOSURE
*/
static esp_err_t gc2145_set_exposure(esp_cam_sensor_device_t *dev, int target)
{
    esp_err_t ret = gc2145_select_page(dev, 0x01);
    ret |=  gc2145_write(dev->sccb_handle, 0x13, target & 0xff);
    return ret;
}

/*V4L2_CID_COLORFX*/
static esp_err_t gc2145_set_effect(esp_cam_sensor_device_t *dev, int val)
{
    esp_err_t ret = gc2145_select_page(dev, 0x00);

    switch (val) {
    case GC2145_EFFECT_ENC_NORMAL:
        ret |=  gc2145_write(dev->sccb_handle, 0x83, 0xe0);
        break;
    case GC2145_EFFECT_ENC_GRAYSCALE:
        ret |=  gc2145_write(dev->sccb_handle, 0x83, 0x12);
        break;
    case GC2145_EFFECT_ENC_SEPIA:
        ret |=  gc2145_write(dev->sccb_handle, 0x83, 0x82);
        break;
    case GC2145_EFFECT_ENC_SEPIAGREEN:
        ret |=  gc2145_write(dev->sccb_handle, 0x43, 0x52);
        break;
    case GC2145_EFFECT_ENC_SEPIABLUE:
        ret |=  gc2145_write(dev->sccb_handle, 0x43, 0x62);
        break;
    case GC2145_EFFECT_ENC_COLORINV:
        ret |=  gc2145_write(dev->sccb_handle, 0x83, 0x01);
        break;
    default:
        ret |=  gc2145_write(dev->sccb_handle, 0x83, 0xe0);
        break;
    }
    return ret;
}

/*V4L2_CID_SCENE*/
static esp_err_t gc2145_set_scene_mode(esp_cam_sensor_device_t *dev, int val)
{
    esp_err_t ret = gc2145_select_page(dev, 0x01);

    switch (val) {
    case GC2145_SCENE_MODE_NORMAL:
        ret |=  gc2145_write(dev->sccb_handle, 0x2f, 0x20);
        ret |=  gc2145_write(dev->sccb_handle, 0x3c, 0x40);
        break;
    case GC2145_SCENE_MODE_NIGHT:
        ret |=  gc2145_write(dev->sccb_handle, 0x2f, 0x30);
        ret |=  gc2145_write(dev->sccb_handle, 0x3c, 0x60);
        break;
    case GC2145_SCENE_MODE_LANDSCAPE:
        ret |=  gc2145_write(dev->sccb_handle, 0x2f, 0x10);
        break;
    case GC2145_SCENE_MODE_PORTRAIT:
        ret |=  gc2145_write(dev->sccb_handle, 0x2f, 0x00);
        break;
    default:
        break;
    }
    return ret;
}

/*V4L2_CID_POWER_LINE_FREQUENCY*/
static esp_err_t gc2145_set_antibanding(esp_cam_sensor_device_t *dev, int val)
{
    esp_err_t ret = gc2145_select_page(dev, 0x00);
    uint8_t banding_array_index = 0;
    switch (val) {
    case GC2145_BANDING_OFF:
        banding_array_index = 0;
        break;
    case GC2145_BANDING_50HZ:
        banding_array_index = 1;
        break;
    case GC2145_BANDING_60HZ:
        banding_array_index = 2;
        break;
    case GC2145_BANDING_AUTO:
        banding_array_index = 3;
        break;
    default:
        break;
    }

    ret = gc2145_write_array(dev->sccb_handle, (gc2145_reginfo_t *)gc2145_antibanding[banding_array_index], GC2145_ANTI_BANDING_REG_SIZE);

    return ret;
}

static esp_err_t gc2145_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;
    switch (qdesc->id) {
    case ESP_CAM_SENSOR_VFLIP:
    case ESP_CAM_SENSOR_HMIRROR:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_AE_LEVEL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0x2f;
        qdesc->number.maximum = 0x95;
        qdesc->number.step = 1;
        qdesc->default_value = GC2145_AEC_TARGET_DEFAULT;
        break;
    case ESP_CAM_SENSOR_BRIGHTNESS:
    case ESP_CAM_SENSOR_CONTRAST:
    case ESP_CAM_SENSOR_SATURATION:
    case ESP_CAM_SENSOR_SHARPNESS:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0x00;
        qdesc->number.maximum = 0xff;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_SPECIAL_EFFECT:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = GC2145_EFFECT_ENC_NORMAL;
        qdesc->number.maximum = GC2145_EFFECT_ENC_COLORINV;
        qdesc->number.step = 1;
        qdesc->default_value = GC2145_EFFECT_ENC_NORMAL;
        break;
    case ESP_CAM_SENSOR_SCENE:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = GC2145_SCENE_MODE_NORMAL;
        qdesc->number.maximum = GC2145_SCENE_MODE_PORTRAIT;
        qdesc->number.step = 1;
        qdesc->default_value = GC2145_SCENE_MODE_NORMAL;
        break;
    case ESP_CAM_SENSOR_AE_FLICKER:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = GC2145_BANDING_OFF;
        qdesc->number.maximum = GC2145_BANDING_AUTO;
        qdesc->number.step = 1;
        qdesc->default_value = GC2145_BANDING_50HZ;
        break;
    case ESP_CAM_SENSOR_AUTO_N_PRESET_WB:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = GC2145_WB_AUTO;
        qdesc->number.maximum = GC2145_WB_MANUAL;
        qdesc->number.step = 1;
        qdesc->default_value = GC2145_WB_AUTO;
        break;
    default: {
        ESP_LOGD(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t gc2145_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t gc2145_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;

        ret = gc2145_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;

        ret = gc2145_set_mirror(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_AE_LEVEL: {
        int *value = (int *)arg;

        ret = gc2145_set_exposure(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_BRIGHTNESS: {
        int *value = (int *)arg;

        ret = gc2145_set_brightness(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_CONTRAST: {
        int *value = (int *)arg;

        ret = gc2145_set_contrast(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_SATURATION: {
        int *value = (int *)arg;

        ret = gc2145_set_saturation(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_SHARPNESS: {
        int *value = (int *)arg;

        ret = gc2145_set_sharpness(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_SPECIAL_EFFECT: {
        int *value = (int *)arg;

        ret = gc2145_set_effect(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_SCENE: {
        int *value = (int *)arg;

        ret = gc2145_set_scene_mode(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_AE_FLICKER: {
        int *value = (int *)arg;

        ret = gc2145_set_antibanding(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_AUTO_N_PRESET_WB: {
        int *value = (int *)arg;

        ret = gc2145_set_wb_mode(dev, *value);
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

static esp_err_t gc2145_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(gc2145_format_info);
    formats->format_array = &gc2145_format_info[0];
    return ESP_OK;
}

static esp_err_t gc2145_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_rgb565 = 1;
    sensor_cap->fmt_yuv = 1;
    return 0;
}

static esp_err_t gc2145_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        if (dev->sensor_port != ESP_CAM_SENSOR_DVP) {
            format = &gc2145_format_info[CONFIG_CAMERA_GC2145_MIPI_IF_FORMAT_INDEX_DAFAULT];
        } else {
            format = &gc2145_format_info[CONFIG_CAMERA_GC2145_DVP_IF_FORMAT_INDEX_DAFAULT];
        }
    }

    ret = gc2145_write_array(dev->sccb_handle, (gc2145_reginfo_t *)format->regs, format->regs_size);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;

    return ret;
}

static esp_err_t gc2145_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t gc2145_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    GC2145_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = gc2145_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = gc2145_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = gc2145_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = gc2145_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = gc2145_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = gc2145_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = gc2145_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    GC2145_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t gc2145_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        GC2145_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
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

static esp_err_t gc2145_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        GC2145_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t gc2145_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del gc2145 (%p)", dev);
    if (dev) {
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t gc2145_ops = {
    .query_para_desc = gc2145_query_para_desc,
    .get_para_value = gc2145_get_para_value,
    .set_para_value = gc2145_set_para_value,
    .query_support_formats = gc2145_query_support_formats,
    .query_support_capability = gc2145_query_support_capability,
    .set_format = gc2145_set_format,
    .get_format = gc2145_get_format,
    .priv_ioctl = gc2145_priv_ioctl,
    .del = gc2145_delete
};

esp_cam_sensor_device_t *gc2145_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    dev->name = (char *)GC2145_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &gc2145_ops;
    if (config->sensor_port != ESP_CAM_SENSOR_DVP) {
        dev->cur_format = &gc2145_format_info[CONFIG_CAMERA_GC2145_MIPI_IF_FORMAT_INDEX_DAFAULT];
    } else {
        dev->cur_format = &gc2145_format_info[CONFIG_CAMERA_GC2145_DVP_IF_FORMAT_INDEX_DAFAULT];
    }

    // Configure sensor power, clock, and SCCB port
    if (gc2145_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (gc2145_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != GC2145_PID) {
        ESP_LOGE(TAG, "Camera sensor is not GC2145, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    gc2145_power_off(dev);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_GC2145_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(gc2145_detect, ESP_CAM_SENSOR_MIPI_CSI, GC2145_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return gc2145_detect(config);
}
#endif

#if CONFIG_CAMERA_GC2145_AUTO_DETECT_DVP_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(gc2145_detect, ESP_CAM_SENSOR_DVP, GC2145_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_DVP;
    return gc2145_detect(config);
}
#endif
