#include "pcf8574.h"
#include <esp_log.h>

#define TAG "PCF8574"

esp_err_t PCF8574::readGpio()
{
	return i2c_bus_read_byte(pcf8574_handle_->i2c_dev, NULL_I2C_MEM_ADDR, &_gpio);
}

esp_err_t PCF8574::writeGpio()
{
	return i2c_bus_write_byte(pcf8574_handle_->i2c_dev, NULL_I2C_MEM_ADDR, _gpio);
}

esp_err_t PCF8574::readGpio(int gpio, uint8_t &val)
{
	auto ret = i2c_bus_read_byte(pcf8574_handle_->i2c_dev, NULL_I2C_MEM_ADDR, &_gpio);
	read(gpio, val);
	return ret;
}

esp_err_t PCF8574::writeGpio(int gpio, uint8_t val)
{
	write(gpio, val);
	return i2c_bus_write_byte(pcf8574_handle_->i2c_dev, NULL_I2C_MEM_ADDR, _gpio);
}

PCF8574::PCF8574(i2c_bus_handle_t bus, uint8_t dev_addr)
{
	pcf8574_dev_t *sens = (pcf8574_dev_t *)calloc(1, sizeof(pcf8574_dev_t));
	sens->i2c_dev = i2c_bus_device_create(bus, dev_addr, i2c_bus_get_current_clk_speed(bus));
	if (sens->i2c_dev == NULL)
	{
		free(sens);
		return;
	}
	sens->dev_addr = dev_addr;
	pcf8574_handle_ = sens;
	auto ret = readGpio();
	ESP_LOGE(TAG, "pcf8574 init: %d", ret);
}

esp_err_t PCF8574::write(uint8_t pin, uint8_t value)
{
	if (pin > 7)
	{
		return ESP_FAIL;
	}
	if (value == 0)
	{
		_gpio &= ~(1 << pin);
	}
	else
	{
		_gpio |= (1 << pin);
	}
	return writeGpio();
}

esp_err_t PCF8574::read(uint8_t pin, uint8_t &value)
{
	if (pin > 7)
	{
		return ESP_FAIL;
	}
	auto ret = readGpio();
	if (ret == ESP_OK)
		value = (_gpio & (1 << pin)) > 0;

	return ret;
}