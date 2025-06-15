/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/timers.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_cam_sensor.h"
#include "esp_cam_sensor_detect.h"
#include "ov2710.h"
#include "ov2710_settings.h"

typedef struct {
    uint32_t ae_target_level;
#if CONFIG_CAMERA_OV2710_STATS_UPDATE_EN
    esp_cam_sensor_stats_t stats; // statistical data for IPA algorithms
#endif
    uint32_t vflip_en : 1;
    uint32_t hmirror_en : 1;
} ov2710_para_t;

struct ov2710_cam {
    ov2710_para_t ov2710_para;
#if CONFIG_CAMERA_OV2710_STATS_UPDATE_EN
    TimerHandle_t wb_timer_handle;
#endif
};

#define OV2710_IO_MUX_LOCK(mux)
#define OV2710_IO_MUX_UNLOCK(mux)
#define OV2710_ENABLE_OUT_XCLK(pin,clk)
#define OV2710_DISABLE_OUT_XCLK(pin)

#define OV2710_PID         0x2710
#define OV2710_SENSOR_NAME "OV2710"
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define OV2710_SUPPORT_NUM CONFIG_CAMERA_OV2710_MAX_SUPPORT
#define OV2710_AEC_TARGET_DEFAULT (0x30)
#define OV2710_MCLK              (24*1000*1000)

static const char *TAG = "ov2710";
#if CONFIG_CAMERA_OV2710_STATS_UPDATE_EN
static portMUX_TYPE s_stats_mutex = portMUX_INITIALIZER_UNLOCKED;
#endif

static const esp_cam_sensor_isp_info_t ov2710_isp_info[] = {
    /* For MIPI */
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 80000000,
            .vts = 1104,
            .hts = 2420,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 80000000,
            .vts = 744,
            .hts = 1792,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        },
    },
};

static const esp_cam_sensor_format_t ov2710_format_info[] = {
    /* For MIPI */
    {
        .name = "MIPI_1lane_24Minput_RAW10_1920x1080_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = OV2710_MCLK,
        .width = 1920,
        .height = 1080,
        .regs = init_reglist_MIPI_1lane_1920_1080_30fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_1lane_1920_1080_30fps),
        .fps = 30,
        .isp_info = &ov2710_isp_info[0],
        .mipi_info = {
            .mipi_clk = 800000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
    {
        .name = "MIPI_1lane_24Minput_RAW10_1280x720_60fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW10,
        .port = ESP_CAM_SENSOR_MIPI_CSI,
        .xclk = OV2710_MCLK,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_MIPI_1lane_1280_720_60fps,
        .regs_size = ARRAY_SIZE(init_reglist_MIPI_1lane_1280_720_60fps),
        .fps = 60,
        .isp_info = &ov2710_isp_info[1],
        .mipi_info = {
            .mipi_clk = 800000000,
            .lane_num = 1,
            .line_sync_en = false,
        },
        .reserved = NULL,
    },
};

static esp_err_t ov2710_read(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t *read_buf)
{
    return esp_sccb_transmit_receive_reg_a16v8(sccb_handle, reg, read_buf);
}

static esp_err_t ov2710_write(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t data)
{
    return esp_sccb_transmit_reg_a16v8(sccb_handle, reg, data);
}

/* write a array of registers  */
static esp_err_t ov2710_write_array(esp_sccb_io_handle_t sccb_handle, ov2710_reginfo_t *regarray)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && regarray[i].reg != OV2710_REG_END) {
        if (regarray[i].reg != OV2710_REG_DELAY) {
            ret = ov2710_write(sccb_handle, regarray[i].reg, regarray[i].val);
        } else {
            delay_ms(regarray[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "Set array done[i=%d]", i);
    return ret;
}

static esp_err_t ov2710_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint16_t reg, uint8_t offset, uint8_t length, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t reg_data = 0;

    ret = ov2710_read(sccb_handle, reg, &reg_data);
    if (ret != ESP_OK) {
        return ret;
    }
    uint8_t mask = ((1 << length) - 1) << offset;
    value = (ret & ~mask) | ((value << offset) & mask);
    ret = ov2710_write(sccb_handle, reg, value);
    return ret;
}

#if CONFIG_CAMERA_OV2710_STATS_UPDATE_EN
static void wb_timer_callback(TimerHandle_t timer)
{
    uint8_t read_v[5] = {0};
    esp_cam_sensor_device_t *dev = (esp_cam_sensor_device_t *)pvTimerGetTimerID(timer);
    struct ov2710_cam *cam_ov2710 = (struct ov2710_cam *)dev->priv;

    ov2710_read(dev->sccb_handle, OV2710_REG_RED_BEFORE_GAIN_AVERAGE, &read_v[0]);
    ov2710_read(dev->sccb_handle, OV2710_REG_GREEN_BEFORE_GAIN_AVERAGE, &read_v[1]);
    ov2710_read(dev->sccb_handle, OV2710_REG_BLUE_BEFORE_GAIN_AVERAGE, &read_v[2]);
    ov2710_read(dev->sccb_handle, OV2710_REG_AEC_AGC_ADJ_MSB, &read_v[3]);
    ov2710_read(dev->sccb_handle, OV2710_REG_AEC_AGC_ADJ_LSB, &read_v[4]);

    portENTER_CRITICAL(&s_stats_mutex);
    cam_ov2710->ov2710_para.stats.wb_avg.red_avg = read_v[0];
    cam_ov2710->ov2710_para.stats.wb_avg.green_avg = read_v[1];
    cam_ov2710->ov2710_para.stats.wb_avg.blue_avg = read_v[2];
    cam_ov2710->ov2710_para.stats.agc_gain = ((read_v[3] & 0x01) + 1) * ((read_v[4] & 0x80) + 1) * ((read_v[4] & 0x40) + 1) * ((read_v[4] & 0x20) + 1) * ((read_v[4] & 0x10) + 1) * ((read_v[4] & 0x0f) / 16 + 1);
    cam_ov2710->ov2710_para.stats.seq++;
    portEXIT_CRITICAL(&s_stats_mutex);
}
#endif

static esp_err_t ov2710_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return ov2710_set_reg_bits(dev->sccb_handle, 0x503d, 7, 1, enable ? 0x01 : 0x00);
}

static esp_err_t ov2710_hw_reset(esp_cam_sensor_device_t *dev)
{
    return ESP_OK;
}

static esp_err_t ov2710_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ov2710_set_reg_bits(dev->sccb_handle, 0x3008, 7, 1, 0x01);
    delay_ms(5);
    return ret;
}

static esp_err_t ov2710_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid_h, pid_l;

    ret = ov2710_read(dev->sccb_handle, OV2710_REG_SENSOR_ID_H, &pid_h);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = ov2710_read(dev->sccb_handle, OV2710_REG_SENSOR_ID_L, &pid_l);
    if (ret != ESP_OK) {
        return ret;
    }
    id->pid = (pid_h << 8) | pid_l;

    return ret;
}

static esp_err_t ov2710_set_AE_target(esp_cam_sensor_device_t *dev, int target)
{
    esp_err_t ret = ESP_OK;
    /* stable in high */
    int fast_high, fast_low;
    int AE_low = target * 23 / 25;  /* 0.92 */
    int AE_high = target * 27 / 25; /* 1.08 */

    fast_high = AE_high << 1;
    if (fast_high > 255) {
        fast_high = 255;
    }

    fast_low = AE_low >> 1;

    ret |= ov2710_write(dev->sccb_handle, 0x3a0f, AE_high);
    ret |= ov2710_write(dev->sccb_handle, 0x3a10, AE_low);
    ret |= ov2710_write(dev->sccb_handle, 0x3a1b, AE_high + 1);
    ret |= ov2710_write(dev->sccb_handle, 0x3a1e, AE_low - 1);
    ret |= ov2710_write(dev->sccb_handle, 0x3a11, fast_high); // Notes that this para not used in auto aec mode.
    ret |= ov2710_write(dev->sccb_handle, 0x3a1f, fast_low);

    return ret;
}

static esp_err_t ov2710_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_OK;
    struct ov2710_cam *cam_ov2710 = (struct ov2710_cam *)dev->priv;
    if (enable) {
        ret = ov2710_write(dev->sccb_handle, 0x4201, 0x00);
        ret |= ov2710_write(dev->sccb_handle, 0x4202, 0x00);
        ret = ov2710_write(dev->sccb_handle, 0x3008, 0x02);
#if CONFIG_CAMERA_OV2710_STATS_UPDATE_EN
        if (pdTRUE != xTimerStart(cam_ov2710->wb_timer_handle, portMAX_DELAY)) {
            ESP_LOGE(TAG, "Timer start err");
        }
#endif
    } else {
        ret = ov2710_write(dev->sccb_handle, 0x3008, 0x42);
        ret |= ov2710_write(dev->sccb_handle, 0x4201, 0x00);
        ret |= ov2710_write(dev->sccb_handle, 0x4202, 0x0f);
#if CONFIG_CAMERA_OV2710_STATS_UPDATE_EN
        if (pdTRUE != xTimerStop(cam_ov2710->wb_timer_handle, 10)) {
            ESP_LOGE(TAG, "Timer stop err");
        }
#endif
    }

    dev->stream_status = enable;
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t ov2710_set_mirror(esp_cam_sensor_device_t *dev, int enable)
{
    return ov2710_set_reg_bits(dev->sccb_handle, 0x3818, 6, 1, enable ? 0x01 : 0x00);
}

static esp_err_t ov2710_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    return ov2710_set_reg_bits(dev->sccb_handle, 0x3818, 5, 1, enable ? 0x01 : 0x00);
}

static esp_err_t ov2710_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
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
        qdesc->number.minimum = 2;
        qdesc->number.maximum = 235;
        qdesc->number.step = 1;
        qdesc->default_value = OV2710_AEC_TARGET_DEFAULT;
        break;
    case ESP_CAM_SENSOR_STATS:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_U8;
        qdesc->u8.size = sizeof(esp_cam_sensor_stats_t);
        break;
    default: {
        ESP_LOGD(TAG, "id=%"PRIx32" is not supported", qdesc->id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }
    return ret;
}

static esp_err_t ov2710_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct ov2710_cam *cam_ov2710 = (struct ov2710_cam *)dev->priv;

    switch (id) {
    case ESP_CAM_SENSOR_AE_LEVEL: {
        ESP_RETURN_ON_FALSE(size == 4, ESP_ERR_INVALID_ARG, TAG, "Para size err");
        *(uint32_t *)arg = cam_ov2710->ov2710_para.ae_target_level;
    }
    break;
#if CONFIG_CAMERA_OV2710_STATS_UPDATE_EN
    case ESP_CAM_SENSOR_STATS: {
        ESP_RETURN_ON_FALSE(size == sizeof(esp_cam_sensor_stats_t), ESP_ERR_INVALID_ARG, TAG, "Para size err");
        portENTER_CRITICAL(&s_stats_mutex);
        memcpy((esp_cam_sensor_stats_t *)arg, &cam_ov2710->ov2710_para.stats, sizeof(esp_cam_sensor_stats_t));
        portEXIT_CRITICAL(&s_stats_mutex);
    }
    break;
#endif
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t ov2710_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    // uint32_t u32_val = *(uint32_t *)arg;
    struct ov2710_cam *cam_ov2710 = (struct ov2710_cam *)dev->priv;

    switch (id) {
    case ESP_CAM_SENSOR_VFLIP: {
        int *value = (int *)arg;
        ret = ov2710_set_vflip(dev, *value);
        if (ret == ESP_OK) {
            cam_ov2710->ov2710_para.vflip_en = *value ? 0x01 : 0x00;
        }
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        int *value = (int *)arg;
        ret = ov2710_set_mirror(dev, *value);
        if (ret == ESP_OK) {
            cam_ov2710->ov2710_para.hmirror_en = *value ? 0x01 : 0x00;
        }
        break;
    }
    case ESP_CAM_SENSOR_AE_LEVEL: {
        int *value = (int *)arg;
        ret = ov2710_set_AE_target(dev, *value);
        if (ret == ESP_OK) {
            cam_ov2710->ov2710_para.ae_target_level = *value;
        }
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

static esp_err_t ov2710_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(ov2710_format_info);
    formats->format_array = &ov2710_format_info[0];
    return ESP_OK;
}

static esp_err_t ov2710_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_raw = 1;
    return 0;
}

static esp_err_t ov2710_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    esp_err_t ret = ESP_OK;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        format = &ov2710_format_info[CONFIG_CAMERA_OV2710_MIPI_IF_FORMAT_INDEX_DAFAULT];
    }

    ret = ov2710_write_array(dev->sccb_handle, (ov2710_reginfo_t *)format->regs);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set format regs fail");
        return ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT;
    }

    ret |= ov2710_set_AE_target(dev, OV2710_AEC_TARGET_DEFAULT);
    ESP_LOGD(TAG, "Set fmt done");

    dev->cur_format = format;

    return ret;
}

static esp_err_t ov2710_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t ov2710_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    OV2710_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = ov2710_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = ov2710_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = ov2710_write(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = ov2710_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = ov2710_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = ov2710_read(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = ov2710_get_sensor_id(dev, arg);
        break;
    default:
        break;
    }

    OV2710_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t ov2710_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        OV2710_ENABLE_OUT_XCLK(dev->xclk_pin, dev->xclk_freq_hz);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << dev->pwdn_pin;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        // carefully, logic is inverted compared to reset pin
        gpio_set_level(dev->pwdn_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 1);
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

static esp_err_t ov2710_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        OV2710_DISABLE_OUT_XCLK(dev->xclk_pin);
    }

    if (dev->pwdn_pin >= 0) {
        gpio_set_level(dev->pwdn_pin, 1);
        delay_ms(10);
        gpio_set_level(dev->pwdn_pin, 0);
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

static esp_err_t ov2710_delete(esp_cam_sensor_device_t *dev)
{
    struct ov2710_cam *cam_ov2710 = (struct ov2710_cam *)dev->priv;
    ESP_LOGD(TAG, "del ov2710 (%p)", dev);
    if (dev) {
        if (dev->priv) {
#if CONFIG_CAMERA_OV2710_STATS_UPDATE_EN
            xTimerDelete(cam_ov2710->wb_timer_handle, portMAX_DELAY);
#endif
            free(dev->priv);
            dev->priv = NULL;
        }
        free(dev);
        dev = NULL;
    }

    return ESP_OK;
}

static const esp_cam_sensor_ops_t ov2710_ops = {
    .query_para_desc = ov2710_query_para_desc,
    .get_para_value = ov2710_get_para_value,
    .set_para_value = ov2710_set_para_value,
    .query_support_formats = ov2710_query_support_formats,
    .query_support_capability = ov2710_query_support_capability,
    .set_format = ov2710_set_format,
    .get_format = ov2710_get_format,
    .priv_ioctl = ov2710_priv_ioctl,
    .del = ov2710_delete
};

esp_cam_sensor_device_t *ov2710_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct ov2710_cam *cam_ov2710;

    if (config == NULL) {
        return NULL;
    }

    dev = calloc(1, sizeof(esp_cam_sensor_device_t));
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_ov2710 = heap_caps_calloc(1, sizeof(struct ov2710_cam), MALLOC_CAP_DEFAULT);
    if (!cam_ov2710) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }
    memset(cam_ov2710, 0x0, sizeof(struct ov2710_cam));

    dev->name = (char *)OV2710_SENSOR_NAME;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->sensor_port = config->sensor_port;
    dev->ops = &ov2710_ops;
    dev->priv = cam_ov2710;

    if (config->sensor_port != ESP_CAM_SENSOR_DVP) {
        dev->cur_format = &ov2710_format_info[CONFIG_CAMERA_OV2710_MIPI_IF_FORMAT_INDEX_DAFAULT];
    }

    // Configure sensor power, clock, and SCCB port
    if (ov2710_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "Camera power on failed");
        goto err_free_handler;
    }

    if (ov2710_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "Get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != OV2710_PID) {
        ESP_LOGE(TAG, "Camera sensor is not OV2710, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

#if CONFIG_CAMERA_OV2710_STATS_UPDATE_EN
    // Init cam privae pata
    cam_ov2710->ov2710_para.stats.flags = ESP_CAM_SENSOR_STATS_FLAG_WB_GAIN | ESP_CAM_SENSOR_STATS_FLAG_AGC_GAIN;
    cam_ov2710->wb_timer_handle = xTimerCreate("wb_t", CONFIG_CAMERA_OV2710_STATS_UPDATE_INTERVAL / portTICK_PERIOD_MS, pdTRUE,
                                  (void *)dev, wb_timer_callback);
    if (!cam_ov2710->wb_timer_handle) {
        ESP_LOGE(TAG, "Init WB timer failed");
        goto err_free_handler;
    }
#endif

    return dev;

err_free_handler:
    ov2710_power_off(dev);
    free(dev->priv);
    free(dev);

    return NULL;
}

#if CONFIG_CAMERA_OV2710_AUTO_DETECT_MIPI_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(ov2710_detect, ESP_CAM_SENSOR_MIPI_CSI, OV2710_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_MIPI_CSI;
    return ov2710_detect(config);
}
#endif
