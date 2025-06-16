/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
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
#include "ov2640_settings.h"
#include "ov2640.h"

struct ov2640_cam {
    uint8_t jpeg_quality;
    int8_t ae_level;
    int8_t brightness;
    int8_t contrast;
    int8_t saturation;
    uint8_t special_effect;
    uint8_t wb_mode;
};

#define OV2640_IO_MUX_LOCK(mux)
#define OV2640_IO_MUX_UNLOCK(mux)
#define OV2640_ENABLE_OUT_CLOCK(pin,clk)
#define OV2640_DISABLE_OUT_CLOCK(pin)

#define OV2640_PID         0x26
#define OV2640_SENSOR_NAME "OV2640"
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif
#define delay_ms(ms)  vTaskDelay((ms > portTICK_PERIOD_MS ? ms/ portTICK_PERIOD_MS : 1))
#define OV2640_SUPPORT_NUM CONFIG_CAMERA_OV2640_MAX_SUPPORT

static volatile ov2640_bank_t s_reg_bank = BANK_MAX; // ov2640 current bank
static const char *TAG = "ov2640";

static const esp_cam_sensor_isp_info_t ov2640_isp_info[] = {
    {
        .isp_v1_info = {
            .version = SENSOR_ISP_INFO_VERSION_DEFAULT,
            .pclk = 81666700,
            .vts = 1280,
            .hts = 960,
            .bayer_type = ESP_CAM_SENSOR_BAYER_BGGR,
        }
    },
};

static const esp_cam_sensor_format_t ov2640_format_info[] = {
    {
        .name = "DVP_8bit_20Minput_RGB565_640x480_6fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RGB565,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 640,
        .height = 480,
        .regs = init_reglist_DVP_8bit_RGB565_640x480_XCLK_20_6fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_RGB565_640x480_XCLK_20_6fps),
        .fps = 6,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_YUV422_640x480_6fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 640,
        .height = 480,
        .regs = init_reglist_DVP_8bit_YUV422_640x480_XCLK_20_6fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_YUV422_640x480_XCLK_20_6fps),
        .fps = 6,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_JPEG_640x480_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_JPEG,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 640,
        .height = 480,
        .regs = init_reglist_DVP_8bit_JPEG_640x480_XCLK_20_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_JPEG_640x480_XCLK_20_25fps),
        .fps = 25,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_RGB565_240x240_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RGB565,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 240,
        .height = 240,
        .regs = init_reglist_DVP_8bit_RGB565_240x240_XCLK_20_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_RGB565_240x240_XCLK_20_25fps),
        .fps = 25,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_YUV422_240x240_25fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_YUV422,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 240,
        .height = 240,
        .regs = init_reglist_DVP_8bit_YUV422_240x240_XCLK_20_25fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_YUV422_240x240_XCLK_20_25fps),
        .fps = 25,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_JPEG_320x240_50fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_JPEG,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 320,
        .height = 240,
        .regs = init_reglist_DVP_8bit_JPEG_320x240_XCLK_20_50fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_JPEG_320x240_XCLK_20_50fps),
        .fps = 50,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_JPEG_1280x720_12fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_JPEG,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 1280,
        .height = 720,
        .regs = init_reglist_DVP_8bit_JPEG_1280x720_XCLK_20_12fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_JPEG_1280x720_XCLK_20_12fps),
        .fps = 12,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_JPEG_1600x1200_12fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_JPEG,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 1600,
        .height = 1200,
        .regs = init_reglist_DVP_8bit_JPEG_1600x1200_XCLK_20_12fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_JPEG_1600x1200_XCLK_20_12fps),
        .fps = 12,
        .isp_info = NULL,
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        // Wrong format (deprecated)
        .name = "DVP_8bit_20Minput_RAW8_800x640_30fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 800,
        .height = 640,
        .regs = init_reglist_DVP_8bit_RAW8_1600x1200_XCLK_20M_15fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_RAW8_1600x1200_XCLK_20M_15fps),
        .fps = 30,
        .isp_info = &ov2640_isp_info[0],
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_RAW8_800x640_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 800,
        .height = 640,
        .regs = init_reglist_DVP_8bit_RAW8_1600x1200_XCLK_20M_15fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_RAW8_1600x1200_XCLK_20M_15fps),
        .fps = 15,
        .isp_info = &ov2640_isp_info[0],
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_RAW8_800x800_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 800,
        .height = 800,
        .regs = init_reglist_DVP_8bit_RAW8_1600x1200_XCLK_20M_15fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_RAW8_1600x1200_XCLK_20M_15fps),
        .fps = 15,
        .isp_info = &ov2640_isp_info[0],
        .mipi_info = {},
        .reserved = NULL,
    },
    {
        .name = "DVP_8bit_20Minput_RAW8_1024x600_15fps",
        .format = ESP_CAM_SENSOR_PIXFORMAT_RAW8,
        .port = ESP_CAM_SENSOR_DVP,
        .xclk = 20000000,
        .width = 1024,
        .height = 600,
        .regs = init_reglist_DVP_8bit_RAW8_1600x1200_XCLK_20M_15fps,
        .regs_size = ARRAY_SIZE(init_reglist_DVP_8bit_RAW8_1600x1200_XCLK_20M_15fps),
        .fps = 15,
        .isp_info = &ov2640_isp_info[0],
        .mipi_info = {},
        .reserved = NULL,
    },
};

static esp_err_t ov2640_set_bank(esp_sccb_io_handle_t sccb_handle, ov2640_bank_t bank)
{
    esp_err_t ret = ESP_OK;
    if (bank != s_reg_bank) {
        ret = esp_sccb_transmit_reg_a8v8(sccb_handle, BANK_SEL, bank);
        if (ret == ESP_OK) {
            s_reg_bank = bank;
        }
    }
    return ret;
}

static esp_err_t ov2640_read_reg(esp_sccb_io_handle_t sccb_handle, ov2640_bank_t bank, uint8_t reg, uint8_t *read_buf)
{
    esp_err_t ret = ov2640_set_bank(sccb_handle, bank);
    if (ret == ESP_OK) {
        ret = esp_sccb_transmit_receive_reg_a8v8(sccb_handle, reg, read_buf);
    }

    return ret;
}

static esp_err_t ov2640_write_reg(esp_sccb_io_handle_t sccb_handle, ov2640_bank_t bank, uint8_t reg, uint8_t value)
{
    esp_err_t ret = ov2640_set_bank(sccb_handle, bank);
    if (ret == ESP_OK) {
        ret = esp_sccb_transmit_reg_a8v8(sccb_handle, reg, value);
    }
    return ret;
}

static esp_err_t ov2640_write_array(esp_sccb_io_handle_t sccb_handle, const ov2640_reginfo_t *regs, size_t regs_size)
{
    int i = 0;
    esp_err_t ret = ESP_OK;
    while ((ret == ESP_OK) && i < regs_size) {
        if (regs[i].reg == BANK_SEL) {
            ret = ov2640_set_bank(sccb_handle, regs[i].val);
        } else if (regs[i].reg == REG_DELAY) {
            delay_ms(regs[i].val);
        } else {
            ret = esp_sccb_transmit_reg_a8v8(sccb_handle, regs[i].reg, regs[i].val);
        }
        i++;
    }
    ESP_LOGD(TAG, "write i=%d", i);
    return ret;
}

static esp_err_t ov2640_set_reg_bits(esp_sccb_io_handle_t sccb_handle, uint8_t bank, uint8_t reg, uint8_t offset, uint8_t mask, uint8_t value)
{
    esp_err_t ret = ESP_OK;
    uint8_t c_value, new_value;

    ret = ov2640_set_bank(sccb_handle, bank);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_sccb_transmit_receive_reg_a8v8(sccb_handle, reg, &c_value);
    if (ret != ESP_OK) {
        return ret;
    }
    new_value = (c_value & ~(mask << offset)) | ((value & mask) << offset);
    ret = esp_sccb_transmit_reg_a8v8(sccb_handle, reg, new_value);
    return ret;
}

static esp_err_t ov2640_write_reg_bits(esp_sccb_io_handle_t sccb_handle, uint8_t bank, uint8_t reg, uint8_t mask, int enable)
{
    return ov2640_set_reg_bits(sccb_handle, bank, reg, 0, mask, enable ? mask : 0);
}

#define WRITE_REG_OR_RETURN(bank, reg, val) ret = ov2640_write_reg(dev->sccb_handle, bank, reg, val); if(ret){return ret;}
#define SET_REG_BITS_OR_RETURN(bank, reg, offset, mask, val) ret = ov2640_set_reg_bits(dev->sccb_handle, bank, reg, offset, mask, val); if(ret){return ret;}

static esp_err_t ov2640_set_test_pattern(esp_cam_sensor_device_t *dev, int enable)
{
    return ov2640_write_reg_bits(dev->sccb_handle, BANK_SENSOR, COM7, COM7_COLOR_BAR, enable ? 1 : 0);
}

static esp_err_t ov2640_hw_reset(esp_cam_sensor_device_t *dev)
{
    if (dev->reset_pin >= 0) {
        gpio_set_level(dev->reset_pin, 0);
        delay_ms(10);
        gpio_set_level(dev->reset_pin, 1);
        delay_ms(10);
    }
    return ESP_OK;
}

static esp_err_t ov2640_soft_reset(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ov2640_write_reg_bits(dev->sccb_handle, BANK_SENSOR, COM7, COM7_SRST, 1);
    delay_ms(50);
    return ret;
}

static esp_err_t ov2640_get_sensor_id(esp_cam_sensor_device_t *dev, esp_cam_sensor_id_t *id)
{
    esp_err_t ret = ESP_FAIL;
    uint8_t pid = 0;
    if (ov2640_set_bank(dev->sccb_handle, BANK_SENSOR) == ESP_OK) {
        ov2640_read_reg(dev->sccb_handle, BANK_SENSOR, REG_PID, &pid);
    }
    if (OV2640_PID == pid) {
        id->pid = pid;
        ov2640_read_reg(dev->sccb_handle, BANK_SENSOR, REG_VER, &id->ver);
        ov2640_read_reg(dev->sccb_handle, BANK_SENSOR, REG_MIDL, &id->midl);
        ov2640_read_reg(dev->sccb_handle, BANK_SENSOR, REG_MIDH, &id->midh);
        ret = ESP_OK;
    }
    return ret;
}

static esp_err_t ov2640_set_stream(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_FAIL;
    if (dev->pwdn_pin >= 0) {
        ret = gpio_set_level(dev->pwdn_pin, enable ? 0 : 1);
    } else {
        ret = ov2640_write_reg(dev->sccb_handle, BANK_SENSOR, COM2, enable ? 0x02 : 0xe2);
        delay_ms(150);
    }

    if (ret == ESP_OK) {
        dev->stream_status = enable;
    }
    ESP_LOGD(TAG, "Stream=%d", enable);
    return ret;
}

static esp_err_t ov2640_set_hmirror(esp_cam_sensor_device_t *dev, int enable)
{
    return ov2640_write_reg_bits(dev->sccb_handle, BANK_SENSOR, REG04, REG04_HFLIP_IMG, enable ? 1 : 0);
}

static esp_err_t ov2640_set_vflip(esp_cam_sensor_device_t *dev, int enable)
{
    esp_err_t ret = ESP_OK;
    ret = ov2640_write_reg_bits(dev->sccb_handle, BANK_SENSOR, REG04, REG04_VREF_EN, enable ? 1 : 0);
    return ret & ov2640_write_reg_bits(dev->sccb_handle, BANK_SENSOR, REG04, REG04_VFLIP_IMG, enable ? 1 : 0);
}

static esp_err_t ov2640_set_jpeg_quality(esp_cam_sensor_device_t *dev, int quality)
{
    esp_err_t ret = ESP_FAIL;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    if (quality < 0) {
        quality = 0;
    } else if (quality > 63) {
        quality = 63;
    }
    ret = ov2640_write_reg(dev->sccb_handle, BANK_DSP, QS, quality);
    if (ret == ESP_OK) {
        cam_ov2640->jpeg_quality = quality;
    }
    return ret;
}

static esp_err_t ov2640_set_ae_level(esp_cam_sensor_device_t *dev, int level)
{
    esp_err_t ret = ESP_OK;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    level += 3;
    if (level <= 0 || level > NUM_AE_LEVELS) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < 3; i++) {
        WRITE_REG_OR_RETURN(BANK_SENSOR, ov2640_ae_levels_regs[0][i], ov2640_ae_levels_regs[level][i]);
    }
    cam_ov2640->ae_level = level - 3;
    return ret;
}

static esp_err_t ov2640_set_contrast(esp_cam_sensor_device_t *dev, int level)
{
    esp_err_t ret = ESP_OK;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    level += 3;
    if (level <= 0 || level > NUM_CONTRAST_LEVELS) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < 7; i++) {
        WRITE_REG_OR_RETURN(BANK_DSP, ov2640_contrast_regs[0][i], ov2640_contrast_regs[level][i]);
    }
    cam_ov2640->contrast = level - 3;
    return ret;
}

static esp_err_t ov2640_set_brightness(esp_cam_sensor_device_t *dev, int level)
{
    esp_err_t ret = ESP_OK;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    level += 3;
    if (level <= 0 || level > NUM_BRIGHTNESS_LEVELS) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < 5; i++) {
        WRITE_REG_OR_RETURN(BANK_DSP, ov2640_brightness_regs[0][i], ov2640_brightness_regs[level][i]);
    }
    cam_ov2640->brightness = level - 3;
    return ret;
}

static esp_err_t ov2640_set_saturation(esp_cam_sensor_device_t *dev, int level)
{
    esp_err_t ret = ESP_OK;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    level += 3;
    if (level <= 0 || level > NUM_SATURATION_LEVELS) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < 5; i++) {
        WRITE_REG_OR_RETURN(BANK_DSP, ov2640_saturation_regs[0][i], ov2640_saturation_regs[level][i]);
    }
    cam_ov2640->saturation = level - 3;
    return ret;
}

static esp_err_t ov2640_set_special_effect(esp_cam_sensor_device_t *dev, int effect)
{
    esp_err_t ret = ESP_OK;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    effect++;
    if (effect <= 0 || effect > NUM_SPECIAL_EFFECTS) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < 5; i++) {
        WRITE_REG_OR_RETURN(BANK_DSP, ov2640_special_effects_regs[0][i], ov2640_special_effects_regs[effect][i]);
    }
    cam_ov2640->special_effect = effect - 3;
    return ret;
}

static esp_err_t ov2640_set_wb_mode(esp_cam_sensor_device_t *dev, int mode)
{
    esp_err_t ret = ESP_OK;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    if (mode < 0 || mode > NUM_WB_MODES) {
        return ESP_ERR_INVALID_ARG;
    }
    SET_REG_BITS_OR_RETURN(BANK_DSP, 0xC7, 6, 1, mode ? 1 : 0);
    if (mode) {
        for (int i = 0; i < 3; i++) {
            WRITE_REG_OR_RETURN(BANK_DSP, ov2640_wb_modes_regs[0][i], ov2640_wb_modes_regs[mode][i]);
        }
    }
    cam_ov2640->wb_mode = mode;
    return ret;
}

static esp_err_t ov2640_set_outsize(esp_cam_sensor_device_t *dev, uint16_t width, uint16_t height)
{
    esp_err_t ret = ESP_OK;
    if (width % 4 || height % 4) {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t outw = width / 4;
    uint16_t outh = height / 4;
    uint8_t temp = 0;
    WRITE_REG_OR_RETURN(BANK_DSP, RESET, 0x04);
    WRITE_REG_OR_RETURN(BANK_DSP, 0x5A, outw & 0XFF); // LSB 8 bits of outw
    WRITE_REG_OR_RETURN(BANK_DSP, 0x5B, outh & 0XFF); // LSB 8 bits of outh
    temp = (outw >> 8) & 0X03;
    temp |= (outh >> 6) & 0X04;
    WRITE_REG_OR_RETURN(BANK_DSP, 0x5C, temp); // MSB of outw and outh
    WRITE_REG_OR_RETURN(BANK_DSP, RESET, 0x00);
    return ret;
}

static esp_err_t ov2640_query_para_desc(esp_cam_sensor_device_t *dev, esp_cam_sensor_param_desc_t *qdesc)
{
    esp_err_t ret = ESP_OK;

    switch (qdesc->id) {
    case ESP_CAM_SENSOR_JPEG_QUALITY:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 1;
        qdesc->number.maximum = 63;
        qdesc->number.step = 1;
        qdesc->default_value = OV2640_JPEG_QUALITY_DEFAULT;
        break;
    case ESP_CAM_SENSOR_AE_LEVEL:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = -2;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_CONTRAST:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = -2;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_BRIGHTNESS:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = -2;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_SATURATION:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = -2;
        qdesc->number.maximum = 1;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_SPECIAL_EFFECT:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 0;
        qdesc->number.maximum = 6;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
        break;
    case ESP_CAM_SENSOR_WB:
        qdesc->type = ESP_CAM_SENSOR_PARAM_TYPE_NUMBER;
        qdesc->number.minimum = 1;
        qdesc->number.maximum = 4;
        qdesc->number.step = 1;
        qdesc->default_value = 0;
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

static esp_err_t ov2640_get_para_value(esp_cam_sensor_device_t *dev, uint32_t id, void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    struct ov2640_cam *cam_ov2640 = (struct ov2640_cam *)dev->priv;
    // The parameter types of this sensor are all 4 bytes.
    ESP_RETURN_ON_FALSE(size == 4, ESP_ERR_INVALID_ARG, TAG, "Para size err");

    switch (id) {
    case ESP_CAM_SENSOR_JPEG_QUALITY: {
        *(int32_t *)arg = cam_ov2640->jpeg_quality;
    }
    break;
    case ESP_CAM_SENSOR_AE_LEVEL: {
        *(int32_t *)arg = cam_ov2640->ae_level;
    }
    break;
    case ESP_CAM_SENSOR_CONTRAST: {
        *(int32_t *)arg = cam_ov2640->contrast;
    }
    break;
    case ESP_CAM_SENSOR_BRIGHTNESS: {
        *(int32_t *)arg = cam_ov2640->brightness;
    }
    break;
    case ESP_CAM_SENSOR_SATURATION: {
        *(int32_t *)arg = cam_ov2640->saturation;
    }
    break;
    case ESP_CAM_SENSOR_SPECIAL_EFFECT: {
        *(int32_t *)arg = cam_ov2640->special_effect;
    }
    break;
    case ESP_CAM_SENSOR_WB: {
        *(int32_t *)arg = cam_ov2640->wb_mode;
    }
    break;
    default: {
        ret = ESP_ERR_NOT_SUPPORTED;
        break;
    }
    }
    return ret;
}

static esp_err_t ov2640_set_para_value(esp_cam_sensor_device_t *dev, uint32_t id, const void *arg, size_t size)
{
    esp_err_t ret = ESP_OK;
    int32_t value = *(int32_t *)arg;
    // The parameter types of this sensor are all 4 bytes.
    ESP_RETURN_ON_FALSE(size == 4, ESP_ERR_INVALID_ARG, TAG, "Para size err");

    switch (id) {
    case ESP_CAM_SENSOR_VFLIP: {
        ret = ov2640_set_vflip(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_HMIRROR: {
        ret = ov2640_set_hmirror(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_JPEG_QUALITY: {
        if (dev->cur_format->format == ESP_CAM_SENSOR_PIXFORMAT_JPEG) {
            ret = ov2640_set_jpeg_quality(dev, value);
        } else {
            ret = ESP_ERR_INVALID_STATE;
        }
        break;
    }
    case ESP_CAM_SENSOR_AE_LEVEL: {
        ret = ov2640_set_ae_level(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_CONTRAST: {
        ret = ov2640_set_contrast(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_BRIGHTNESS: {
        ret = ov2640_set_brightness(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_SATURATION: {
        ret = ov2640_set_saturation(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_SPECIAL_EFFECT: {
        ret = ov2640_set_special_effect(dev, value);
        break;
    }
    case ESP_CAM_SENSOR_WB: {
        ret = ov2640_set_wb_mode(dev, value);
        break;
    }
    default: {
        ESP_LOGE(TAG, "set id=%"PRIx32" is not supported", id);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    }

    return ret;
}

static esp_err_t ov2640_query_support_formats(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_array_t *formats)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, formats);

    formats->count = ARRAY_SIZE(ov2640_format_info);
    formats->format_array = &ov2640_format_info[0];
    return ESP_OK;
}

static esp_err_t ov2640_query_support_capability(esp_cam_sensor_device_t *dev, esp_cam_sensor_capability_t *sensor_cap)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, sensor_cap);

    sensor_cap->fmt_yuv = 1;
    sensor_cap->fmt_rgb565 = 1;
    sensor_cap->fmt_jpeg = 1;
    return ESP_OK;
}

static esp_err_t ov2640_set_format(esp_cam_sensor_device_t *dev, const esp_cam_sensor_format_t *format)
{
    ESP_CAM_SENSOR_NULL_POINTER_CHECK(TAG, dev);

    esp_err_t ret = ESP_FAIL;
    /* Depending on the interface type, an available configuration is automatically loaded.
    You can set the output format of the sensor without using query_format().*/
    if (format == NULL) {
        if (dev->sensor_port == ESP_CAM_SENSOR_DVP) {
            format = &ov2640_format_info[CONFIG_CAMERA_OV2640_DVP_IF_FORMAT_INDEX_DAFAULT];
        } else {
            return ret;
        }
    }
    if (!strcmp(format->name, "DVP_8bit_20Minput_RAW8_800x640_30fps")) {
        ESP_LOGW(TAG, "this format is deprecated, please use 'DVP_8bit_20Minput_RAW8_800x640_15fps' instead");
    }
    // write common reg list
    ret = ov2640_write_array(dev->sccb_handle, ov2640_settings_cif, ARRAY_SIZE(ov2640_settings_cif));
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ret, TAG, "Common reg list write failed");
    // write format related regs
    ret = ov2640_write_array(dev->sccb_handle, (ov2640_reginfo_t *)format->regs, format->regs_size);
    if (ret == ESP_OK) {
        ret = ov2640_set_outsize(dev, format->width, format->height);
    }
    ESP_RETURN_ON_FALSE(ret == ESP_OK, ESP_CAM_SENSOR_ERR_FAILED_SET_FORMAT, TAG, "format reg list write failed");

    dev->cur_format = format;

    return ret;
}

static esp_err_t ov2640_get_format(esp_cam_sensor_device_t *dev, esp_cam_sensor_format_t *format)
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

static esp_err_t ov2640_priv_ioctl(esp_cam_sensor_device_t *dev, uint32_t cmd, void *arg)
{
    esp_err_t ret = ESP_OK;
    uint8_t regval;
    esp_cam_sensor_reg_val_t *sensor_reg;
    OV2640_IO_MUX_LOCK(mux);

    switch (cmd) {
    case ESP_CAM_SENSOR_IOC_HW_RESET:
        ret = ov2640_hw_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_SW_RESET:
        ret = ov2640_soft_reset(dev);
        break;
    case ESP_CAM_SENSOR_IOC_S_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = esp_sccb_transmit_reg_a8v8(dev->sccb_handle, sensor_reg->regaddr, sensor_reg->value);
        break;
    case ESP_CAM_SENSOR_IOC_S_STREAM:
        ret = ov2640_set_stream(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_S_TEST_PATTERN:
        ret = ov2640_set_test_pattern(dev, *(int *)arg);
        break;
    case ESP_CAM_SENSOR_IOC_G_REG:
        sensor_reg = (esp_cam_sensor_reg_val_t *)arg;
        ret = esp_sccb_transmit_receive_reg_a8v8(dev->sccb_handle, sensor_reg->regaddr, &regval);
        if (ret == ESP_OK) {
            sensor_reg->value = regval;
        }
        break;
    case ESP_CAM_SENSOR_IOC_G_CHIP_ID:
        ret = ov2640_get_sensor_id(dev, arg);
        break;
    default:
        ESP_LOGE(TAG, "cmd=%"PRIx32" is not supported", cmd);
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    OV2640_IO_MUX_UNLOCK(mux);
    return ret;
}

static esp_err_t ov2640_power_on(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        OV2640_ENABLE_OUT_CLOCK(dev->xclk_pin, dev->xclk_freq_hz);
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

static esp_err_t ov2640_power_off(esp_cam_sensor_device_t *dev)
{
    esp_err_t ret = ESP_OK;

    if (dev->xclk_pin >= 0) {
        OV2640_DISABLE_OUT_CLOCK(dev->xclk_pin);
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

static esp_err_t ov2640_delete(esp_cam_sensor_device_t *dev)
{
    ESP_LOGD(TAG, "del ov2640 (%p)", dev);
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

static esp_cam_sensor_ops_t ov2640_ops = {
    .query_para_desc = ov2640_query_para_desc,
    .get_para_value = ov2640_get_para_value,
    .set_para_value = ov2640_set_para_value,
    .query_support_formats = ov2640_query_support_formats,
    .query_support_capability = ov2640_query_support_capability,
    .set_format = ov2640_set_format,
    .get_format = ov2640_get_format,
    .priv_ioctl = ov2640_priv_ioctl,
    .del = ov2640_delete
};

esp_cam_sensor_device_t *ov2640_detect(esp_cam_sensor_config_t *config)
{
    esp_cam_sensor_device_t *dev = NULL;
    struct ov2640_cam *cam_ov2640;

    if (config == NULL) {
        return NULL;
    }

    dev = heap_caps_calloc(1, sizeof(esp_cam_sensor_device_t), MALLOC_CAP_DEFAULT);
    if (dev == NULL) {
        ESP_LOGE(TAG, "No memory for camera");
        return NULL;
    }

    cam_ov2640 = heap_caps_calloc(1, sizeof(struct ov2640_cam), MALLOC_CAP_DEFAULT);
    if (!cam_ov2640) {
        ESP_LOGE(TAG, "failed to calloc cam");
        free(dev);
        return NULL;
    }

    dev->name = (char *)OV2640_SENSOR_NAME;
    dev->ops = &ov2640_ops;
    dev->sccb_handle = config->sccb_handle;
    dev->xclk_pin = config->xclk_pin;
    dev->reset_pin = config->reset_pin;
    dev->pwdn_pin = config->pwdn_pin;
    dev->priv = cam_ov2640;
    if (config->sensor_port == ESP_CAM_SENSOR_DVP) {
        dev->cur_format = &ov2640_format_info[CONFIG_CAMERA_OV2640_DVP_IF_FORMAT_INDEX_DAFAULT];
    } else {
        ESP_LOGE(TAG, "Not support MIPI port");
    }

    // Configure sensor power, clock
    if (ov2640_power_on(dev) != ESP_OK) {
        ESP_LOGE(TAG, "power on failed");
        goto err_free_handler;
    }

    if (ov2640_get_sensor_id(dev, &dev->id) != ESP_OK) {
        ESP_LOGE(TAG, "get sensor ID failed");
        goto err_free_handler;
    } else if (dev->id.pid != OV2640_PID) {
        ESP_LOGE(TAG, "sensor is not OV2640, PID=0x%x", dev->id.pid);
        goto err_free_handler;
    }
    ESP_LOGI(TAG, "Detected Camera sensor PID=0x%x", dev->id.pid);

    return dev;

err_free_handler:
    ov2640_power_off(dev);
    free(dev->priv);
    free(dev);
    return NULL;
}

#if CONFIG_CAMERA_OV2640_AUTO_DETECT_DVP_INTERFACE_SENSOR
ESP_CAM_SENSOR_DETECT_FN(ov2640_detect, ESP_CAM_SENSOR_DVP, OV2640_SCCB_ADDR)
{
    ((esp_cam_sensor_config_t *)config)->sensor_port = ESP_CAM_SENSOR_DVP;
    return ov2640_detect(config);
}
#endif
