#ifndef MAIN_BOARDS_DF_K10_K10_I2C_DEVICE_H_
#define MAIN_BOARDS_DF_K10_K10_I2C_DEVICE_H_

#include <driver/i2c_master.h>
#include "i2c_device.h"

class K10I2cDevice: public I2cDevice {
 public:
   K10I2cDevice(i2c_master_bus_handle_t i2c_bus, uint8_t addr);

 protected:
    i2c_master_dev_handle_t i2c_device_;

    void WriteValue(uint8_t value);
    void WriteValues(uint8_t* buffer, size_t length);
    uint8_t ReadValue();
    void ReadValues(uint8_t* buffer, size_t length);
};

#endif  // MAIN_BOARDS_DF_K10_K10_I2C_DEVICE_H_
