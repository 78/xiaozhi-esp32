#ifndef __SY6970_H__
#define __SY6970_H__

#include "i2c_device.h"

class Sy6970 : public I2cDevice {
public:
    Sy6970(i2c_master_bus_handle_t i2c_bus, uint8_t addr);
    bool IsCharging();
    bool IsPowerGood();
    bool IsChargingDone();
    int GetBatteryLevel();
    void PowerOff();

private:
    int GetChangingStatus();
    int GetBatteryVoltage();
    int GetChargeTargetVoltage();
};

#endif