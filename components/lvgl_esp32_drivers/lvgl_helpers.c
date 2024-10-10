/**
 * @file lvgl_helpers.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "sdkconfig.h"
#include "lvgl_helpers.h"
#include "esp_log.h"

#include "lvgl_tft/disp_spi.h"
#include "lvgl_touch/tp_spi.h"

#include "lvgl_spi_conf.h"

#include "lvgl_i2c/i2c_manager.h"

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include <dirent.h>
/*********************
 *      DEFINES
 *********************/

#define TAG "lvgl_helpers"

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
#define MOUNT_POINT "/sdcard"
/* Interface and driver initialization */
void lvgl_driver_init(void)
{
    /* Since LVGL v8 LV_HOR_RES_MAX and LV_VER_RES_MAX are not defined, so
     * print it only if they are defined. */
#if (LVGL_VERSION_MAJOR < 8)
    ESP_LOGI(TAG, "Display hor size: %d, ver size: %d", LV_HOR_RES_MAX, LV_VER_RES_MAX);
#endif

    ESP_LOGI(TAG, "Display buffer size: %d", DISP_BUF_SIZE);

#if defined(CONFIG_LV_TFT_DISPLAY_CONTROLLER_FT81X)
    ESP_LOGI(TAG, "Initializing SPI master for FT81X");

    lvgl_spi_driver_init(TFT_SPI_HOST,
                         DISP_SPI_MISO, DISP_SPI_MOSI, DISP_SPI_CLK,
                         SPI_BUS_MAX_TRANSFER_SZ, 1,
                         DISP_SPI_IO2, DISP_SPI_IO3);

    disp_spi_add_device(TFT_SPI_HOST);
    disp_driver_init();

#if defined(CONFIG_LV_TOUCH_CONTROLLER_FT81X)
    touch_driver_init();
#endif

    return;
#endif

#if defined(SHARED_SPI_BUS)
    ESP_LOGI(TAG, "Initializing shared SPI master");

    lvgl_spi_driver_init(TFT_SPI_HOST,
                         TP_SPI_MISO, DISP_SPI_MOSI, DISP_SPI_CLK,
                         SPI_BUS_MAX_TRANSFER_SZ, 1,
                         -1, -1);

    disp_spi_add_device(TFT_SPI_HOST);
    tp_spi_add_device(TOUCH_SPI_HOST);

    disp_driver_init();
    touch_driver_init();

    return;
#endif
    /* Display controller initialization */
#if defined CONFIG_LV_TFT_DISPLAY_PROTOCOL_SPI
    ESP_LOGI(TAG, "Initializing SPI master for display");

    lvgl_spi_driver_init(TFT_SPI_HOST,
                         DISP_SPI_MISO, DISP_SPI_MOSI, DISP_SPI_CLK,
                         SPI_BUS_MAX_TRANSFER_SZ, 1,
                         DISP_SPI_IO2, DISP_SPI_IO3);

    disp_spi_add_device(TFT_SPI_HOST);

    disp_driver_init();
#elif defined(CONFIG_LV_I2C_DISPLAY)
    disp_driver_init();
#else
#error "No protocol defined for display controller"
#endif

/* Touch controller initialization */
#if CONFIG_LV_TOUCH_CONTROLLER != TOUCH_CONTROLLER_NONE
#if defined(CONFIG_LV_TOUCH_DRIVER_PROTOCOL_SPI)
    ESP_LOGI(TAG, "Initializing SPI master for touch");

    lvgl_spi_driver_init(TOUCH_SPI_HOST,
                         TP_SPI_MISO, TP_SPI_MOSI, TP_SPI_CLK,
                         0 /* Defaults to 4094 */, 2,
                         -1, -1);

    tp_spi_add_device(TOUCH_SPI_HOST);

    touch_driver_init();
#elif defined(CONFIG_LV_I2C_TOUCH)
   touch_driver_init();
#elif defined(CONFIG_LV_TOUCH_DRIVER_ADC)
    touch_driver_init();
#elif defined(CONFIG_LV_TOUCH_DRIVER_DISPLAY)
    touch_driver_init();
#else
#error "No protocol defined for touch controller"
#endif
#else
#endif
}

/* Initialize spi bus master
 *
 * NOTE: dma_chan type and value changed to int instead of spi_dma_chan_t
 * for backwards compatibility with ESP-IDF versions prior v4.3.
 *
 * We could use the ESP_IDF_VERSION_VAL macro available in the "esp_idf_version.h"
 * header available since ESP-IDF v4.
 */
bool lvgl_spi_driver_init(int host,
                          int miso_pin, int mosi_pin, int sclk_pin,
                          int max_transfer_sz,
                          int dma_channel,
                          int quadwp_pin, int quadhd_pin)
{
    int dma_chan = 0 /* SPI_DMA_DISABLED */;

#if defined(CONFIG_IDF_TARGET_ESP32)
    assert((SPI_HOST <= host) && (VSPI_HOST >= host));
    const char *spi_names[] = {
        "SPI_HOST", "HSPI_HOST", "VSPI_HOST"};

    dma_chan = dma_channel;
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
    assert((SPI_HOST <= host) && (HSPI_HOST >= host));
    const char *spi_names[] = {
        "SPI_HOST", "", ""};

    dma_chan = dma_channel;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    assert((SPI1_HOST <= host) && (SPI3_HOST >= host));
    const char *spi_names[] = {
        "SPI1_HOST", "SPI2_HOST", "SPI3_HOST"};

    dma_chan = 3 /* SPI_DMA_CH_AUTO */;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    //    assert((SPI_HOST <= host) && (SPI3_HOST >= host));
    const char *spi_names[] = {
        "SPI_HOST", "HSPI_HOST", "VSPI_HOST"};

    dma_chan = SPI_DMA_CH_AUTO;
#else
#error "Target chip not selected"
#endif

    ESP_LOGI(TAG, "Configuring SPI host %s (%d)", spi_names[host], host);
    ESP_LOGI(TAG, "MISO pin: %d, MOSI pin: %d, SCLK pin: %d, IO2/WP pin: %d, IO3/HD pin: %d",
             miso_pin, mosi_pin, sclk_pin, quadwp_pin, quadhd_pin);

    ESP_LOGI(TAG, "Max transfer size: %d (bytes)", max_transfer_sz);

    spi_bus_config_t buscfg = {
        .miso_io_num = miso_pin,
        .mosi_io_num = mosi_pin,
        .sclk_io_num = sclk_pin,
        .quadwp_io_num = quadwp_pin,
        .quadhd_io_num = quadhd_pin,
        .max_transfer_sz = max_transfer_sz};

    ESP_LOGI(TAG, "Initializing SPI bus...");
    esp_err_t ret = spi_bus_initialize(host, &buscfg, dma_chan);
    assert(ret == ESP_OK);

    return ESP_OK != ret;
}
