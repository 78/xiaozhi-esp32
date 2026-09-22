#ifndef CW2017_BATTERY_MONITOR_H
#define CW2017_BATTERY_MONITOR_H

#include <driver/i2c_master.h>

// CW2017 fuel-gauge helper for boards that mount the chip on the shared
// codec I2C bus (e.g. FoloToy AI Passport, address 0x63).
//
// The chip is optional: a missing device must not crash the board, so reads
// return -1 / false instead of asserting. Battery "charging" is not reported
// by CW2017 and the Passport has no charge-detect GPIO, so IsCharging() is
// always false; callers decide how to surface the unknown state.
class Cw2017BatteryMonitor {
public:
    Cw2017BatteryMonitor(i2c_master_bus_handle_t i2c_bus, uint8_t addr = 0x63);
    ~Cw2017BatteryMonitor();

    // Returns true when the chip answers on the bus (and wakes it from a
    // possible sleep/reset state). Safe to call once at board init.
    bool Initialize();

    // Battery state of charge in percent (0-100), or -1 when unavailable.
    int GetBatteryLevel();

    // Battery voltage in mV, or -1 when unavailable.
    int GetBatteryVoltageMv();

    bool IsPresent() const { return present_; }

private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t i2c_device_;
    uint8_t device_address_;
    bool present_ = false;

    int ReadReg16(uint8_t reg, uint16_t* value);
};

#endif  // CW2017_BATTERY_MONITOR_H
