/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "i2c_bus.h"
#include "bmp280.h"
#include "math.h"
#include "esp_log.h"

bmp280_handle_t bmp280_create(i2c_bus_handle_t bus, uint8_t dev_addr)
{
    bmp280_dev_t *sens = (bmp280_dev_t *)calloc(1, sizeof(bmp280_dev_t));
    sens->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
    if (sens->i2c_dev == NULL)
    {
        free(sens);
        return NULL;
    }
    sens->dev_addr = dev_addr;
    return (bmp280_handle_t)sens;
}

esp_err_t bmp280_delete(bmp280_handle_t *sensor)
{
    if (*sensor == NULL)
    {
        return ESP_OK;
    }
    bmp280_dev_t *sens = (bmp280_dev_t *)(*sensor);
    i2c_bus_device_delete(&sens->i2c_dev);
    free(sens);
    *sensor = NULL;
    return ESP_OK;
}

static esp_err_t bmp280_read_uint16(bmp280_handle_t sensor, uint8_t addr, uint16_t *data)
{
    esp_err_t ret = ESP_FAIL;
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    uint8_t data0, data1;
    if (i2c_bus_read_byte(sens->i2c_dev, addr, &data0) != ESP_OK)
    {
        return ret;
    }
    if (i2c_bus_read_byte(sens->i2c_dev, addr + 1, &data1) != ESP_OK)
    {
        return ret;
    }
    *data = (data0 << 8) | data1;
    return ESP_OK;
}

static esp_err_t bmp280_read_uint16_le(bmp280_handle_t sensor, uint8_t addr, uint16_t *data)
{
    esp_err_t ret = ESP_FAIL;
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    uint8_t data0, data1;
    if (i2c_bus_read_byte(sens->i2c_dev, addr, &data0) != ESP_OK)
    {
        return ret;
    }
    if (i2c_bus_read_byte(sens->i2c_dev, addr + 1, &data1) != ESP_OK)
    {
        return ret;
    }
    *data = (data1 << 8) | data0;
    return ESP_OK;
}

unsigned int bmp280_getconfig(bmp280_handle_t sensor)
{
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    return (sens->config_t.t_sb << 5) | (sens->config_t.filter << 3) | sens->config_t.spi3w_en;
}

unsigned int bmp280_getctrl_meas(bmp280_handle_t sensor)
{
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    return (sens->ctrl_meas_t.osrs_t << 5) | (sens->ctrl_meas_t.osrs_p << 3) | sens->ctrl_meas_t.mode;
}

unsigned int bmp280_getctrl_hum(bmp280_handle_t sensor)
{
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    return (sens->ctrl_hum_t.osrs_h);
}

bool bmp280_is_reading_calibration(bmp280_handle_t sensor)
{
    uint8_t rstatus = 0;
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    if (i2c_bus_read_byte(sens->i2c_dev, BMP280_REGISTER_STATUS, &rstatus) != ESP_OK)
    {
        return false;
    }
    return (rstatus & (1 << 0)) != 0;
}

esp_err_t bmp280_read_coefficients(bmp280_handle_t sensor)
{
    uint8_t data = 0;
    uint8_t data1 = 0;
    uint16_t data16 = 0;
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_T1, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_t1 = data16;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_T2, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_t2 = (int16_t)data16;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_T3, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_t3 = (int16_t)data16;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_P1, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_p1 = data16;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_P2, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_p2 = (int16_t)data16;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_P3, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_p3 = (int16_t)data16;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_P4, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_p4 = (int16_t)data16;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_P5, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_p5 = (int16_t)data16;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_P6, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_p6 = (int16_t)data16;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_P7, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_p7 = (int16_t)data16;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_P8, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_p8 = (int16_t)data16;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_P9, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_p9 = (int16_t)data16;
    if (i2c_bus_read_byte(sens->i2c_dev, BMP280_REGISTER_DIG_H1, &data) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_h1 = data;
    if (bmp280_read_uint16_le(sensor, BMP280_REGISTER_DIG_H2, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_h2 = (int16_t)data16;
    if (i2c_bus_read_byte(sens->i2c_dev, BMP280_REGISTER_DIG_H3, &data) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_h3 = data;
    if (i2c_bus_read_byte(sens->i2c_dev, BMP280_REGISTER_DIG_H4, &data) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (i2c_bus_read_byte(sens->i2c_dev, BMP280_REGISTER_DIG_H4 + 1, &data1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_h4 = (data << 4) | (data1 & 0xF);
    if (i2c_bus_read_byte(sens->i2c_dev, BMP280_REGISTER_DIG_H5 + 1, &data) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (i2c_bus_read_byte(sens->i2c_dev, BMP280_REGISTER_DIG_H5, &data1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_h5 = (data << 4) | (data1 >> 4);
    if (i2c_bus_read_byte(sens->i2c_dev, BMP280_REGISTER_DIG_H6, &data) != ESP_OK)
    {
        return ESP_FAIL;
    }
    sens->data_t.dig_h6 = (int8_t)data;
    return ESP_OK;
}

esp_err_t bmp280_set_sampling(bmp280_handle_t sensor, bmp280_sensor_mode mode, bmp280_sensor_sampling tempSampling, bmp280_sensor_sampling pressSampling, bmp280_sensor_sampling humSampling, bmp280_sensor_filter filter, bmp280_standby_duration duration)
{
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    sens->ctrl_meas_t.mode = mode;
    sens->ctrl_meas_t.osrs_t = tempSampling;
    sens->ctrl_meas_t.osrs_p = pressSampling;
    sens->ctrl_hum_t.osrs_h = humSampling;
    sens->config_t.filter = filter;
    sens->config_t.t_sb = duration;
    // you must make sure to also set REGISTER_CONTROL after setting the
    // CONTROLHUMID register, otherwise the values won't be applied (see DS 5.4.3)
    if (i2c_bus_write_byte(sens->i2c_dev, BMP280_REGISTER_CONTROLHUMID, bmp280_getctrl_hum(sensor)) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (i2c_bus_write_byte(sens->i2c_dev, BMP280_REGISTER_CONFIG, bmp280_getconfig(sensor)) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (i2c_bus_write_byte(sens->i2c_dev, BMP280_REGISTER_CONTROL, bmp280_getctrl_meas(sensor)) != ESP_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t bmp280_default_init(bmp280_handle_t sensor)
{
    // check if sensor, i.e. the chip ID is correct
    uint8_t chipid = 0;
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    if (i2c_bus_read_byte(sens->i2c_dev, BMP280_REGISTER_CHIPID, &chipid) != ESP_OK)
    {
        ESP_LOGI("BMP280:", "bmp280_default_init->bmp280_read_byte ->BMP280_REGISTER_CHIPID failed!!!!:%x", chipid);
        return ESP_FAIL;
    }
    if (chipid != BMP280_DEFAULT_CHIPID)
    {
        ESP_LOGI("BMP280:", "bmp280_default_init->BMP280_DEFAULT_CHIPID:%x", chipid);
        return ESP_FAIL;
    }
    // reset the sens using soft-reset, this makes sure the IIR is off, etc.
    if (i2c_bus_write_byte(sens->i2c_dev, BMP280_REGISTER_SOFTRESET, 0xB6) != ESP_OK)
    {
        return ESP_FAIL;
    }
    // wait for chip to wake up.
    vTaskDelay(300 / portTICK_RATE_MS);
    // if chip is still reading calibration, delay
    while (bmp280_is_reading_calibration(sensor))
    {
        vTaskDelay(100 / portTICK_RATE_MS);
    }
    if (bmp280_read_coefficients(sensor) != ESP_OK)
    { // read trimming parameters, see DS 4.2.2
        return ESP_FAIL;
    }
    if (bmp280_set_sampling(sensor, BMP280_MODE_NORMAL, BMP280_SAMPLING_X16, BMP280_SAMPLING_X16, BMP280_SAMPLING_X16, BMP280_FILTER_OFF, BMP280_STANDBY_MS_0_5) != ESP_OK)
    { // use defaults
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t bmp280_take_forced_measurement(bmp280_handle_t sensor)
{
    uint8_t data = 0;
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    if (sens->ctrl_meas_t.mode == BMP280_MODE_FORCED)
    {
        // set to forced mode, i.e. "take next measurement"
        if (i2c_bus_write_byte(sens->i2c_dev, BMP280_REGISTER_CONTROL, bmp280_getctrl_meas(sensor)) != ESP_OK)
        {
            return ESP_FAIL;
        }
        // wait until measurement has been completed, otherwise we would read, the values from the last measurement
        if (i2c_bus_read_byte(sens->i2c_dev, BMP280_REGISTER_STATUS, &data) != ESP_OK)
        {
            return ESP_FAIL;
        }
        while (data & 0x08)
        {
            i2c_bus_read_byte(sens->i2c_dev, BMP280_REGISTER_STATUS, &data);
            vTaskDelay(10 / portTICK_RATE_MS);
        }
    }
    return ESP_OK;
}

esp_err_t bmp280_read_temperature(bmp280_handle_t sensor, float *temperature)
{
    int32_t var1, var2;
    uint8_t data[3] = {0};
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    if (i2c_bus_read_bytes(sens->i2c_dev, BMP280_REGISTER_TEMPDATA, 3, data) != ESP_OK)
    {
        return ESP_FAIL;
    }
    int32_t adc_T = (data[0] << 16) | (data[1] << 8) | data[2];
    if (adc_T == 0x800000)
    { // value in case temp measurement was disabled
        return ESP_FAIL;
    }
    adc_T >>= 4;

    var1 = ((((adc_T >> 3) - ((int32_t)sens->data_t.dig_t1 << 1))) * ((int32_t)sens->data_t.dig_t2)) >> 11;

    var2 = (((((adc_T >> 4) - ((int32_t)sens->data_t.dig_t1)) * ((adc_T >> 4) - ((int32_t)sens->data_t.dig_t1))) >> 12) * ((int32_t)sens->data_t.dig_t3)) >> 14;

    sens->t_fine = var1 + var2;
    *temperature = ((sens->t_fine * 5 + 128) >> 8) / 100.0;
    return ESP_OK;
}

esp_err_t bmp280_read_pressure(bmp280_handle_t sensor, float *pressure)
{
    int64_t var1, var2, p;
    uint8_t data[3] = {0};
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    float temp = 0.0;
    if (bmp280_read_temperature(sensor, &temp) != ESP_OK)
    {
        // must be done first to get t_fine
        return ESP_FAIL;
    }
    if (i2c_bus_read_bytes(sens->i2c_dev, BMP280_REGISTER_PRESSUREDATA, 3, data) != ESP_OK)
    {
        return ESP_FAIL;
    }
    int32_t adc_P = (data[0] << 16) | (data[1] << 8) | data[2];
    if (adc_P == 0x800000)
    { // value in case pressure measurement was disabled
        return ESP_FAIL;
    }
    adc_P >>= 4;
    var1 = ((int64_t)sens->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)sens->data_t.dig_p6;
    var2 = var2 + ((var1 * (int64_t)sens->data_t.dig_p5) << 17);
    var2 = var2 + (((int64_t)sens->data_t.dig_p4) << 35);
    var1 = ((var1 * var1 * (int64_t)sens->data_t.dig_p3) >> 8) + ((var1 * (int64_t)sens->data_t.dig_p2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)sens->data_t.dig_p1) >> 33;
    if (var1 == 0)
    {
        return ESP_FAIL; // avoid exception caused by division by zero
    }
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)sens->data_t.dig_p9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)sens->data_t.dig_p8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)sens->data_t.dig_p7) << 4);
    p = p >> 8; // /256
    *pressure = (float)p / 100;
    return ESP_OK;
}

esp_err_t bmp280_read_humidity(bmp280_handle_t sensor, float *humidity)
{
    uint16_t data16;
    bmp280_dev_t *sens = (bmp280_dev_t *)sensor;
    float temp = 0.0;
    if (bmp280_read_temperature(sensor, &temp) != ESP_OK)
    {
        // must be done first to get t_fine
        return ESP_FAIL;
    }
    if (bmp280_read_uint16(sensor, BMP280_REGISTER_HUMIDDATA, &data16) != ESP_OK)
    {
        return ESP_FAIL;
    }
    int32_t adc_H = data16;
    if (adc_H == 0x8000)
    { // value in case humidity measurement was disabled
        return ESP_FAIL;
    }
    int32_t v_x1_u32r;
    v_x1_u32r = (sens->t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)sens->data_t.dig_h4) << 20) - (((int32_t)sens->data_t.dig_h5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)sens->data_t.dig_h6)) >> 10) * (((v_x1_u32r * ((int32_t)sens->data_t.dig_h3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)sens->data_t.dig_h2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)sens->data_t.dig_h1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    *humidity = (v_x1_u32r >> 12) / 1024.0;
    return ESP_OK;
}

esp_err_t bmp280_read_altitude(bmp280_handle_t sensor, float seaLevel, float *altitude)
{
    float pressure = 0.0;
    float temp = 0.0;
    if (bmp280_read_pressure(sensor, &temp) != ESP_OK)
    {
        return ESP_FAIL;
    }
    float atmospheric = pressure / 100.0F;
    *altitude = 44330.0 * (1.0 - pow(atmospheric / seaLevel, 0.1903));
    return ESP_OK;
}

esp_err_t bmp280_calculates_pressure(bmp280_handle_t sensor, float altitude,
                                     float atmospheric, float *pressure)
{
    *pressure = atmospheric / pow(1.0 - (altitude / 44330.0), 5.255);
    return ESP_OK;
}
