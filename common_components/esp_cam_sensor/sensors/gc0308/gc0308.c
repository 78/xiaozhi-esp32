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
#include "gc0308_settings.h"
#include "gc0308.h"

#define GC0308_IO_MUX_LOCK(mux)
#define GC0308_IO_MUX_UNLOCK(mux)
#define GC0308_ENABLE_OUT_XCLK(pin,clk)
#define GC0308_DISABLE_OUT_XCLK(pin)

#define GC0308_PID         0x9b
#define GC0308_SENSOR_NAME "GC0308"
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define GC0308_SUPPORT_NUM CONFIG_CAMERA_GC0308_MAX_SUPPORT

static const char *TAG = "gc0308";

static const esp_cam_sensor_format_t gc0308_format_info[] = {
    {
        .name = "DVP_8bit_20Minput_YUV422_640x480_16fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 640,
        .height = 480,
        .regs = DVP_8bit_20Minput_640x480_yuv422_16fps,
        .regs_size = ARRAY_SIZE(DVP_8bit_20Minput_640x480_yuv422_16fps),
        .fps = 16,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_RGB565_640x480_16fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RGB565,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 640,
        .height = 480,
        .regs = DVP_8bit_20Minput_640x480_rgb565_16fps,
        .regs_size = ARRAY_SIZE(DVP_8bit_20Minput_640x480_rgb565_16fps),
        .fps = 16,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_grayscale_640x480_16fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 640,
        .height = 480,
        .regs = DVP_8bit_20Minput_640x480_only_y_16fps,
        .regs_size = ARRAY_SIZE(DVP_8bit_20Minput_640x480_only_y_16fps),
        .fps = 16,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_YUV422_320x240_20fps_subsample",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 320,
        .height = 240,
        .regs = DVP_8bit_20Minput_320x240_yuv422_20fps_subsample,
        .regs_size = ARRAY_SIZE(DVP_8bit_20Minput_320x240_yuv422_20fps_subsample),
        .fps = 20,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_grayscale_320x240_20fps_subsample",
        .format = ESP_CAM_SENSOR_PIXFORMAT_GRAYSCALE,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 320,
        .height = 240,
        .regs = DVP_8bit_20Minput_320x240_only_y_20fps_subsample,
        .regs_size = ARRAY_SIZE(DVP_8bit_20Minput_320x240_only_y_20fps_subsample),
        .fps = 20,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_RGB565_320x240_20fps_subsample",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RGB565,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 320,
        .height = 240,
        .regs = DVP_8bit_20Minput_320x240_rgb565_20fps_subsample,
        .regs_size = ARRAY_SIZE(DVP_8bit_20Minput_320x240_rgb565_20fps_subsample),
        .fps = 20,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
};

static esp_err_t gc0308_read(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a8v8(sccb_handle, reg, read_buf);
}

static esp_err_t gc0308_write(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a8v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t gc0308_write_array(esp_sccb_io_handle_t sccb_handle, gc0308_reginfo_t *regarray, size_t regs_size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && (i < regs_size)) {
        if (regarray[i].reg != GC0308_REG_DELAY) {
            ret = gc0308_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    return ret;
}

static esp_err_t gc0308_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint8_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = gc0308_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = gc0308_write(sccb_handle, reg, value);
    return ret;
}

static esp_err_t gc0308_select_page(esp_cam_sensor_device_t *dev, uint8_t page)
{
    return gc0308_write(dev->sccb_handle, GC0308_REG_PAGE_SELECT, page);
}

static esp_err_t gc0308_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_OK;
    ret = gc0308_select_page(dev, 0x00);
    if (enable) {
        ret |= gc0308_set_reg_bits(dev->sccb_handle, GC0308_REG_DEBUG_MODE, 0, 0x01, 0x01);
    } else {
        ret |= gc0308_set_reg_bits(dev->sccb_handle, GC0308_REG_DEBUG_MODE, 0, 0x01, 0x00);
    }
    return ret;
}

static esp_err_t gc0308_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t gc0308_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = gc0308_select_page(dev, 0x00);
    ret |= gc0308_set_reg_bits(dev->sccb_handle, GC0308_REG_PAGE_SELECT, 7, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t gc0308_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h;
    ret = gc0308_select_page(dev, 0x00);
    ret |= gc0308_read(dev->sccb_handle, 0x00, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = pid_h;

    return ret;
}

static esp_err_t gc0308_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = gc0308_select_page(dev, 0x00);
    if (enable) {
        ret |= gc0308_set_reg_bits(dev->sccb_handle, GC0308_REG_ANALOG_MODE, 0, 1, 0);
        ret |= gc0308_write(dev->sccb_handle, GC0308_REG_OUTPUT_EN, 0x0f);
    } else {
        ret |= gc0308_set_reg_bits(dev->sccb_handle, GC0308_REG_ANALOG_MODE, 0, 1, 0x01);
        ret |= gc0308_write(dev->sccb_handle, GC0308_REG_OUTPUT_EN, 0x00);
    }

    if (ret == ESP_OK) {
        dev->stream_status = enable;
    }
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t gc0308_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = gc0308_select_page(dev, 0x00);
    ret |= gc0308_set_reg_bits(dev->sccb_handle, GC0308_REG_CISCTL_MODE1, 0, 0x01, enable != 0);
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "Set h-mirror to: %d", enable);
    }
    return ret;
}

static esp_err_t gc0308_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    ret = gc0308_select_page(dev, 0x00);
    ret |= gc0308_set_reg_bits(dev->sccb_handle, GC0308_REG_CISCTL_MODE1, 1, 0x01, enable != 0);
    if (ret == 0) {
        ESP_LOGD(TAG, "Set vflip to: %d", enable);
    }

    return ret;
}

static esp_err_t gc0308_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
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

static esp_err_t gc0308_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t gc0308_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;

    switch (id) {
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;

        ret = gc0308_set_vflip(dev, *value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;

        ret = gc0308_set_mirror(dev, *value);
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

static esp_err_t gc0308_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(gc0308_format_info);
    formats->format_array = &gc0308_format_info[0];
    return ESP_OK;
}

static esp_err_t gc0308_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_rgb565 = 1;
    sensor_cap->fmt_yuv = 1;
    return 0;
}

static esp_err_t gc0308_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &gc0308_format_info[CONFIG_CAMERA_GC0308_DVP_IF_FORMAT_INDEX_DAFAULT];
    }

    ret = gc0308_write_array(dev->sccb_handle, (gc0308_reginfo_t *)format->regs, format->regs_size);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    dev->cur_format = format;

    return ret;
}

static esp_err_t gc0308_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t gc0308_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    GC0308_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = gc0308_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = gc0308_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = gc0308_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = gc0308_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = gc0308_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = gc0308_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = gc0308_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    GC0308_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t gc0308_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        GC0308_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
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

static esp_err_t gc0308_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        GC0308_DISABLE_OUT_XCLK(dev->xclk_pin);
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

static esp_err_t gc0308_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del gc0308 (%p)", dev);
    if (dev) {
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t gc0308_ops = {
    .query_para_desc = gc0308_query_para_desc,
    .get_para_value = gc0308_get_para_value,
    .set_para_value = gc0308_set_para_value,
    .query_support_formats = gc0308_query_support_formats,
    .query_support_capability = gc0308_query_support_capability,
    .set_format = gc0308_set_format,
    .get_format = gc0308_get_format,
    .priv_ioctl = gc0308_priv_ioctl,
    .del = gc0308_delete
};

esp_cam_sensor_device_t *gc0308_detect(esp_cam_sensor_config_t *config)
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

    dev->name = (char *)GC0308_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &gc0308_ops;
    dev->cur_format = &gc0308_format_info[CONFIG_CAMERA_GC0308_DVP_IF_FORMAT_INDEX_DAFAULT];

    // Configure sensor power, clock, and SCCB port
    if (gc0308_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (gc0308_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != GC0308_PID) {
        ESP_LOGE(TAG, "Camera sensor is not GC0308, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    gc0308_power_off(dev);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_GC0308_AUTO_DETECT_DVP_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(gc0308_detect, ESP_CAM_SENSOR_DVP, GC0308_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_DVP;
    return gc0308_detect(config);
}
#endif
