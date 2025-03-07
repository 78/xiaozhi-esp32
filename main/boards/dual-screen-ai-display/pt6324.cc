/*
 * Author: 施华锋
 * Date: 2025-02-12
 * Description: This file implements the methods of the PT6324Writer class for communicating with the PT6324 device via SPI.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "pt6324.h"
#include "string.h"

#define TAG "PT6324Writer"

/**
 * @brief Writes data to the PT6324 device.
 *
 * This function constructs an SPI transaction to send the specified data buffer of a given length to the PT6324 device.
 * It uses the SPI device handle stored in the class to queue the transaction.
 *
 * @param dat A pointer to the data buffer to be sent.
 * @param len The length of the data buffer in bits.
 */
void PT6324Writer::pt6324_write_data(uint8_t *dat, int len)
{
    // Create an SPI transaction structure and initialize it to zero
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    // Set the length of the transaction in bits
    t.length = len;

    // Set the pointer to the data buffer to be transmitted
    t.tx_buffer = dat;

    // Queue the SPI transaction. This function will block until the transaction can be queued.
    ESP_ERROR_CHECK(spi_device_queue_trans(spi_device_, &t, portMAX_DELAY));

    // The following code can be uncommented if you need to wait for the transaction to complete and verify the result
    spi_transaction_t *ret_trans;
    ESP_ERROR_CHECK(spi_device_get_trans_result(spi_device_, &ret_trans, portMAX_DELAY));
    assert(ret_trans == &t);

    return;
}

/**
 * @brief Constructor for the PT6324Writer class.
 *
 * Initializes the SPI bus and adds the PT632 PT6324 device to the SPI bus using the specified GPIO pins and SPI host device SPI host device.
 *
 * @param din The GPIO pin number for the data input line.
 * @param clk The GPIO pin number for the clock line.
 * @param cs The GPIO pin number for the chip select line.
 * @param spi_num The SPI host device number to use for communication.
 */
PT6324Writer::PT6324Writer(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num)
{
    // Initialize the SPI bus configuration structure
    spi_bus_config_t buscfg = {0};

    // Log the initialization process
    ESP_LOGI(TAG, "Initialize VFD SPI bus");

    // Set the clock and data pins for the SPI bus
    buscfg.sclk_io_num = clk;
    buscfg.data0_io_num = din;

    // Set the maximum transfer size in bytes
    buscfg.max_transfer_sz = 256;

    // Initialize the SPI bus with the specified configuration
    ESP_ERROR_CHECK(spi_bus_initialize(spi_num, &buscfg, SPI_DMA_CH_AUTO));

    // Initialize the SPI device interface configuration structure
    spi_device_interface_config_t devcfg = {
        .mode = 3,                 // Set the SPI mode to 3
        .clock_speed_hz = 1000000, // Set the clock speed to 1MHz
        .spics_io_num = cs,        // Set the chip select pin
        .flags = SPI_DEVICE_BIT_LSBFIRST,
        .queue_size = 7,
    };

    // Add the PT6324 device to the SPI bus with the specified configuration
    ESP_ERROR_CHECK(spi_bus_add_device(spi_num, &devcfg, &spi_device_));
}

/**
 * @brief Initializes the PT6324 device.
 *
 * Sends the initialization data to set the brightness of the PT6324 device.
 */
void PT6324Writer::pt6324_init()
{
    // Define the initialization data to set the brightness
    uint8_t data[] = {0x0F, 0x0F, 0x40};
    dimmen = true;
    // Send the initialization data to the PT6324 device
    pt6324_write_data(data, (sizeof data) * 8);
}

void PT6324Writer::pt6324_setbrightness(uint8_t brightness)
{
    dimming = brightness * 8 / 100;
    if (dimming > 7)
        dimming = 7;
    else if (dimming < 1)
        dimming = 1;
}

void PT6324Writer::pt6324_setsleep(bool en)
{
    dimmen = !en;
    if (!dimmen)
    {
        memset(gram, 0, sizeof gram);
        // pt6324_refrash(gram);
    }
}

/**
 * @brief Refreshes the display of the PT6324 device.
 *
 * Updates the display of the PT6324 device with the data stored in the given graphics RAM buffer.
 *
 * @param gram A pointer to the graphics RAM buffer containing the display data.
 */
void PT6324Writer::pt6324_refrash(uint8_t *gram)
{
    // Create a buffer to hold the data to be sent for refreshing the display
    uint8_t data_gram[48 + 1] = {0};

    // Set the start address command
    data_gram[0] = 0xC0;

    // Copy the graphics RAM data to the buffer
    for (size_t i = 1; i < sizeof data_gram; i++)
        data_gram[i] = gram[i - 1];

    // Send the graphics RAM data to the PT6324 device
    pt6324_write_data(data_gram, (sizeof data_gram) * 8);

    // Define the command to turn on the display
    uint8_t data[1] = {0x80};

    data[0] |= dimming | (dimmen ? 0x8 : 0);
    
    // Send the display on command to the PT6324 device
    pt6324_write_data(data, (sizeof data) * 8);
}