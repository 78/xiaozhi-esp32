/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include <inttypes.h>
#include "hal/gpio_ll.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_sccb_i2c.h"
#include "esp_cam_sensor_detect.h"

#include "esp_video_init.h"
#include "esp_video_device_internal.h"
#include "esp_private/esp_cam_dvp.h"
#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
#include "esp_video_pipeline_isp.h"
#endif

#define SCCB_NUM_MAX                I2C_NUM_MAX

#define INTF_PORT_NAME(port)        (((port) == ESP_CAM_SENSOR_DVP) ? "DVP" : "CSI")

/**
 * @brief SCCB initialization mark
 */
typedef struct sccb_mark {
    int i2c_ref;                                /*!< I2C reference */
    i2c_master_bus_handle_t handle;             /*!< I2C master handle */
    const esp_video_init_sccb_config_t *config; /*!< SCCB initialization config pointer */
    uint16_t dev_addr;                          /*!< Slave device address */
    esp_cam_sensor_port_t port;                 /*!< Slave device data interface */
} esp_video_init_sccb_mark_t;

static const char *TAG = "esp_video_init";

#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE || CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
/**
 * @brief Create I2C master handle
 *
 * @param mark SCCB initialization make array
 * @param port Slave device data interface
 * @param init_sccb_config SCCB initialization configuration
 * @param dev_addr device address
 *
 * @return
 *      - I2C master handle on success
 *      - NULL if failed
 */
static i2c_master_bus_handle_t create_i2c_master_bus(esp_video_init_sccb_mark_t *mark,
        esp_cam_sensor_port_t port,
        const esp_video_init_sccb_config_t *init_sccb_config,
        uint16_t dev_addr)
{
    esp_err_t ret;
    i2c_master_bus_handle_t bus_handle = NULL;
    int i2c_port = init_sccb_config->i2c_config.port;

    if (i2c_port > SCCB_NUM_MAX) {
        return NULL;
    }

    if (mark[i2c_port].handle != NULL) {
        if (init_sccb_config->i2c_config.scl_pin != mark[i2c_port].config->i2c_config.scl_pin) {
            ESP_LOGE(TAG, "Interface %s and %s: I2C port %d SCL pin is mismatched", INTF_PORT_NAME(i2c_port),
                     INTF_PORT_NAME(mark[i2c_port].port), i2c_port);
            return NULL;
        }

        if (init_sccb_config->i2c_config.sda_pin != mark[i2c_port].config->i2c_config.sda_pin) {
            ESP_LOGE(TAG, "Interface %s and %s: I2C port %d SDA pin is mismatched", INTF_PORT_NAME(i2c_port),
                     INTF_PORT_NAME(mark[i2c_port].port), i2c_port);
            return NULL;
        }

        if (dev_addr == mark[i2c_port].dev_addr) {
            ESP_LOGE(TAG, "Interface %s and %s: use same SCCB device address %d", INTF_PORT_NAME(i2c_port),
                     INTF_PORT_NAME(mark[i2c_port].port), dev_addr);
            return NULL;
        }

        bus_handle = mark[i2c_port].handle;
    } else {
        i2c_master_bus_config_t i2c_bus_config = {0};

        i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        i2c_bus_config.i2c_port = init_sccb_config->i2c_config.port;
        i2c_bus_config.scl_io_num = init_sccb_config->i2c_config.scl_pin;
        i2c_bus_config.sda_io_num = init_sccb_config->i2c_config.sda_pin;
        i2c_bus_config.glitch_ignore_cnt = 7;
        i2c_bus_config.flags.enable_internal_pullup = true,

        ret = i2c_new_master_bus(&i2c_bus_config, &bus_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to initialize I2C master bus port %d", init_sccb_config->i2c_config.port);
            return NULL;
        }

        mark[i2c_port].handle = bus_handle;
        mark[i2c_port].config = init_sccb_config;
        mark[i2c_port].dev_addr = dev_addr;
        mark[i2c_port].port = port;
    }

    mark[i2c_port].i2c_ref++;
    assert(mark[i2c_port].i2c_ref > 0);

    return bus_handle;
}

/**
 * @brief Create SCCB device
 *
 * @param mark SCCB initialization make array
 * @param port Slave device data interface
 * @param init_sccb_config SCCB initialization configuration
 * @param dev_addr device address
 *
 * @return
 *      - SCCB handle on success
 *      - NULL if failed
 */
static esp_sccb_io_handle_t create_sccb_device(esp_video_init_sccb_mark_t *mark,
        esp_cam_sensor_port_t port,
        const esp_video_init_sccb_config_t *init_sccb_config,
        uint16_t dev_addr)
{
    esp_err_t ret;
    esp_sccb_io_handle_t sccb_io;
    sccb_i2c_config_t sccb_config = {0};
    i2c_master_bus_handle_t bus_handle;

    if (init_sccb_config->init_sccb) {
        bus_handle = create_i2c_master_bus(mark, port, init_sccb_config, dev_addr);
    } else {
        bus_handle = init_sccb_config->i2c_handle;
    }

    if (!bus_handle) {
        return NULL;
    }

    sccb_config.dev_addr_length = I2C_ADDR_BIT_LEN_7,
    sccb_config.device_address = dev_addr,
    sccb_config.scl_speed_hz = init_sccb_config->freq,
    ret = sccb_new_i2c_io(bus_handle, &sccb_config, &sccb_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize SCCB");
        return NULL;
    }

    return sccb_io;
}

/**
 * @brief Destroy SCCB device
 *
 * @param handle SCCB handle
 * @param mark SCCB initialization make array
 *
 * @return None
 */
static void destroy_sccb_device(esp_sccb_io_handle_t handle, esp_video_init_sccb_mark_t *mark,
                                const esp_video_init_sccb_config_t *init_sccb_config)
{
    esp_sccb_del_i2c_io(handle);
    if (init_sccb_config->init_sccb) {
        int i2c_port = init_sccb_config->i2c_config.port;

        if (mark[i2c_port].handle) {
            assert(mark[i2c_port].i2c_ref > 0);
            mark[i2c_port].i2c_ref--;
            if (!mark[i2c_port].i2c_ref) {
                i2c_del_master_bus(mark[i2c_port].handle);
                mark[i2c_port].handle = NULL;
            }
        }
    }
}
#endif

/**
 * @brief Initialize video hardware and software, including I2C, MIPI CSI and so on.
 *
 * @param config video hardware configuration
 *
 * @return
 *      - ESP_OK on success
 *      - Others if failed
 */
esp_err_t esp_video_init(const esp_video_init_config_t *config)
{
    esp_err_t ret;
    bool csi_inited = false;
    bool dvp_inited = false;
    esp_video_init_sccb_mark_t sccb_mark[SCCB_NUM_MAX] = {0};

    if (config == NULL) {
        ESP_LOGW(TAG, "Please validate camera config");
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE
    ret = esp_video_create_isp_video_device();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to create hardware ISP video device");
        return ret;
    }
#endif

    for (esp_cam_sensor_detect_fn_t *p = &__esp_cam_sensor_detect_fn_array_start; p < &__esp_cam_sensor_detect_fn_array_end; ++p) {
#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
        if (!csi_inited && p->port == ESP_CAM_SENSOR_MIPI_CSI && config->csi != NULL) {
            esp_cam_sensor_config_t cfg;
            esp_cam_sensor_device_t *cam_dev;

            cfg.sccb_handle = create_sccb_device(sccb_mark, ESP_CAM_SENSOR_MIPI_CSI, &config->csi->sccb_config, p->sccb_addr);
            if (!cfg.sccb_handle) {
                return ESP_FAIL;
            }

            cfg.reset_pin = config->csi->reset_pin,
            cfg.pwdn_pin = config->csi->pwdn_pin,
            cam_dev = (*(p->detect))((void *)&cfg);
            if (!cam_dev) {
                destroy_sccb_device(cfg.sccb_handle, sccb_mark, &config->csi->sccb_config);
                ESP_LOGE(TAG, "failed to detect MIPI-CSI camera sensor with address=%x", p->sccb_addr);
                continue;
            }

            ret = esp_video_create_csi_video_device(cam_dev);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "failed to create MIPI-CSI video device");
                return ret;
            }

#if CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER
            if (cam_dev->cur_format && cam_dev->cur_format->isp_info) {
                const esp_ipa_config_t *ipa_config = esp_ipa_pipeline_get_config(cam_dev->name);
                if (ipa_config) {
                    esp_video_isp_config_t isp_config = {
                        .cam_dev = ESP_VIDEO_MIPI_CSI_DEVICE_NAME,
                        .isp_dev = ESP_VIDEO_ISP1_DEVICE_NAME,
                        .ipa_config = ipa_config
                    };

                    ret = esp_video_isp_pipeline_init(&isp_config);
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "failed to create ISP pipeline controller");
                        return ret;
                    }
                } else {
                    ESP_LOGW(TAG, "failed to get configuration to initialize ISP controller");
                }
            }
#endif
            csi_inited = true;
        }
#endif

#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
        if (!dvp_inited && p->port == ESP_CAM_SENSOR_DVP && config->dvp != NULL) {
            int dvp_ctlr_id = 0;
            esp_cam_sensor_config_t cfg;
            esp_cam_sensor_device_t *cam_dev;

            ret = esp_cam_ctlr_dvp_init(dvp_ctlr_id, CAM_CLK_SRC_DEFAULT, &config->dvp->dvp_pin);
            if (ret != ESP_OK) {
                return ret;
            }

            if (config->dvp->dvp_pin.xclk_io >= 0 && config->dvp->xclk_freq > 0) {
                ret = esp_cam_ctlr_dvp_output_clock(dvp_ctlr_id, CAM_CLK_SRC_DEFAULT, config->dvp->xclk_freq);
                if (ret != ESP_OK) {
                    return ret;
                }
            }

            cfg.sccb_handle = create_sccb_device(sccb_mark, ESP_CAM_SENSOR_DVP, &config->dvp->sccb_config, p->sccb_addr);
            if (!cfg.sccb_handle) {
                return ESP_FAIL;
            }

            cfg.reset_pin = config->dvp->reset_pin,
            cfg.pwdn_pin = config->dvp->pwdn_pin,
            cam_dev = (*(p->detect))((void *)&cfg);
            if (!cam_dev) {
                destroy_sccb_device(cfg.sccb_handle, sccb_mark, &config->dvp->sccb_config);
                esp_cam_ctlr_dvp_deinit(dvp_ctlr_id);
                ESP_LOGE(TAG, "failed to detect DVP camera with address=%x", p->sccb_addr);
                continue;
            }

            ret = esp_video_create_dvp_video_device(cam_dev);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "failed to create DVP video device");
                return ret;
            }

            dvp_inited = true;
        }
#endif
    }

#if CONFIG_ESP_VIDEO_ENABLE_HW_H264_VIDEO_DEVICE
    ret = esp_video_create_h264_video_device(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to create hardware H.264 video device");
        return ret;
    }
#endif

#if CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_VIDEO_DEVICE
    jpeg_encoder_handle_t handle = NULL;

    if (config->jpeg) {
        handle = config->jpeg->enc_handle;
    }

    ret = esp_video_create_jpeg_video_device(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to create hardware JPEG video device");
        return ret;
    }
#endif

    return ESP_OK;
}
