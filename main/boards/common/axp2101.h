#ifndef __AXP2101_H__
#define __AXP2101_H__

#include "i2c_device.h"

class Axp2101 : public I2cDevice {
public:
    Axp2101(i2c_master_bus_handle_t i2c_bus, uint8_t addr);
    bool IsCharging();
    bool IsDischarging();
    bool IsChargingDone();
    int GetBatteryLevel();
    // Returns false and leaves `celsius` untouched when the reading is not usable -
    // same shape as Board::GetTemperature(float&), which is what this feeds.
    bool GetTemperature(float& celsius);
    void PowerOff();

private:
    int GetBatteryCurrentDirection();
};

#endif
