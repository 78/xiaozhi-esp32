/**
 * @file disp_spi.h
 *
 */

#ifndef DISP_SPI_H
#define DISP_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdint.h>
#include <stdbool.h>
#include <driver/spi_master.h>
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#define gpio_pad_select_gpio esp_rom_gpio_pad_select_gpio
#endif
#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef enum _disp_spi_send_flag_t {
    DISP_SPI_SEND_QUEUED        = 0x00000000,
    DISP_SPI_SEND_POLLING       = 0x00000001,
    DISP_SPI_SEND_SYNCHRONOUS   = 0x00000002,
    DISP_SPI_SIGNAL_FLUSH       = 0x00000004,
    DISP_SPI_RECEIVE            = 0x00000008,
    DISP_SPI_CMD_8              = 0x00000010, /* Reserved */
    DISP_SPI_CMD_16             = 0x00000020, /* Reserved */
    DISP_SPI_ADDRESS_8          = 0x00000040, 
    DISP_SPI_ADDRESS_16         = 0x00000080, 
    DISP_SPI_ADDRESS_24         = 0x00000100, 
    DISP_SPI_ADDRESS_32         = 0x00000200, 
    DISP_SPI_MODE_DIO           = 0x00000400, 
    DISP_SPI_MODE_QIO           = 0x00000800, 
    DISP_SPI_MODE_DIOQIO_ADDR   = 0x00001000, 
	DISP_SPI_VARIABLE_DUMMY		= 0x00002000,
} disp_spi_send_flag_t;


/**********************
 * GLOBAL PROTOTYPES
 **********************/
void disp_spi_add_device(spi_host_device_t host);
void disp_spi_add_device_config(spi_host_device_t host, spi_device_interface_config_t *devcfg);
void disp_spi_add_device_with_speed(spi_host_device_t host, int clock_speed_hz);
void disp_spi_change_device_speed(int clock_speed_hz);
void disp_spi_remove_device();

/*	Important! 
	All buffers should also be 32-bit aligned and DMA capable to prevent extra allocations and copying.
	When DMA reading (even in polling mode) the ESP32 always read in 4-byte chunks even if less is requested.
	Extra space will be zero filled. Always ensure the out buffer is large enough to hold at least 4 bytes!
*/
void disp_spi_transaction(const uint8_t *data, size_t length,
    disp_spi_send_flag_t flags, uint8_t *out, uint64_t addr, uint8_t dummy_bits);

void disp_wait_for_pending_transactions(void);
void disp_spi_acquire(void);
void disp_spi_release(void);

static inline void disp_spi_send_data(uint8_t *data, size_t length) {
    disp_spi_transaction(data, length, DISP_SPI_SEND_POLLING, NULL, 0, 0);
}

static inline void disp_spi_send_colors(uint8_t *data, size_t length) {
    disp_spi_transaction(data, length,
        DISP_SPI_SEND_QUEUED | DISP_SPI_SIGNAL_FLUSH,
        NULL, 0, 0);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*DISP_SPI_H*/
