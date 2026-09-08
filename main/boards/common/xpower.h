#ifndef __XPOWERS_H__
#define __XPOWERS_H__

#include "i2c_device.h"

#include <esp_log.h>

// xpowerslib selects the chip at compile time via a macro: whichever XPOWERS_CHIP_*
// is defined determines which class XPowersPMU is typedef'd to. The macro only takes
// effect for the current translation unit, so it must be defined before including
// this wrapper, e.g.:
//   #define XPOWERS_CHIP_AXP2101
//   #include "xpower.h"
#if !defined(XPOWERS_CHIP_AXP192) && !defined(XPOWERS_CHIP_AXP202) && !defined(XPOWERS_CHIP_AXP2101)
#error "Define XPOWERS_CHIP_AXP192/AXP202/AXP2101 before including xpower.h, \
otherwise XPowersLib.h does not generate the XPowersPMU typedef"
#endif

#include "XPowersLib.h"

// Generic XPowers power-management chip wrapper.
//
// How it hooks into I2cDevice (single bus handle):
//   - The bus device handle is registered once by I2cDevice;
//   - The XPowers library reuses that same handle through read/write callbacks
//     instead of calling i2c_master_bus_add_device itself;
//   - The actual I2C transfers go through the inherited ReadRegs/WriteRegs.
//
// The chip is determined by the XPowersPMU typedef, so the same code works for
// AXP192 / AXP202 / AXP2101 alike. Subclasses (e.g. each board's Pmic) may still
// call the inherited ReadReg/WriteReg directly for register-level access, or use
// the public member pmu for xpowerslib's high-level API (rail voltages, charging
// current, IRQs, etc.).
class Xpowers : public I2cDevice {
public:
    Xpowers(i2c_master_bus_handle_t i2c_bus, uint8_t addr)
        : I2cDevice(i2c_bus, addr), pmu() {
        s_instances_[addr & 0x7F] = this;
        if (!pmu.begin(addr, &Xpowers::ReadCallback, &Xpowers::WriteCallback)) {
            ESP_LOGE("Xpowers", "XPowers PMU init failed at 0x%02x", addr);
        }
    }

    // Entry point to xpowerslib's high-level API: pmu.enableDC3() / pmu.setChargerConstantCurr(...) / ...
    XPowersPMU pmu;

    // The convenience methods below behave the same on AXP192 / AXP202 / AXP2101, so
    // they can be used directly; for chip-specific capabilities use pmu.<method>() instead.
    bool IsCharging() {
        return pmu.isCharging();
    }

    bool IsDischarging() {
        return pmu.isDischarge();
    }

    int GetBatteryLevel() {
        return pmu.getBatteryPercent();  // -1 if no battery connected
    }

    uint16_t GetBatteryVoltage() {
        return pmu.getBattVoltage();
    }

    bool IsBatteryConnect() {
        return pmu.isBatteryConnect();
    }

    float GetTemperature() {
        return pmu.getTemperature();
    }

    void PowerOff() {
        pmu.shutdown();
    }

private:
    // The iic_fptr_t callbacks carry no `this`, so the instance is looked up in the
    // static table by slave address.
    static int ReadCallback(uint8_t devAddr, uint8_t regAddr, uint8_t* data, uint8_t len) {
        Xpowers* self = Find(devAddr);
        if (self == nullptr) {
            return -1;
        }
        self->ReadRegs(regAddr, data, len);
        return 0;
    }

    static int WriteCallback(uint8_t devAddr, uint8_t regAddr, uint8_t* data, uint8_t len) {
        Xpowers* self = Find(devAddr);
        if (self == nullptr) {
            return -1;
        }
        self->WriteRegs(regAddr, data, len);
        return 0;
    }

    static Xpowers* Find(uint8_t devAddr) {
        return s_instances_[devAddr & 0x7F];
    }

    static inline Xpowers* s_instances_[128] = {};  // C++17 inline static member
};

#endif  // __XPOWERS_H__