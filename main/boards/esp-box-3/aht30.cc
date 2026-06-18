#include "aht30.h"
#include "config.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

#define TAG "Aht30"

// AHT30 protocol (matches AHT21 / AHT2x family):
//   Init/calibrate:    0xBE 0x08 0x00, then wait 10ms
//   Trigger measure:   0xAC 0x33 0x00, then wait 80ms
//   Read 6 bytes:      [status, hum_h, hum_m, hum_l_temp_h, temp_m, temp_l]
//   Status bit 7: busy (1 = measurement in progress).
//   Status bit 3: calibrated (must be 1 after calibration).
//   Humidity raw    = (data[1] << 12) | (data[2] << 4) | (data[3] >> 4)
//   Humidity %      = humidity_raw / 1048576 * 100         (1048576 = 2^20)
//   Temperature raw = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5]
//   Temperature C   = temperature_raw / 1048576 * 200 - 50
static constexpr uint32_t kAhtRaw20Bit = 1u << 20;

Aht30::Aht30(i2c_master_bus_handle_t bus) : dev_(nullptr), ok_(false) {
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = SENSOR_AHT30_ADDR;
    dev_cfg.scl_speed_hz = 100000;
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &dev_);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(err));
        return;
    }
    // Calibration is one-time at boot; safe to retry on every Read but cheaper
    // to do once. SendCalibration is also called inside Read on the first
    // attempt that sees an uncalibrated status, as a defensive fallback.
    SendCalibration();
    ok_ = true;
}

Aht30::~Aht30() {
    if (dev_ != nullptr) {
        i2c_master_bus_rm_device(dev_);
    }
}

esp_err_t Aht30::SendCalibration() {
    const uint8_t cmd[3] = {0xBE, 0x08, 0x00};
    esp_err_t err = i2c_master_transmit(dev_, cmd, sizeof(cmd), 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "calibration cmd failed: %s", esp_err_to_name(err));
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}

esp_err_t Aht30::Read(float* temp_c, float* humidity_pct) {
    if (!ok_ || dev_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t trigger[3] = {0xAC, 0x33, 0x00};
    esp_err_t err = i2c_master_transmit(dev_, trigger, sizeof(trigger), 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "trigger failed: %s", esp_err_to_name(err));
        return err;
    }

    // AHT30 datasheet: 80ms typical measurement; pad slightly for noise margin.
    vTaskDelay(pdMS_TO_TICKS(85));

    uint8_t buf[6] = {0};
    err = i2c_master_receive(dev_, buf, sizeof(buf), 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "read failed: %s", esp_err_to_name(err));
        return err;
    }

    // Status bit 7 = busy; bit 3 = calibrated.
    if (buf[0] & 0x80) {
        ESP_LOGW(TAG, "device still busy: 0x%02x", buf[0]);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (!(buf[0] & 0x08)) {
        ESP_LOGW(TAG, "device uncalibrated: 0x%02x — re-running calibration", buf[0]);
        SendCalibration();
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint32_t humidity_raw = (static_cast<uint32_t>(buf[1]) << 12)
                          | (static_cast<uint32_t>(buf[2]) << 4)
                          | (static_cast<uint32_t>(buf[3]) >> 4);
    uint32_t temperature_raw = (static_cast<uint32_t>(buf[3] & 0x0F) << 16)
                             | (static_cast<uint32_t>(buf[4]) << 8)
                             | static_cast<uint32_t>(buf[5]);

    *humidity_pct = static_cast<float>(humidity_raw) / kAhtRaw20Bit * 100.0f;
    *temp_c = static_cast<float>(temperature_raw) / kAhtRaw20Bit * 200.0f - 50.0f;

    return ESP_OK;
}
