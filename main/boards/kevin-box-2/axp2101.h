#ifndef __AXP2101_H__
#define __AXP2101_H__

#include <driver/i2c_master.h>

class Axp2101 {
public:
    Axp2101() = default;
    bool Initialize(i2c_master_bus_handle_t i2c_bus, int i2c_device_address);
    bool IsCharging();
    bool IsChargingDone();
    int GetBatteryLevel();
    void PowerOff();

private:
    i2c_master_dev_handle_t i2c_device_ = nullptr;

    void WriteReg(uint8_t reg, uint8_t value);
    uint8_t ReadReg(uint8_t reg);
};

#endif
