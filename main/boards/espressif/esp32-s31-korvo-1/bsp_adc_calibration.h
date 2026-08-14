/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 移植自官方 esp-dev-kits ESP32-S31-Korvo-1 factory demo BSP
 * （examples/common_components/esp32_s31_korvo）
 */

#pragma once

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t bsp_s31_adc_calibration_init(adc_oneshot_unit_handle_t handle,
                                       adc_unit_t unit,
                                       adc_channel_t channel);

esp_err_t bsp_s31_adc_calibration_raw_to_mv(adc_unit_t unit, int raw, int *voltage_mv);

#ifdef __cplusplus
}
#endif
