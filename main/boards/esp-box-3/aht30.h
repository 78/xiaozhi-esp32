#ifndef _AHT30_H_
#define _AHT30_H_

#include <driver/i2c_master.h>
#include <esp_err.h>

// AHT30 temperature & humidity sensor on the BOX-3 SENSOR sub-board.
// I²C 0x38, 6-byte read protocol, 80ms measurement settling time.
// Datasheet: AHT30 supersedes AHT21 with same protocol; per-unit calibration
// coefficients are baked into the ASIC, no per-board calibration needed.
//
// Usage: instantiate once with the shared sensor I²C bus handle. Call Read()
// from any task; method is not re-entrant — caller must serialize if shared
// across tasks. Read() takes ~85ms (issue command, wait, read).

class Aht30 {
public:
    explicit Aht30(i2c_master_bus_handle_t bus);
    ~Aht30();

    // Returns ESP_OK on success and writes temp_c + humidity_pct.
    // ESP_ERR_INVALID_RESPONSE if status byte indicates uncalibrated/busy
    // device, ESP_ERR_TIMEOUT or other esp_err_t on I²C transport failure.
    esp_err_t Read(float* temp_c, float* humidity_pct);

private:
    i2c_master_dev_handle_t dev_;
    bool ok_;

    esp_err_t SendCalibration();
};

#endif  // _AHT30_H_
