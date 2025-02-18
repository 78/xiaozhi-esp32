/*
 * Author: 施华锋
 * Date: 2025-01-16
 * Description: This header file defines the PT6324Writer class for communicating with the PT6324 device via SPI.
 */

#ifndef _PT6324_H_
#define _PT6324_H_

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_log.h>

/**
 * @class PT6324Writer
 * @brief A class for interacting with the PT6324 device using SPI communication.
 *
 * This class provides methods to initialize the PT6324 device, write data to it, and refresh its display.
 */
class PT6324Writer
{
public:
    /**
     * @brief Constructor for the PT6324Writer class.
     *
     * Initializes the PT6324Writer object with the specified GPIO pins and SPI host device.
     *
     * @param din The GPIO pin number for the data input line.
     * @param clk The GPIO pin number for the clock line.
     * @param cs The GPIO pin number for the chip select line.
     * @param spi_num The SPI host device number to use for communication.
     */
    PT6324Writer(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num);

    /**
     * @brief Constructor for the PT6324Writer class.
     *
     * Initializes the PT6324Writer object with an existing SPI device handle.
     *
     * @param spi_device The SPI device handle to use for communication.
     */
    PT6324Writer(spi_device_handle_t spi_device) : spi_device_(spi_device) {}

    /**
     * @brief Initializes the PT6324 device.
     *
     * This method performs any necessary setup operations to prepare the PT6324 device for use.
     */
    void pt6324_init();

private:
    spi_device_handle_t spi_device_;

    /**
     * @brief Writes data to the PT6324 device.
     *
     * Sends the specified data buffer of a given length to the PT6324 device via SPI.
     *
     * @param dat A pointer to the data buffer to be sent.
     * @param len The length of the data buffer in bytes.
     */
    void pt6324_write_data(uint8_t *dat, int len);

protected:
    /**
     * @brief Refreshes the display of the PT6324 device.
     *
     * Updates the display of the PT6324 device with the data stored in the given graphics RAM buffer.
     *
     * @param gram A pointer to the graphics RAM buffer containing the display data.
     */
    void pt6324_refrash(uint8_t *gram);
};

#endif