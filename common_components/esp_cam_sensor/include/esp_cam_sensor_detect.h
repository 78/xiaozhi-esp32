/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_cam_sensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Define a camera detect function which can be executed automatically, in application layer.
 *
 * @param f  function name (identifier)
 * @param i  interface which is used to communicate with the camera
 * @param (varargs)  optional, additional attributes for the function declaration (such as IRAM_ATTR)
 *
 * The function defined using this macro must return esp_cam_sensor_device_t on success. Any other value will be
 * logged and the automatic, process in application layer should be abort.
 *
 * There should be at lease one undefined symble to be added in the camera driver in order to avoid
 * the optimization of the linker. Because otherwise the linker will ignore camera driver as it has
 * no other files depending on any symbols in it.
 *
 * Some thing like this should be added in the CMakeLists.txt of the camera driver:
 *  target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ov2640_detect")
 */
#define ESP_CAM_SENSOR_DETECT_FN(f, i, j, ...) \
    static esp_cam_sensor_device_t * __VA_ARGS__ __esp_cam_sensor_detect_fn_##f(void *config); \
    static __attribute__((used)) _SECTION_ATTR_IMPL(".esp_cam_sensor_detect_fn", __COUNTER__) \
        esp_cam_sensor_detect_fn_t esp_cam_sensor_detect_fn_##f = { .detect = ( __esp_cam_sensor_detect_fn_##f), .port = (i), .sccb_addr = (j) }; \
    static esp_cam_sensor_device_t *__esp_cam_sensor_detect_fn_##f(void *config)

/**
 * @brief camera sensor auto detect function array start.
 */
extern esp_cam_sensor_detect_fn_t __esp_cam_sensor_detect_fn_array_start;

/**
 * @brief camera sensor auto detect function array end.
 */
extern esp_cam_sensor_detect_fn_t __esp_cam_sensor_detect_fn_array_end;

#ifdef __cplusplus
}
#endif
