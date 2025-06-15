/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"

#include "esp_cam_sensor_xclk.h"

/**
 * @brief xclk generator controller type
 */
typedef struct esp_cam_sensor_xclk esp_cam_sensor_xclk_t;

/**
 * @brief xclk generator controller type
 */
struct esp_cam_sensor_xclk {
    esp_cam_sensor_xclk_source_t cam_xclk_source;
    /**
     * @brief Start xclk output
     *
     * @param[in] handle xclk generator handle
     * @param[in] config Configuration data
     * @return
     *        - ESP_OK: Success.
     */
    esp_err_t (*start)(esp_cam_sensor_xclk_t *xclk_handle, const esp_cam_sensor_xclk_config_t *config);
    /**
     * @brief Stop xclk output
     *
     * @param[in] handle xclk generator handle
     * @return
     *        - ESP_OK: Success.
     */
    esp_err_t (*stop)(esp_cam_sensor_xclk_t *xclk_handle);
    /**
     * @brief Free xclk handle memory
     *
     * @param[in] handle xclk generator handle
     * @return
     *        - ESP_OK: Success.
     */
    esp_err_t (*free)(esp_cam_sensor_xclk_t *xclk_handle);
};

#if CONFIG_CAMERA_XCLK_USE_LEDC
/**
 * @brief XCLK generator instance type implemented by LEDC
 */
typedef struct xclk_generator_ledc_t {
    ledc_timer_t ledc_channel;
    esp_cam_sensor_xclk_t base;
} xclk_generator_ledc_t;

#define NO_CAMERA_LEDC_CHANNEL 0xFF
/*Note that when setting the resolution to 1, we can only choose to divide the frequency by 1 times from CLK*/
#define XCLK_LEDC_DUTY_RES_DEFAULT  LEDC_TIMER_1_BIT
#endif

#if CONFIG_CAMERA_XCLK_USE_ESP_CLOCK_ROUTER
/**
 * @brief XCLK generator instance type implemented by ESP SoC clock
 */
typedef struct xclk_generator_soc_clock_t {
    esp_clock_output_mapping_handle_t clkout_mapping_ret_hdl; /*Clock output control handler*/
    esp_cam_sensor_xclk_t base;
} xclk_generator_soc_clock_t;

#define XCLK_SOC_CLOCK_SOURCE_DEFAULT_HZ 480000000
#endif

#define XCLK_MEM_CAPS_DEFAULT          MALLOC_CAP_DEFAULT

static const char *TAG = "xclk";

#if CONFIG_CAMERA_XCLK_USE_LEDC
static esp_err_t xclk_timer_conf(ledc_timer_t ledc_timer, ledc_clk_cfg_t clk_cfg, int xclk_freq_hz)
{
    esp_err_t err;
    ledc_timer_config_t timer_conf = {0};
    timer_conf.duty_resolution = XCLK_LEDC_DUTY_RES_DEFAULT;
    timer_conf.freq_hz = xclk_freq_hz;
    timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    timer_conf.clk_cfg = clk_cfg;
    timer_conf.timer_num = (ledc_timer_t)ledc_timer;
    err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed for freq %d, rc=%x", xclk_freq_hz, err);
    }
    return err;
}

/**
 * @brief Stop xclk output, and set idle level
 *
 * @param  ledc_channel LEDC channel (0-7), select from ledc_channel_t
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
static esp_err_t xclk_ledc_generator_free(esp_cam_sensor_xclk_t *xclk_handle)
{
    xclk_generator_ledc_t *xclk_ledc = __containerof(xclk_handle, xclk_generator_ledc_t, base);
    heap_caps_free(xclk_ledc);
    return ESP_OK;
}

/**
 * @brief Stop xclk output, and set idle level
 *
 * @param  ledc_channel LEDC channel (0-7), select from ledc_channel_t
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 */
static esp_err_t xclk_ledc_generator_stop(esp_cam_sensor_xclk_t *xclk_handle)
{
    esp_err_t ret = ESP_OK;
    xclk_generator_ledc_t *xclk_ledc = __containerof(xclk_handle, xclk_generator_ledc_t, base);

    if (xclk_ledc->ledc_channel != NO_CAMERA_LEDC_CHANNEL) {
        ret = ledc_stop(LEDC_LOW_SPEED_MODE, xclk_ledc->ledc_channel, 0);
    }
    ESP_LOGD(TAG, "xclk ledc stop");
    return ret;
}

/**
 * @brief Configure LEDC timer&channel for generating XCLK
 *        Configure LEDC timer with the given source timer/frequency(Hz)
 *
 * @param[in]  xclk_handle xclk handle
 * @param[in]  config      LEDC channel configuration
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *     - ESP_FAIL Can not find a proper pre-divider number base on the given frequency and the current duty_resolution.
 */
esp_err_t xclk_ledc_generator_start(esp_cam_sensor_xclk_t *xclk_handle, const esp_cam_sensor_xclk_config_t *config)
{
    ESP_RETURN_ON_FALSE(xclk_handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    esp_err_t err = xclk_timer_conf(config->ledc_cfg.timer, config->ledc_cfg.clk_cfg, config->ledc_cfg.xclk_freq_hz);
    xclk_generator_ledc_t *xclk_ledc = __containerof(xclk_handle, xclk_generator_ledc_t, base);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed, rc=%x", err);
        return err;
    }

    ledc_channel_config_t ch_conf = {0};
    ch_conf.gpio_num = config->ledc_cfg.xclk_pin;
    ch_conf.speed_mode = LEDC_LOW_SPEED_MODE;
    ch_conf.channel = config->ledc_cfg.channel;
    ch_conf.intr_type = LEDC_INTR_DISABLE;
    ch_conf.timer_sel = config->ledc_cfg.timer;
    ch_conf.duty = 1;
    ch_conf.hpoint = 0;
    err = ledc_channel_config(&ch_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed, rc=%x", err);
        return err;
    }

    xclk_ledc->ledc_channel = config->ledc_cfg.channel;

    ESP_LOGD(TAG, "xclk ledc start: %p", xclk_ledc);
    return ESP_OK;
}
#endif

#if CONFIG_CAMERA_XCLK_USE_ESP_CLOCK_ROUTER
static esp_err_t xclk_soc_clock_generator_free(esp_cam_sensor_xclk_t *xclk_handle)
{
    xclk_generator_soc_clock_t *xclk_soc_clock = __containerof(xclk_handle, xclk_generator_soc_clock_t, base);
    heap_caps_free(xclk_soc_clock);
    return ESP_OK;
}

static esp_err_t xclk_soc_clock_generator_stop(esp_cam_sensor_xclk_t *xclk_handle)
{
    esp_err_t ret = ESP_OK;
    xclk_generator_soc_clock_t *xclk_soc_clock = __containerof(xclk_handle, xclk_generator_soc_clock_t, base);

    ret = esp_clock_output_stop(xclk_soc_clock->clkout_mapping_ret_hdl);
    ESP_LOGD(TAG, "clock router stop");
    return ret;
}

/**
 * @brief Configure clock signal source for generating XCLK
 *
 * @param[in]   xclk_handle XCLK control handle
 * @param[in]   config SOC clock router configuration
 * @return
 *      - ESP_OK: Output specified clock signal to specified GPIO successfully
 *      - ESP_ERR_INVALID_ARG: Specified GPIO not supported to output internal clock
 *                             or specified GPIO is already mapped to other internal clock source.
 *      - ESP_FAIL: There are no clock out signals that can be allocated.
 */
esp_err_t xclk_soc_clock_generator_start(esp_cam_sensor_xclk_t *xclk_handle, const esp_cam_sensor_xclk_config_t *config)
{
    uint32_t div_num; // clock frequency division value, should be in the range of 1 ~ 256
    esp_err_t err;
    const uint32_t src_clock_default = XCLK_SOC_CLOCK_SOURCE_DEFAULT_HZ; // default is CLKOUT_SIG_SPLL(480M)
    esp_clock_output_mapping_handle_t clkout_mapping_hdl;
    xclk_generator_soc_clock_t *xclk_soc_clock = __containerof(xclk_handle, xclk_generator_soc_clock_t, base);
    ESP_RETURN_ON_FALSE(((src_clock_default % config->esp_clock_router_cfg.xclk_freq_hz) == 0), ESP_FAIL, TAG, "Clock cannot be divided");

    div_num = src_clock_default / config->esp_clock_router_cfg.xclk_freq_hz;
    err = esp_clock_output_start(CLKOUT_SIG_SPLL, config->esp_clock_router_cfg.xclk_pin, &clkout_mapping_hdl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "clkout config failed, rc=%x", err);
        return err;
    }

    err = esp_clock_output_set_divider(clkout_mapping_hdl, div_num);
    if (err != ESP_OK) {
        esp_clock_output_stop(clkout_mapping_hdl);
        ESP_LOGE(TAG, "clkout set divider failed, rc=%x", err);
        return err;
    }
    xclk_soc_clock->clkout_mapping_ret_hdl = clkout_mapping_hdl;

    ESP_LOGD(TAG, "xclk soc clock router start: %p", xclk_soc_clock);

    return err;
}
#endif

esp_err_t esp_cam_sensor_xclk_allocate(esp_cam_sensor_xclk_source_t source, esp_cam_sensor_xclk_handle_t *ret_handle)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(ret_handle, ESP_ERR_INVALID_ARG, TAG, "handle address cannot be 0");
    switch (source) {
#if CONFIG_CAMERA_XCLK_USE_LEDC
    case ESP_CAM_SENSOR_XCLK_LEDC:
        xclk_generator_ledc_t *xclk_ledc = heap_caps_calloc(1, sizeof(xclk_generator_ledc_t), XCLK_MEM_CAPS_DEFAULT);
        ESP_RETURN_ON_FALSE(xclk_ledc, ESP_ERR_NO_MEM, TAG, "no mem for xclk handle");
        xclk_ledc->base.cam_xclk_source = ESP_CAM_SENSOR_XCLK_LEDC;
        xclk_ledc->base.start = xclk_ledc_generator_start;
        xclk_ledc->base.stop = xclk_ledc_generator_stop;
        xclk_ledc->base.free = xclk_ledc_generator_free;
        *ret_handle = &(xclk_ledc->base);
        break;
#endif
#if CONFIG_CAMERA_XCLK_USE_ESP_CLOCK_ROUTER
    case ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER:
        xclk_generator_soc_clock_t *xclk_soc_clock = heap_caps_calloc(1, sizeof(xclk_generator_soc_clock_t), XCLK_MEM_CAPS_DEFAULT);
        ESP_RETURN_ON_FALSE(xclk_soc_clock, ESP_ERR_NO_MEM, TAG, "no mem for xclk handle");
        xclk_soc_clock->base.cam_xclk_source = ESP_CAM_SENSOR_XCLK_ESP_CLOCK_ROUTER;
        xclk_soc_clock->base.start = xclk_soc_clock_generator_start;
        xclk_soc_clock->base.stop = xclk_soc_clock_generator_stop;
        xclk_soc_clock->base.free = xclk_soc_clock_generator_free;
        *ret_handle = &(xclk_soc_clock->base);
        break;
#endif
    default:
        ESP_LOGE(TAG, "source not supported");
        ret = ESP_ERR_INVALID_ARG;
        break;
    }
    return ret;
}

esp_err_t esp_cam_sensor_xclk_start(esp_cam_sensor_xclk_handle_t xclk_handle, const esp_cam_sensor_xclk_config_t *config)
{
    esp_err_t ret = ESP_OK;
    esp_cam_sensor_xclk_t *handle = (esp_cam_sensor_xclk_t *)xclk_handle;
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "handle address cannot be 0");
    ESP_RETURN_ON_FALSE(handle->start, ESP_ERR_NOT_SUPPORTED, TAG, "driver not supported");
    ret = handle->start(handle, config);
    return ret;
}

esp_err_t esp_cam_sensor_xclk_stop(esp_cam_sensor_xclk_handle_t xclk_handle)
{
    esp_cam_sensor_xclk_t *handle = (esp_cam_sensor_xclk_t *)xclk_handle;
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument: null pointer");
    ESP_RETURN_ON_FALSE(handle->stop, ESP_ERR_NOT_SUPPORTED, TAG, "driver not supported");

    return handle->stop(handle);
}

esp_err_t esp_cam_sensor_xclk_free(esp_cam_sensor_xclk_handle_t xclk_handle)
{
    esp_cam_sensor_xclk_t *handle = (esp_cam_sensor_xclk_t *)xclk_handle;
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, TAG, "invalid argument: null pointer");
    ESP_RETURN_ON_FALSE(handle->free, ESP_ERR_NOT_SUPPORTED, TAG, "driver not supported");

    return handle->free(handle);
}
