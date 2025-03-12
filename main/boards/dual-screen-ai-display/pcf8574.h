#ifndef __PCF8574_H__
#define __PCF8574_H__

#include "i2c_bus.h"

#define PCF8574_DEFAULT_ADDRESS 0x20
typedef struct
{
    i2c_bus_device_handle_t i2c_dev;
    uint8_t dev_addr;
} pcf8574_dev_t;
class PCF8574
{
private:
    pcf8574_dev_t *pcf8574_handle_;
    uint8_t _gpio = 0;

    esp_err_t readGpio();
    esp_err_t writeGpio();

public:
    PCF8574(i2c_bus_handle_t bus, uint8_t dev_addr = PCF8574_DEFAULT_ADDRESS);

    esp_err_t write(uint8_t pin, uint8_t value);
    esp_err_t read(uint8_t pin, uint8_t &value);
};

#endif