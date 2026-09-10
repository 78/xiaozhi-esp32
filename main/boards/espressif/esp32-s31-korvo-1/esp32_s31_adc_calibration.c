/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 移植自官方 esp-dev-kits ESP32-S31-Korvo-1 factory demo BSP
 * （examples/common_components/esp32_s31_korvo）
 * ESP32-S31 SAR ADC 软件校准：通过内部校准寄存器计算各位权重，
 * 使无硬件校准方案的 S31 也能得到准确的电压读数。
 */

#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include "bsp_adc_calibration.h"
#include "esp_check.h"
#include "esp_log.h"
#include "hal/adc_types.h"
#include "soc/soc_caps.h"

static const char *TAG = "S31-ADC-CALI";

#define BSP_S31_ADC_SAR1_BASE              (0x10)
#define BSP_S31_ADC_SAR2_BASE              (0x11)
#define BSP_S31_ADC_SAR_HOST_ID            (0)

#define BSP_S31_ADC_REG_CAL_CTRL0          (0x0)
#define BSP_S31_ADC_REG_CAL_SEL            (0x2)
#define BSP_S31_ADC_REG_CAL_SEL_1          (0x3)
#define BSP_S31_ADC_REG_RAW_CTRL           (0x4)

#define BSP_S31_ADC_CAL_DONE_MSB           (0)
#define BSP_S31_ADC_CAL_DONE_LSB           (0)
#define BSP_S31_ADC_POL_SEL_MSB            (1)
#define BSP_S31_ADC_POL_SEL_LSB            (1)
#define BSP_S31_ADC_EN_RAW_DATA_MSB        (4)
#define BSP_S31_ADC_EN_RAW_DATA_LSB        (4)

#define BSP_S31_ADC_SAMPLE_COUNT           (256)
#define BSP_S31_ADC_CAL_CYCLES             (5)
#define BSP_S31_ADC_WEIGHT_Q               (8)
#define BSP_S31_ADC_WEIGHT_SCALE           (1 << BSP_S31_ADC_WEIGHT_Q)
#define BSP_S31_ADC_MAX_MV                 (2000)

typedef struct {
    uint8_t bit;
    uint8_t cal_sel;
    uint8_t cal_sel_1;
} bsp_s31_adc_cal_config_t;

enum {
    BSP_S31_ADC_BIT_COUNT = 17,
    BSP_S31_ADC_CAL_CONFIG_COUNT = 7,
    BSP_S31_ADC_POL_COUNT = 2,
};

typedef struct {
    bool calibrated;
    int32_t weights_q[BSP_S31_ADC_BIT_COUNT];
} bsp_s31_adc_calibration_t;

void regi2c_ctrl_write_reg_mask(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t msb, uint8_t lsb, uint8_t data);

static const bsp_s31_adc_cal_config_t s_cal_config[BSP_S31_ADC_CAL_CONFIG_COUNT] = {
    {10, 0b00000010, 0b00000001},
    {11, 0b00000100, 0b00000011},
    {12, 0b00001000, 0b00000111},
    {13, 0b00010000, 0b00001111},
    {14, 0b00100000, 0b00011111},
    {15, 0b01000000, 0b01111111},
    {16, 0b10000000, 0b01111111},
};

static const int32_t s_ideal_weights[BSP_S31_ADC_BIT_COUNT] = {
    2048, 1024, 512, 256, 256, 128, 64, 32, 32, 16, 8, 8, 4, 2, 2, 0, 1,
};

static bsp_s31_adc_calibration_t s_adc_calibration[SOC_ADC_PERIPH_NUM];

static uint8_t bsp_s31_adc_get_sar_base(adc_unit_t unit)
{
    return (unit == ADC_UNIT_1) ? BSP_S31_ADC_SAR1_BASE : BSP_S31_ADC_SAR2_BASE;
}

static void bsp_s31_adc_write_mask(adc_unit_t unit, uint8_t reg, uint8_t msb, uint8_t lsb, uint8_t value)
{
    regi2c_ctrl_write_reg_mask(bsp_s31_adc_get_sar_base(unit), BSP_S31_ADC_SAR_HOST_ID, reg, msb, lsb, value);
}

static void bsp_s31_adc_set_cal_done(adc_unit_t unit, bool done)
{
    bsp_s31_adc_write_mask(unit, BSP_S31_ADC_REG_CAL_CTRL0,
                           BSP_S31_ADC_CAL_DONE_MSB, BSP_S31_ADC_CAL_DONE_LSB, done ? 1 : 0);
}

static void bsp_s31_adc_set_raw_data(adc_unit_t unit, bool enable)
{
    bsp_s31_adc_write_mask(unit, BSP_S31_ADC_REG_RAW_CTRL,
                           BSP_S31_ADC_EN_RAW_DATA_MSB, BSP_S31_ADC_EN_RAW_DATA_LSB, enable ? 1 : 0);
}

static void bsp_s31_adc_set_pol(adc_unit_t unit, bool pol)
{
    bsp_s31_adc_write_mask(unit, BSP_S31_ADC_REG_CAL_CTRL0,
                           BSP_S31_ADC_POL_SEL_MSB, BSP_S31_ADC_POL_SEL_LSB, pol ? 1 : 0);
}

static void bsp_s31_adc_set_cal_config(adc_unit_t unit, const bsp_s31_adc_cal_config_t *config)
{
    regi2c_ctrl_write_reg_mask(bsp_s31_adc_get_sar_base(unit), BSP_S31_ADC_SAR_HOST_ID,
                               BSP_S31_ADC_REG_CAL_SEL, 7, 0, config->cal_sel);
    regi2c_ctrl_write_reg_mask(bsp_s31_adc_get_sar_base(unit), BSP_S31_ADC_SAR_HOST_ID,
                               BSP_S31_ADC_REG_CAL_SEL_1, 7, 0, config->cal_sel_1);
}

static void bsp_s31_adc_reset_weights(bsp_s31_adc_calibration_t *calibration)
{
    for (int i = 0; i < BSP_S31_ADC_BIT_COUNT; i++) {
        calibration->weights_q[i] = s_ideal_weights[i] * BSP_S31_ADC_WEIGHT_SCALE;
    }
}

static int32_t bsp_s31_adc_calc_code_q(const int32_t weights_q[BSP_S31_ADC_BIT_COUNT], uint32_t raw)
{
    int32_t code_q = 0;

    raw &= 0x1FFFF;
    for (int i = 0; i < BSP_S31_ADC_BIT_COUNT; i++) {
        if (raw & (1U << (BSP_S31_ADC_BIT_COUNT - 1 - i))) {
            code_q += weights_q[i];
        }
    }

    return code_q;
}

static esp_err_t bsp_s31_adc_collect_raw(adc_oneshot_unit_handle_t handle,
                                         adc_channel_t channel,
                                         uint32_t raw_samples[BSP_S31_ADC_CAL_CONFIG_COUNT][BSP_S31_ADC_POL_COUNT][BSP_S31_ADC_SAMPLE_COUNT])
{
    int raw = 0;

    for (int config_idx = 0; config_idx < BSP_S31_ADC_CAL_CONFIG_COUNT; config_idx++) {
        const bsp_s31_adc_cal_config_t *config = &s_cal_config[config_idx];
        bsp_s31_adc_set_cal_config(ADC_UNIT_1, config);

        for (int pol = 0; pol < BSP_S31_ADC_POL_COUNT; pol++) {
            bsp_s31_adc_set_pol(ADC_UNIT_1, pol == 0);
            for (int sample = 0; sample < BSP_S31_ADC_SAMPLE_COUNT; sample++) {
                ESP_RETURN_ON_ERROR(adc_oneshot_read(handle, channel, &raw),
                                    TAG, "read ADC raw data during calibration failed");
                raw_samples[config_idx][pol][sample] = raw & 0x1FFFF;
            }
        }
    }

    return ESP_OK;
}

esp_err_t bsp_s31_adc_calibration_init(adc_oneshot_unit_handle_t handle,
                                       adc_unit_t unit,
                                       adc_channel_t channel)
{
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "ADC handle is null");
    ESP_RETURN_ON_FALSE(unit == ADC_UNIT_1, ESP_ERR_NOT_SUPPORTED, TAG, "only ADC1 calibration is used by this BSP");

    bsp_s31_adc_calibration_t *calibration = &s_adc_calibration[unit];
    if (calibration->calibrated) {
        bsp_s31_adc_set_raw_data(unit, true);
        bsp_s31_adc_set_cal_done(unit, true);
        return ESP_OK;
    }

    uint32_t (*raw_samples)[BSP_S31_ADC_POL_COUNT][BSP_S31_ADC_SAMPLE_COUNT] =
        calloc(BSP_S31_ADC_CAL_CONFIG_COUNT, sizeof(*raw_samples));
    ESP_RETURN_ON_FALSE(raw_samples, ESP_ERR_NO_MEM, TAG, "allocate ADC calibration buffer failed");

    bsp_s31_adc_reset_weights(calibration);
    bsp_s31_adc_set_raw_data(unit, true);
    bsp_s31_adc_set_cal_done(unit, false);

    esp_err_t ret = bsp_s31_adc_collect_raw(handle, channel, raw_samples);
    bsp_s31_adc_set_cal_done(unit, true);
    if (ret != ESP_OK) {
        free(raw_samples);
        return ret;
    }

    for (int cycle = 0; cycle < BSP_S31_ADC_CAL_CYCLES; cycle++) {
        for (int config_idx = 0; config_idx < BSP_S31_ADC_CAL_CONFIG_COUNT; config_idx++) {
            int64_t code_sum[2] = {};

            for (int pol = 0; pol < BSP_S31_ADC_POL_COUNT; pol++) {
                for (int sample = 0; sample < BSP_S31_ADC_SAMPLE_COUNT; sample++) {
                    code_sum[pol] += bsp_s31_adc_calc_code_q(calibration->weights_q,
                                                             raw_samples[config_idx][pol][sample]);
                }
            }

            int32_t avg_pol1_q = code_sum[0] / BSP_S31_ADC_SAMPLE_COUNT;
            int32_t avg_pol0_q = code_sum[1] / BSP_S31_ADC_SAMPLE_COUNT;
            int32_t delta_q = (avg_pol1_q - avg_pol0_q) / 2;
            int weight_idx = BSP_S31_ADC_BIT_COUNT - 1 - s_cal_config[config_idx].bit;
            calibration->weights_q[weight_idx] += delta_q;
        }
    }

    calibration->calibrated = true;
    free(raw_samples);

    ESP_LOGI(TAG, "Button ADC software calibration finished: w0_q=%" PRId32 ", w6_q=%" PRId32 ", w16_q=%" PRId32,
             calibration->weights_q[0], calibration->weights_q[6], calibration->weights_q[16]);
    return ESP_OK;
}

esp_err_t bsp_s31_adc_calibration_raw_to_mv(adc_unit_t unit, int raw, int *voltage_mv)
{
    ESP_RETURN_ON_FALSE(voltage_mv, ESP_ERR_INVALID_ARG, TAG, "voltage output is null");
    ESP_RETURN_ON_FALSE(unit == ADC_UNIT_1, ESP_ERR_NOT_SUPPORTED, TAG, "only ADC1 calibration is used by this BSP");

    bsp_s31_adc_calibration_t *calibration = &s_adc_calibration[unit];
    ESP_RETURN_ON_FALSE(calibration->calibrated, ESP_ERR_INVALID_STATE, TAG, "ADC calibration is not ready");

    int32_t code_q = bsp_s31_adc_calc_code_q(calibration->weights_q, raw);
    int64_t mv = BSP_S31_ADC_MAX_MV - ((int64_t)4000 * code_q) / (4393 * BSP_S31_ADC_WEIGHT_SCALE);

    if (mv < 0) {
        mv = 0;
    } else if (mv > BSP_S31_ADC_MAX_MV) {
        mv = BSP_S31_ADC_MAX_MV;
    }

    *voltage_mv = (int)mv;
    return ESP_OK;
}
