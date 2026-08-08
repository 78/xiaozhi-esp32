#ifndef _BODY_IO_EXPANDER_H_
#define _BODY_IO_EXPANDER_H_

#include <driver/i2c_master.h>

// PY32L020 IO expander on the StackChan body board.
// Drives the servo power rail (VM_EN) and the 12 RGB LEDs.
//
// This does not reuse I2cDevice: the PY32 is a firmware I2C slave that only
// answers at 100 kHz, and a missing body board must not abort the firmware.
class BodyIoExpander {
public:
    BodyIoExpander(i2c_master_bus_handle_t i2c_bus, uint8_t addr);
    ~BodyIoExpander();

    BodyIoExpander(const BodyIoExpander&) = delete;
    BodyIoExpander& operator=(const BodyIoExpander&) = delete;

    // The expander boots slower than the host, so probing has to be retried.
    bool WaitUntilReady(int timeout_ms = 1500);

    void SetDirection(uint8_t pin, bool output);
    void SetPullUp(uint8_t pin, bool pull_up);
    void SetOpenDrain(uint8_t pin, bool open_drain);
    void DigitalWrite(uint8_t pin, bool level);

    void SetLedCount(uint8_t count);
    void SetLedColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
    // Writes the whole ring in one transaction.
    void SetAllLedColors(uint8_t count, uint8_t r, uint8_t g, uint8_t b);
    void RefreshLeds();

private:
    esp_err_t ReadReg(uint8_t reg, uint8_t* value);
    esp_err_t WriteReg(uint8_t reg, uint8_t value);
    esp_err_t WriteRegs(uint8_t reg, const uint8_t* data, size_t length);
    void WriteBit(uint8_t reg_low, uint8_t reg_high, uint8_t pin, bool value);

    i2c_master_dev_handle_t i2c_device_ = nullptr;
    uint8_t address_;
};

#endif // _BODY_IO_EXPANDER_H_
