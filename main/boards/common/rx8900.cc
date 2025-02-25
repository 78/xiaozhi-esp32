/*
 * Author: 施华锋
 * Date: 2025-01-16
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "i2c_bus.h"
#include "rx8900.h"
#include "esp_log.h"

static uint8_t rx8900_available = 0;

rx8900_handle_t rx8900_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    rx8900_dev_t *sens = (rx8900_dev_t *)calloc(1, sizeof(rx8900_dev_t));
    sens->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sens->i2c_dev == NULL)
    {
        free(sens);
        return NULL;
    }
    return (rx8900_handle_t)sens;
}

esp_err_t rx8900_delete(rx8900_handle_t *sensor)
{
    if (*sensor == NULL)
    {
        return ESP_OK;
    }
    rx8900_dev_t *sens = (rx8900_dev_t *)(*sensor);
    i2c_bus_device_delete(&sens->i2c_dev);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

esp_err_t rx8900_default_init(rx8900_handle_t sensor)
{
    rx8900_dev_t *sens = (rx8900_dev_t *)sensor;
    // reset the sens using soft-reset, this makes sure the IIR is off, etc.
    if (i2c_bus_write_byte(sens->i2c_dev, RX8900_EXT_REG, 0x08) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (i2c_bus_write_byte(sens->i2c_dev, RX8900_REG_STATUS, 0) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (i2c_bus_write_byte(sens->i2c_dev, RX8900_REG_CONTROL, 64) != ESP_OK)
    {
        return ESP_FAIL;
    }
    rx8900_available = 1;
    return ESP_OK;
}

esp_err_t rx8900_read_temperature(rx8900_handle_t sensor, float *temperature)
{
    if (!rx8900_available)
        return ESP_FAIL;
    uint8_t data[1] = {0};
    rx8900_dev_t *sens = (rx8900_dev_t *)sensor;
    if (i2c_bus_read_bytes(sens->i2c_dev, RX8900_REG_TEMP, 1, data) != ESP_OK)
    {
        return ESP_FAIL;
    }

    *temperature = (data[0] * 2 - 187.19) / 3.218;
    return ESP_OK;
}

static uint8_t B2D(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static uint8_t D2B(uint8_t decimal)
{
    return (((decimal / 10) << 4) | (decimal % 10));
}

esp_err_t rx8900_read_time(rx8900_handle_t sensor, struct tm *tm_local)
{
    if (!rx8900_available)
        return ESP_FAIL;
    uint8_t data[7] = {0};
    rx8900_dev_t *sens = (rx8900_dev_t *)sensor;
    if (i2c_bus_read_bytes(sens->i2c_dev, RX8900_REG_TIME, 7, data) != ESP_OK)
    {
        return ESP_FAIL;
    }

    tm_local->tm_sec = B2D(data[0] & 0x7F);
    tm_local->tm_min = B2D(data[1] & 0x7F);
    tm_local->tm_hour = B2D(data[2] & 0x3F);
    tm_local->tm_wday = (data[3] & 0x7f);
    tm_local->tm_mday = B2D(data[4] & 0x3F);
    tm_local->tm_mon = B2D(data[5] & 0x1F) - 1;
    tm_local->tm_year = B2D(data[6]) % 100 + 100;

    return ESP_OK;
}

esp_err_t rx8900_write_time(rx8900_handle_t sensor, struct tm *tm_local)
{
    if (!rx8900_available)
        return ESP_FAIL;
    uint8_t data[7] = {D2B(tm_local->tm_sec), D2B(tm_local->tm_min), D2B(tm_local->tm_hour), (uint8_t)(tm_local->tm_wday), D2B(tm_local->tm_mday), D2B(tm_local->tm_mon + 1), D2B(tm_local->tm_year % 100)};

    rx8900_dev_t *sens = (rx8900_dev_t *)sensor;
    if (i2c_bus_write_bytes(sens->i2c_dev, RX8900_REG_TIME, 7, data) != ESP_OK)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}
