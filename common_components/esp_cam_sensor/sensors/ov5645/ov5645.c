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
#include "ov5645_settings.h"
#include "ov5645.h"

#define OV5645_IO_MUX_LOCK(mux)
#define OV5645_IO_MUX_UNLOCK(mux)
#define OV5645_ENABLE_OUT_XCLK(pin,clk)
#define OV5645_DISABLE_OUT_XCLK(pin)

#define OV5645_PID         0x5645
#define OV5645_SENSOR_NAME "OV5645"
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define OV5645_SUPPORT_NUM CONFIG_CAMERA_OV5645_MAX_SUPPORT

static const char *TAG = "ov5645";

static const esp_cam_sensor_format_t ov5645_format_info[] = {
    {
        .name = "MIPI_2lane_24Minput_YUV422_1280x960_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 960,
        .regs = ov5645_MIPI_2lane_yuv422_960p_30fps,
        .regs_size = ARRAY_SIZE(ov5645_MIPI_2lane_yuv422_960p_30fps),
        .fps = 30,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = OV5645_LINE_RATE_16BITS_1280x960_30FPS,
            .lane_num = 2,
            .line_sync_en = CONFIG_CAMERA_OV5645_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_RGB565_1280x960_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RGB565,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 960,
        .regs = ov5645_MIPI_2lane_rgb565_960p_30fps,
        .regs_size = ARRAY_SIZE(ov5645_MIPI_2lane_rgb565_960p_30fps),
        .fps = 30,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = OV5645_LINE_RATE_16BITS_1280x960_30FPS,
            .lane_num = 2,
            .line_sync_en = CONFIG_CAMERA_OV5645_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_YUV420_1280x960_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV420,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1280,
        .height = 960,
        .regs = ov5645_MIPI_2lane_yuv420_960p_30fps,
        .regs_size = ARRAY_SIZE(ov5645_MIPI_2lane_yuv420_960p_30fps),
        .fps = 30,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = OV5645_LINE_RATE_16BITS_1280x960_30FPS,
            .lane_num = 2,
            .line_sync_en = CONFIG_CAMERA_OV5645_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_YUV422_2592x1944_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 2592,
        .height = 1944,
        .regs = ov5645_MIPI_2lane_yuv422_2592x1944_15fps,
        .regs_size = ARRAY_SIZE(ov5645_MIPI_2lane_yuv422_2592x1944_15fps),
        .fps = 15,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = OV5645_LINE_RATE_16BITS_2592x1944_15FPS,
            .lane_num = 2,
            .line_sync_en = CONFIG_CAMERA_OV5645_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_YUV422_1920x1080_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 1920,
        .height = 1080,
        .regs = ov5645_MIPI_2lane_yuv422_1080p_15fps,
        .regs_size = ARRAY_SIZE(ov5645_MIPI_2lane_yuv422_1080p_15fps),
        .fps = 15,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = OV5645_LINE_RATE_16BITS_1920x1080_15FPS,
            .lane_num = 2,
            .line_sync_en = CONFIG_CAMERA_OV5645_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_2lane_24Minput_YUV422_640x480_24fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = 24000000,
        .width = 640,
        .height = 480,
        .regs = ov5645_MIPI_2lane_yuv422_640x480_24fps,
        .regs_size = ARRAY_SIZE(ov5645_MIPI_2lane_yuv422_640x480_24fps),
        .fps = 24,
        .isp_info = NULL,
        .mipi_info = {
            .mipi_clk = OV5645_LINE_RATE_16BITS_640x480_24FPS,
            .lane_num = 2,
            .line_sync_en = CONFIG_CAMERA_OV5645_CSI_LINESYNC_ENABLE ? true : false,
        },
        .reserved = NULL,
    },
};

static esp_err_t ov5645_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t ov5645_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t ov5645_write_array(esp_sccb_io_handle_t sccb_handle, const ov5645_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != OV5645_REG_END) {
        if (regarray[i].reg != OV5645_REG_DELAY) {
            ret = ov5645_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "count=%d", i);
    return ret;
}

static esp_err_t ov5645_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = ov5645_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (reg_data & ~mask) | ((value << offset) & mask);
    ret = ov5645_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t ov5645_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return ov5645_set_reg_bits(dev->sccb_handle, 0x503d, 7, 1, enable ? 1 : 0);
}

static esp_err_t ov5645_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t ov5645_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ov5645_set_reg_bits(dev->sccb_handle, 0x3008, 7, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t ov5645_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    uint8_t pid_h, pid_l;
    esp_err_t ret = ov5645_read(dev->sccb_handle, OV5645_REG_SENSOR_ID_H, &pid_h);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "read pid_h failed");

    ret = ov5645_read(dev->sccb_handle, OV5645_REG_SENSOR_ID_L, &pid_l);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "read pid_l failed");

    uint16_t pid = (pid_h << 8) | pid_l;
    if (pid) {
        id->pid = pid;
    }
    return ret;
}

static esp_err_t ov5645_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    if (enable) {
        ret = ov5645_write_array(dev->sccb_handle, ov5645_mipi_stream_on);
    } else {
        ret = ov5645_write_array(dev->sccb_handle, ov5645_mipi_stream_off);
    }
    if (ret == ESP_OK) {
        dev->stream_status = (uint8_t)enable;
    }

    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t ov5645_set_hmirror(esp_cam_sensor_device_t *dev, int enable)
{
    return ov5645_set_reg_bits(dev->sccb_handle, 0x3821, 2, 1, enable ? 1 : 0);
}

static esp_err_t ov5645_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return ov5645_set_reg_bits(dev->sccb_handle, 0x3820, 2, 1, enable ? 1 : 0);
}

static esp_err_t ov5645_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
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
    default: {
        ESP_LOGD(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t ov5645_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t ov5645_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        ret = ov5645_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        ret = ov5645_set_hmirror(dev, *value);
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

static esp_err_t ov5645_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(ov5645_format_info);
    formats->format_array = &ov5645_format_info[0];
    return ESP_OK;
}

static esp_err_t ov5645_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_yuv = 1;
    return ESP_OK;
}

static esp_err_t ov5645_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &ov5645_format_info[CONFIG_CAMERA_OV5645_MIPI_IF_FORMAT_INDEX_DAFAULT];
    }

    ret = ov5645_write_array(dev->sccb_handle, ov5645_mipi_reset_regs);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "write reset regs failed");

    ret = ov5645_write_array(dev->sccb_handle, (const ov5645_reginfo_t *)format->regs);
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT, TAG, "write format regs failed");

    dev->cur_format = format;

    return ret;
}

static esp_err_t ov5645_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t ov5645_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    OV5645_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = ov5645_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = ov5645_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = ov5645_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = ov5645_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = ov5645_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = ov5645_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = ov5645_get_sensor_id(dev, arg);
        break;
    default:
        ESP_LOGE(TAG, "cmd=%" PRIx32 " is not supported", cmd);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }

    OV5645_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t ov5645_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        OV5645_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        ret = gpio_config(&conf);

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
        ret = gpio_config(&conf);

        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }

    return ret;
}

static esp_err_t ov5645_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        OV5645_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t ov5645_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del ov5645 (%p)", dev);
    if (dev) {
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t ov5645_ops = {
    .query_para_desc = ov5645_query_para_desc,
    .get_para_value = ov5645_get_para_value,
    .set_para_value = ov5645_set_para_value,
    .query_support_formats = ov5645_query_support_formats,
    .query_support_capability = ov5645_query_support_capability,
    .set_format = ov5645_set_format,
    .get_format = ov5645_get_format,
    .priv_ioctl = ov5645_priv_ioctl,
    .del = ov5645_delete
};

esp_cam_sensor_device_t *ov5645_detect(esp_cam_sensor_config_t *config)
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

    dev->name = (char *)OV5645_SENSOR_NAME;
    dev->ops = &ov5645_ops;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    if (config->sensor_port == ESP_CAM_SENSOR_MIPI_CSI) {
        dev->cur_format = &ov5645_format_info[CONFIG_CAMERA_OV5645_MIPI_IF_FORMAT_INDEX_DAFAULT];
    } else {
        ESP_LOGE(TAG, "Not support DVP port");
    }

    // Configure sensor power, clock, and SCCB port
    if (ov5645_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (ov5645_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Camera get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != OV5645_PID) {
        ESP_LOGE(TAG, "Camera sensor is not OV5645, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    ov5645_power_off(dev);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_OV5645_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(ov5645_detect, ESP_CAM_SENSOR_MIPI_CSI, OV5645_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return ov5645_detect(config);
}
#endif
