#include "body_io_expander.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstring>

#define TAG "BodyIoExpander"

namespace {

constexpr uint8_t kRegVersion = 0x02;
constexpr uint8_t kRegGpioModeLow = 0x03;
constexpr uint8_t kRegGpioModeHigh = 0x04;
constexpr uint8_t kRegGpioOutLow = 0x05;
constexpr uint8_t kRegGpioOutHigh = 0x06;
constexpr uint8_t kRegGpioPullUpLow = 0x09;
constexpr uint8_t kRegGpioPullUpHigh = 0x0A;
constexpr uint8_t kRegGpioPullDownLow = 0x0B;
constexpr uint8_t kRegGpioPullDownHigh = 0x0C;
constexpr uint8_t kRegGpioDriveLow = 0x13;
constexpr uint8_t kRegGpioDriveHigh = 0x14;
constexpr uint8_t kRegLedConfig = 0x24;
constexpr uint8_t kRegLedRamStart = 0x30;

constexpr uint8_t kLedRefreshBit = 1 << 6;
constexpr uint8_t kMaxLedCount = 32;
constexpr uint32_t kI2cSpeedHz = 100 * 1000;
constexpr int kI2cTimeoutMs = 100;

}  // namespace

BodyIoExpander::BodyIoExpander(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : address_(addr) {
    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = kI2cSpeedHz,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0,
        },
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &device_config, &i2c_device_));
}

BodyIoExpander::~BodyIoExpander() {
    if (i2c_device_ != nullptr) {
        i2c_master_bus_rm_device(i2c_device_);
    }
}

esp_err_t BodyIoExpander::ReadReg(uint8_t reg, uint8_t* value) {
    return i2c_master_transmit_receive(i2c_device_, &reg, 1, value, 1, kI2cTimeoutMs);
}

esp_err_t BodyIoExpander::WriteReg(uint8_t reg, uint8_t value) {
    uint8_t buffer[2] = {reg, value};
    return i2c_master_transmit(i2c_device_, buffer, sizeof(buffer), kI2cTimeoutMs);
}

esp_err_t BodyIoExpander::WriteRegs(uint8_t reg, const uint8_t* data, size_t length) {
    uint8_t buffer[1 + kMaxLedCount * 2];
    if (length > sizeof(buffer) - 1) {
        length = sizeof(buffer) - 1;
    }
    buffer[0] = reg;
    memcpy(buffer + 1, data, length);
    return i2c_master_transmit(i2c_device_, buffer, length + 1, kI2cTimeoutMs);
}

bool BodyIoExpander::WaitUntilReady(int timeout_ms) {
    const int kPollIntervalMs = 100;
    for (int elapsed = 0; elapsed < timeout_ms; elapsed += kPollIntervalMs) {
        uint8_t version = 0;
        if (ReadReg(kRegVersion, &version) == ESP_OK && version != 0 && version != 0xFF) {
            ESP_LOGI(TAG, "Body IO expander ready after %d ms, version 0x%02X", elapsed, version);
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(kPollIntervalMs));
    }
    ESP_LOGE(TAG, "Body IO expander not responding at 0x%02X", address_);
    return false;
}

void BodyIoExpander::WriteBit(uint8_t reg_low, uint8_t reg_high, uint8_t pin, bool value) {
    uint8_t reg = pin < 8 ? reg_low : reg_high;
    uint8_t mask = 1 << (pin < 8 ? pin : pin - 8);
    uint8_t current = 0;
    esp_err_t err = ReadReg(reg, &current);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Read reg 0x%02X failed: %s", reg, esp_err_to_name(err));
        return;
    }
    uint8_t updated = value ? (current | mask) : (current & ~mask);
    if (updated != current) {
        WriteReg(reg, updated);
    }
}

void BodyIoExpander::SetDirection(uint8_t pin, bool output) {
    WriteBit(kRegGpioModeLow, kRegGpioModeHigh, pin, output);
}

void BodyIoExpander::SetPullUp(uint8_t pin, bool pull_up) {
    if (pull_up) {
        WriteBit(kRegGpioPullDownLow, kRegGpioPullDownHigh, pin, false);
        WriteBit(kRegGpioPullUpLow, kRegGpioPullUpHigh, pin, true);
    } else {
        WriteBit(kRegGpioPullUpLow, kRegGpioPullUpHigh, pin, false);
        WriteBit(kRegGpioPullDownLow, kRegGpioPullDownHigh, pin, true);
    }
}

void BodyIoExpander::SetOpenDrain(uint8_t pin, bool open_drain) {
    WriteBit(kRegGpioDriveLow, kRegGpioDriveHigh, pin, open_drain);
}

void BodyIoExpander::DigitalWrite(uint8_t pin, bool level) {
    WriteBit(kRegGpioOutLow, kRegGpioOutHigh, pin, level);
}

void BodyIoExpander::SetLedCount(uint8_t count) {
    if (count > kMaxLedCount) {
        count = kMaxLedCount;
    }
    WriteReg(kRegLedConfig, count & 0x3F);
}

void BodyIoExpander::SetLedColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= kMaxLedCount) {
        return;
    }
    uint16_t color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    uint8_t data[2] = {static_cast<uint8_t>(color565 & 0xFF), static_cast<uint8_t>(color565 >> 8)};
    WriteRegs(kRegLedRamStart + index * 2, data, sizeof(data));
}

void BodyIoExpander::SetAllLedColors(uint8_t count, uint8_t r, uint8_t g, uint8_t b) {
    if (count > kMaxLedCount) {
        count = kMaxLedCount;
    }
    uint16_t color565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    uint8_t data[kMaxLedCount * 2];
    for (uint8_t i = 0; i < count; i++) {
        data[i * 2] = color565 & 0xFF;
        data[i * 2 + 1] = color565 >> 8;
    }
    WriteRegs(kRegLedRamStart, data, count * 2);
}

void BodyIoExpander::RefreshLeds() {
    uint8_t config = 0;
    if (ReadReg(kRegLedConfig, &config) == ESP_OK) {
        WriteReg(kRegLedConfig, config | kLedRefreshBit);
    }
}
