#include "axp2101.h"
#include "board.h"
#include "display.h"

#include <esp_log.h>

#define TAG "Axp2101"

Axp2101::Axp2101(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
    // The die-temperature ADC channel powers up disabled (register 0x30 bit 4), so without
    // this the register pair below reads zero no matter how often it is polled.
    //
    // Read-modify-write rather than writing 0x30 outright: a board's own init may have
    // configured the other five channels deliberately, and this helper has no business
    // undoing that.
    uint8_t adc = ReadReg(0x30);
    if ((adc & 0x10) == 0) {
        WriteReg(0x30, adc | 0x10);
    }
}

int Axp2101::GetBatteryCurrentDirection() {
    return (ReadReg(0x01) & 0b01100000) >> 5;
}

bool Axp2101::IsCharging() {
    return GetBatteryCurrentDirection() == 1;
}

bool Axp2101::IsDischarging() {
    return GetBatteryCurrentDirection() == 2;
}

bool Axp2101::IsChargingDone() {
    uint8_t value = ReadReg(0x01);
    return (value & 0b00000111) == 0b00000100;
}

int Axp2101::GetBatteryLevel() {
    return ReadReg(0xA4);
}

bool Axp2101::GetTemperature(float& celsius) {
    // Register 0xA5 is not a temperature. The die temperature is a 14-bit value in 0x3C/0x3D,
    // and the high six bits must be read before the low eight (datasheet V1.0 sections 6.10
    // and 6.13.2.38-39). Bits 7:6 of the high byte are undocumented, so they are masked off.
    uint16_t high = ReadReg(0x3C);
    uint16_t low = ReadReg(0x3D);
    uint16_t raw = (uint16_t)(((high & 0x3F) << 8) | low);

    // The conversion to Celsius is NOT in the datasheet - it is vendor convention, written
    // identically in XPowersLib and M5Unified: 7274 counts is 22 degrees, and each further
    // count is a twentieth of a degree colder. Deliberately in floating point; M5Unified does
    // this subtraction in unsigned integers, which wraps into the hundreds for every
    // temperature below 22 degrees, that is, for most rooms.
    float value = 22.0f + (7274.0f - (float)raw) / 20.0f;

    // A silent I2C failure, or an ADC channel a board has switched back off, reads as zero -
    // which converts to 386 degrees. Report nothing rather than nonsense: a caller that sees
    // false can drop the field, and a missing reading is honest where a plausible wrong one
    // is not.
    if (raw == 0 || value < -40.0f || value > 125.0f) {
        return false;
    }
    celsius = value;
    return true;
}

void Axp2101::PowerOff() {
    uint8_t value = ReadReg(0x10);
    value = value | 0x01;
    WriteReg(0x10, value);
}
