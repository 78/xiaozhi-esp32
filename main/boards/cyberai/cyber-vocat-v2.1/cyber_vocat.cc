#include "../common/low_battery_shutdown.h"
#include "application.h"
#include "backlight.h"
#include "button.h"
#include "codecs/box_audio_codec.h"
#include "config.h"
#include "display/emote_display.h"
#if CONFIG_USE_EMOTE_MESSAGE_STYLE
#include <esp_heap_caps.h>
#include "../common/dma_panel.h"
#endif
#include "display/lcd_display.h"
#include "wifi_board.h"

#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <atomic>
#include <cinttypes>
#include <memory>
#include <mutex>
#include "esp_idf_version.h"

#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st77916.h>
#include <cstdlib>
#include "i2c_bus.h"
#include "i2c_device.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "driver/temperature_sensor.h"

#define TAG "Cyber-VoCat-v2.1"

namespace Motion {
struct AccelSample {
    int x = 0;
    int y = 0;
    int z = 0;
};  // Milligravity.
static i2c_master_dev_handle_t device = nullptr;

static esp_err_t Read(uint8_t reg, uint8_t* data, size_t length) {
    return i2c_master_transmit_receive(device, &reg, 1, data, length, 50);
}
static esp_err_t Write(uint8_t reg, uint8_t value) {
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(device, data, sizeof(data), 50);
}
static int16_t Decode(const uint8_t* data) {
    return static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                                (static_cast<uint16_t>(data[1]) << 8));
}
static void Deinitialize() {
    if (device != nullptr) {
        i2c_master_bus_rm_device(device);
        device = nullptr;
    }
}
esp_err_t Initialize(i2c_master_bus_handle_t bus) {
    if (device != nullptr)
        return ESP_OK;
    i2c_device_config_t config = {};
    config.device_address = QMA6100P_I2C_ADDR;
    config.scl_speed_hz = 400000;
    esp_err_t added = i2c_master_bus_add_device(bus, &config, &device);
    if (added != ESP_OK)
        return added;
    uint8_t id = 0;
    esp_err_t ret = Read(0x00, &id, 1);
    if (ret == ESP_OK && id != 0x90)
        ret = ESP_ERR_NOT_FOUND;
    // QMA6100P: wake up and select +/-4 g, preserving unrelated register bits.
    uint8_t value = 0;
    if (ret == ESP_OK)
        ret = Read(0x11, &value, 1);
    if (ret == ESP_OK)
        ret = Write(0x11, value | 0x80);
    if (ret == ESP_OK)
        ret = Read(0x0F, &value, 1);
    if (ret == ESP_OK)
        ret = Write(0x0F, (value & 0xF0) | 0x02);
    if (ret != ESP_OK)
        Deinitialize();
    return ret;
}
bool ReadAccel(AccelSample& sample) {
    if (device == nullptr)
        return false;
    uint8_t data[6] = {};
    if (Read(0x01, data, sizeof(data)) != ESP_OK)
        return false;
    sample.x = (static_cast<int>(Decode(data)) / 4) * 1000 / 2048;
    sample.y = (static_cast<int>(Decode(data + 2)) / 4) * 1000 / 2048;
    sample.z = (static_cast<int>(Decode(data + 4)) / 4) * 1000 / 2048;
    return true;
}
}  // namespace Motion

temperature_sensor_handle_t temp_sensor = NULL;
static const st77916_lcd_init_cmd_t vendor_specific_init_yysj[] = {
    {0xF0, (uint8_t[]){0x28}, 1, 0},
    {0xF2, (uint8_t[]){0x28}, 1, 0},
    {0x73, (uint8_t[]){0xF0}, 1, 0},
    {0x7C, (uint8_t[]){0xD1}, 1, 0},
    {0x83, (uint8_t[]){0xE0}, 1, 0},
    {0x84, (uint8_t[]){0x61}, 1, 0},
    {0xF2, (uint8_t[]){0x82}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x01}, 1, 0},
    {0xF1, (uint8_t[]){0x01}, 1, 0},
    {0xB0, (uint8_t[]){0x56}, 1, 0},
    {0xB1, (uint8_t[]){0x4D}, 1, 0},
    {0xB2, (uint8_t[]){0x24}, 1, 0},
    {0xB4, (uint8_t[]){0x87}, 1, 0},
    {0xB5, (uint8_t[]){0x44}, 1, 0},
    {0xB6, (uint8_t[]){0x8B}, 1, 0},
    {0xB7, (uint8_t[]){0x40}, 1, 0},
    {0xB8, (uint8_t[]){0x86}, 1, 0},
    {0xBA, (uint8_t[]){0x00}, 1, 0},
    {0xBB, (uint8_t[]){0x08}, 1, 0},
    {0xBC, (uint8_t[]){0x08}, 1, 0},
    {0xBD, (uint8_t[]){0x00}, 1, 0},
    {0xC0, (uint8_t[]){0x80}, 1, 0},
    {0xC1, (uint8_t[]){0x10}, 1, 0},
    {0xC2, (uint8_t[]){0x37}, 1, 0},
    {0xC3, (uint8_t[]){0x80}, 1, 0},
    {0xC4, (uint8_t[]){0x10}, 1, 0},
    {0xC5, (uint8_t[]){0x37}, 1, 0},
    {0xC6, (uint8_t[]){0xA9}, 1, 0},
    {0xC7, (uint8_t[]){0x41}, 1, 0},
    {0xC8, (uint8_t[]){0x01}, 1, 0},
    {0xC9, (uint8_t[]){0xA9}, 1, 0},
    {0xCA, (uint8_t[]){0x41}, 1, 0},
    {0xCB, (uint8_t[]){0x01}, 1, 0},
    {0xD0, (uint8_t[]){0x91}, 1, 0},
    {0xD1, (uint8_t[]){0x68}, 1, 0},
    {0xD2, (uint8_t[]){0x68}, 1, 0},
    {0xF5, (uint8_t[]){0x00, 0xA5}, 2, 0},
    {0xDD, (uint8_t[]){0x4F}, 1, 0},
    {0xDE, (uint8_t[]){0x4F}, 1, 0},
    {0xF1, (uint8_t[]){0x10}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0xF0, (uint8_t[]){0x02}, 1, 0},
    {0xE0,
     (uint8_t[]){0xF0, 0x0A, 0x10, 0x09, 0x09, 0x36, 0x35, 0x33, 0x4A, 0x29, 0x15, 0x15, 0x2E,
                 0x34},
     14, 0},
    {0xE1,
     (uint8_t[]){0xF0, 0x0A, 0x0F, 0x08, 0x08, 0x05, 0x34, 0x33, 0x4A, 0x39, 0x15, 0x15, 0x2D,
                 0x33},
     14, 0},
    {0xF0, (uint8_t[]){0x10}, 1, 0},
    {0xF3, (uint8_t[]){0x10}, 1, 0},
    {0xE0, (uint8_t[]){0x07}, 1, 0},
    {0xE1, (uint8_t[]){0x00}, 1, 0},
    {0xE2, (uint8_t[]){0x00}, 1, 0},
    {0xE3, (uint8_t[]){0x00}, 1, 0},
    {0xE4, (uint8_t[]){0xE0}, 1, 0},
    {0xE5, (uint8_t[]){0x06}, 1, 0},
    {0xE6, (uint8_t[]){0x21}, 1, 0},
    {0xE7, (uint8_t[]){0x01}, 1, 0},
    {0xE8, (uint8_t[]){0x05}, 1, 0},
    {0xE9, (uint8_t[]){0x02}, 1, 0},
    {0xEA, (uint8_t[]){0xDA}, 1, 0},
    {0xEB, (uint8_t[]){0x00}, 1, 0},
    {0xEC, (uint8_t[]){0x00}, 1, 0},
    {0xED, (uint8_t[]){0x0F}, 1, 0},
    {0xEE, (uint8_t[]){0x00}, 1, 0},
    {0xEF, (uint8_t[]){0x00}, 1, 0},
    {0xF8, (uint8_t[]){0x00}, 1, 0},
    {0xF9, (uint8_t[]){0x00}, 1, 0},
    {0xFA, (uint8_t[]){0x00}, 1, 0},
    {0xFB, (uint8_t[]){0x00}, 1, 0},
    {0xFC, (uint8_t[]){0x00}, 1, 0},
    {0xFD, (uint8_t[]){0x00}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xFF, (uint8_t[]){0x00}, 1, 0},
    {0x60, (uint8_t[]){0x40}, 1, 0},
    {0x61, (uint8_t[]){0x04}, 1, 0},
    {0x62, (uint8_t[]){0x00}, 1, 0},
    {0x63, (uint8_t[]){0x42}, 1, 0},
    {0x64, (uint8_t[]){0xD9}, 1, 0},
    {0x65, (uint8_t[]){0x00}, 1, 0},
    {0x66, (uint8_t[]){0x00}, 1, 0},
    {0x67, (uint8_t[]){0x00}, 1, 0},
    {0x68, (uint8_t[]){0x00}, 1, 0},
    {0x69, (uint8_t[]){0x00}, 1, 0},
    {0x6A, (uint8_t[]){0x00}, 1, 0},
    {0x6B, (uint8_t[]){0x00}, 1, 0},
    {0x70, (uint8_t[]){0x40}, 1, 0},
    {0x71, (uint8_t[]){0x03}, 1, 0},
    {0x72, (uint8_t[]){0x00}, 1, 0},
    {0x73, (uint8_t[]){0x42}, 1, 0},
    {0x74, (uint8_t[]){0xD8}, 1, 0},
    {0x75, (uint8_t[]){0x00}, 1, 0},
    {0x76, (uint8_t[]){0x00}, 1, 0},
    {0x77, (uint8_t[]){0x00}, 1, 0},
    {0x78, (uint8_t[]){0x00}, 1, 0},
    {0x79, (uint8_t[]){0x00}, 1, 0},
    {0x7A, (uint8_t[]){0x00}, 1, 0},
    {0x7B, (uint8_t[]){0x00}, 1, 0},
    {0x80, (uint8_t[]){0x48}, 1, 0},
    {0x81, (uint8_t[]){0x00}, 1, 0},
    {0x82, (uint8_t[]){0x06}, 1, 0},
    {0x83, (uint8_t[]){0x02}, 1, 0},
    {0x84, (uint8_t[]){0xD6}, 1, 0},
    {0x85, (uint8_t[]){0x04}, 1, 0},
    {0x86, (uint8_t[]){0x00}, 1, 0},
    {0x87, (uint8_t[]){0x00}, 1, 0},
    {0x88, (uint8_t[]){0x48}, 1, 0},
    {0x89, (uint8_t[]){0x00}, 1, 0},
    {0x8A, (uint8_t[]){0x08}, 1, 0},
    {0x8B, (uint8_t[]){0x02}, 1, 0},
    {0x8C, (uint8_t[]){0xD8}, 1, 0},
    {0x8D, (uint8_t[]){0x04}, 1, 0},
    {0x8E, (uint8_t[]){0x00}, 1, 0},
    {0x8F, (uint8_t[]){0x00}, 1, 0},
    {0x90, (uint8_t[]){0x48}, 1, 0},
    {0x91, (uint8_t[]){0x00}, 1, 0},
    {0x92, (uint8_t[]){0x0A}, 1, 0},
    {0x93, (uint8_t[]){0x02}, 1, 0},
    {0x94, (uint8_t[]){0xDA}, 1, 0},
    {0x95, (uint8_t[]){0x04}, 1, 0},
    {0x96, (uint8_t[]){0x00}, 1, 0},
    {0x97, (uint8_t[]){0x00}, 1, 0},
    {0x98, (uint8_t[]){0x48}, 1, 0},
    {0x99, (uint8_t[]){0x00}, 1, 0},
    {0x9A, (uint8_t[]){0x0C}, 1, 0},
    {0x9B, (uint8_t[]){0x02}, 1, 0},
    {0x9C, (uint8_t[]){0xDC}, 1, 0},
    {0x9D, (uint8_t[]){0x04}, 1, 0},
    {0x9E, (uint8_t[]){0x00}, 1, 0},
    {0x9F, (uint8_t[]){0x00}, 1, 0},
    {0xA0, (uint8_t[]){0x48}, 1, 0},
    {0xA1, (uint8_t[]){0x00}, 1, 0},
    {0xA2, (uint8_t[]){0x05}, 1, 0},
    {0xA3, (uint8_t[]){0x02}, 1, 0},
    {0xA4, (uint8_t[]){0xD5}, 1, 0},
    {0xA5, (uint8_t[]){0x04}, 1, 0},
    {0xA6, (uint8_t[]){0x00}, 1, 0},
    {0xA7, (uint8_t[]){0x00}, 1, 0},
    {0xA8, (uint8_t[]){0x48}, 1, 0},
    {0xA9, (uint8_t[]){0x00}, 1, 0},
    {0xAA, (uint8_t[]){0x07}, 1, 0},
    {0xAB, (uint8_t[]){0x02}, 1, 0},
    {0xAC, (uint8_t[]){0xD7}, 1, 0},
    {0xAD, (uint8_t[]){0x04}, 1, 0},
    {0xAE, (uint8_t[]){0x00}, 1, 0},
    {0xAF, (uint8_t[]){0x00}, 1, 0},
    {0xB0, (uint8_t[]){0x48}, 1, 0},
    {0xB1, (uint8_t[]){0x00}, 1, 0},
    {0xB2, (uint8_t[]){0x09}, 1, 0},
    {0xB3, (uint8_t[]){0x02}, 1, 0},
    {0xB4, (uint8_t[]){0xD9}, 1, 0},
    {0xB5, (uint8_t[]){0x04}, 1, 0},
    {0xB6, (uint8_t[]){0x00}, 1, 0},
    {0xB7, (uint8_t[]){0x00}, 1, 0},
    {0xB8, (uint8_t[]){0x48}, 1, 0},
    {0xB9, (uint8_t[]){0x00}, 1, 0},
    {0xBA, (uint8_t[]){0x0B}, 1, 0},
    {0xBB, (uint8_t[]){0x02}, 1, 0},
    {0xBC, (uint8_t[]){0xDB}, 1, 0},
    {0xBD, (uint8_t[]){0x04}, 1, 0},
    {0xBE, (uint8_t[]){0x00}, 1, 0},
    {0xBF, (uint8_t[]){0x00}, 1, 0},
    {0xC0, (uint8_t[]){0x10}, 1, 0},
    {0xC1, (uint8_t[]){0x47}, 1, 0},
    {0xC2, (uint8_t[]){0x56}, 1, 0},
    {0xC3, (uint8_t[]){0x65}, 1, 0},
    {0xC4, (uint8_t[]){0x74}, 1, 0},
    {0xC5, (uint8_t[]){0x88}, 1, 0},
    {0xC6, (uint8_t[]){0x99}, 1, 0},
    {0xC7, (uint8_t[]){0x01}, 1, 0},
    {0xC8, (uint8_t[]){0xBB}, 1, 0},
    {0xC9, (uint8_t[]){0xAA}, 1, 0},
    {0xD0, (uint8_t[]){0x10}, 1, 0},
    {0xD1, (uint8_t[]){0x47}, 1, 0},
    {0xD2, (uint8_t[]){0x56}, 1, 0},
    {0xD3, (uint8_t[]){0x65}, 1, 0},
    {0xD4, (uint8_t[]){0x74}, 1, 0},
    {0xD5, (uint8_t[]){0x88}, 1, 0},
    {0xD6, (uint8_t[]){0x99}, 1, 0},
    {0xD7, (uint8_t[]){0x01}, 1, 0},
    {0xD8, (uint8_t[]){0xBB}, 1, 0},
    {0xD9, (uint8_t[]){0xAA}, 1, 0},
    {0xF3, (uint8_t[]){0x01}, 1, 0},
    {0xF0, (uint8_t[]){0x00}, 1, 0},
    {0x21, (uint8_t[]){}, 0, 0},
    {0x11, (uint8_t[]){}, 0, 0},
    {0x00, (uint8_t[]){}, 0, 120},
};
float tsens_value;
class CyberVocat;

// Battery retries run only on its worker. Touch uses one checked transfer so
// a failed sample cannot synthesize a press/release or stall other callbacks.
static esp_err_t ReadPeripheralRegs(i2c_master_dev_handle_t device, uint8_t reg, uint8_t* data,
                                    size_t length, int attempts) {
    esp_err_t error = ESP_FAIL;
    for (int attempt = 0; attempt < attempts; ++attempt) {
        error = i2c_master_transmit_receive(device, &reg, 1, data, length, 50);
        if (error == ESP_OK)
            return ESP_OK;
        if (error != ESP_ERR_TIMEOUT && error != ESP_ERR_INVALID_RESPONSE && error != ESP_FAIL)
            break;
        if (attempt + 1 < attempts)
            vTaskDelay(pdMS_TO_TICKS(10 << attempt));
    }
    return error;
}

class Charge : public I2cDevice {
public:
    static constexpr uint8_t kRegVoltage = 0x08;
    static constexpr uint8_t kRegBatteryStatus = 0x0A;
    static constexpr uint8_t kRegCurrent = 0x0C;
    static constexpr uint8_t kRegStateOfCharge = 0x2C;
    static constexpr uint8_t kRegAverageCurrent = 0x14;
    static constexpr int kChargingCurrentMa = 30;
    static constexpr uint16_t kBatteryStatusDsg = BIT0;

    Charge(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {}

    bool ReadWord(uint8_t reg, uint16_t& value) {
        uint8_t data[2] = {0};
        const esp_err_t error = ReadPeripheralRegs(i2c_device_, reg, data, sizeof(data), 3);
        if (error != ESP_OK) {
            const int64_t now = esp_timer_get_time() / 1000;
            if (now >= next_error_log_ms_) {
                ESP_LOGW(TAG, "Battery read unavailable reg=0x%02x error=%s", reg,
                         esp_err_to_name(error));
                next_error_log_ms_ = now + 10000;
            }
            return false;
        }
        value = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
        return true;
    }

    bool GetSnapshot(int& level, bool& charging, bool& discharging, int* voltage_mv = nullptr) {
        std::lock_guard<std::mutex> lock(sample_mutex_);
        const int64_t now = esp_timer_get_time() / 1000;
        if (!sample_valid_ || now - sample_at_ms_ > 10000)
            return false;
        level = level_;
        charging = charging_;
        discharging = discharging_;
        if (voltage_mv != nullptr)
            *voltage_mv = voltage_mv_;
        return true;
    }

    bool Update() {
        uint16_t level, status, average, current, voltage;
        if (!ReadWord(kRegStateOfCharge, level) || !ReadWord(kRegBatteryStatus, status) ||
            !ReadWord(kRegAverageCurrent, average) || !ReadWord(kRegCurrent, current) ||
            !ReadWord(kRegVoltage, voltage) || level > 100 || voltage < 2000 || voltage > 5000)
            return false;
        ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &tsens_value));
        std::lock_guard<std::mutex> lock(sample_mutex_);
        level_ = level;
        voltage_mv_ = voltage;
        discharging_ = (status & kBatteryStatusDsg) != 0;
        // Current is a fallback: AverageCurrent can lag charger insertion.
        charging_ = !discharging_ || static_cast<int16_t>(average) > kChargingCurrentMa ||
                    static_cast<int16_t>(current) > kChargingCurrentMa;
        sample_at_ms_ = esp_timer_get_time() / 1000;
        sample_valid_ = true;
        return true;
    }

private:
    std::mutex sample_mutex_;
    int level_ = 0;
    int voltage_mv_ = 0;
    bool charging_ = false;
    bool discharging_ = false;
    bool sample_valid_ = false;
    int64_t sample_at_ms_ = 0;
    int64_t next_error_log_ms_ = 0;
};

class Cst816s : public I2cDevice {
public:
    struct TouchPoint_t {
        int num = 0;
        int x = -1;
        int y = -1;
    };

    enum TouchEvent { TOUCH_NONE, TOUCH_PRESS, TOUCH_RELEASE, TOUCH_HOLD };

    Cst816s(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        read_buffer_ = new uint8_t[6];
        was_touched_ = false;
        press_count_ = 0;

        // Create touch interrupt semaphore
        touch_isr_mux_ = xSemaphoreCreateBinary();
        if (touch_isr_mux_ == NULL) {
            ESP_LOGE(TAG, "Failed to create touch semaphore");
        }
    }

    ~Cst816s() {
        delete[] read_buffer_;

        // Delete semaphore if it exists
        if (touch_isr_mux_ != NULL) {
            vSemaphoreDelete(touch_isr_mux_);
            touch_isr_mux_ = NULL;
        }
    }

    bool UpdateTouchPoint() {
        if (ReadPeripheralRegs(i2c_device_, 0x02, read_buffer_, 6, 1) != ESP_OK)
            return false;
        tp_.num = read_buffer_[0] & 0x0F;
        tp_.x = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2];
        tp_.y = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4];
        return true;
    }

    const TouchPoint_t& GetTouchPoint() { return tp_; }

    TouchEvent CheckTouchEvent() {
        bool is_touched = (tp_.num > 0);
        TouchEvent event = TOUCH_NONE;

        if (is_touched && !was_touched_) {
            // Press event (transition from not touched to touched)
            press_count_++;
            event = TOUCH_PRESS;
            ESP_LOGI(TAG, "TOUCH PRESS - count: %d, x: %d, y: %d", press_count_, tp_.x, tp_.y);
        } else if (!is_touched && was_touched_) {
            // Release event (transition from touched to not touched)
            event = TOUCH_RELEASE;
            ESP_LOGI(TAG, "TOUCH RELEASE - total presses: %d", press_count_);
        } else if (is_touched && was_touched_) {
            // Continuous touch (hold)
            event = TOUCH_HOLD;
            ESP_LOGD(TAG, "TOUCH HOLD - x: %d, y: %d", tp_.x, tp_.y);
        }

        // Update previous state
        was_touched_ = is_touched;
        return event;
    }

    int GetPressCount() const { return press_count_; }

    void ResetPressCount() { press_count_ = 0; }

    // Semaphore management methods
    SemaphoreHandle_t GetTouchSemaphore() { return touch_isr_mux_; }

    bool WaitForTouchEvent(TickType_t timeout = portMAX_DELAY) {
        if (touch_isr_mux_ != NULL) {
            return xSemaphoreTake(touch_isr_mux_, timeout) == pdTRUE;
        }
        return false;
    }

    void NotifyTouchEvent() {
        if (touch_isr_mux_ != NULL) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(touch_isr_mux_, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }

private:
    uint8_t* read_buffer_ = nullptr;
    TouchPoint_t tp_;

    // Touch state tracking
    bool was_touched_;
    int press_count_;

    // Touch interrupt semaphore
    SemaphoreHandle_t touch_isr_mux_;
};

class CyberVocat : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    i2c_bus_handle_t shared_i2c_bus_handle_ = nullptr;
    Cst816s* cst816s_ = nullptr;
    Charge* charge_ = nullptr;
    std::unique_ptr<Button> power_button_;
    bool power_key_armed_ = false;
    bool power_key_pressed_ = false;
    std::atomic<bool> power_off_requested_{false};
    cyberai::LowBatteryShutdown low_battery_shutdown_{
        LOW_BATTERY_SHUTDOWN_PERCENT, LOW_BATTERY_SHUTDOWN_MV, LOW_BATTERY_SHUTDOWN_CONFIRM_MS};
    TaskHandle_t power_off_task_handle_ = nullptr;
    Display* display_ = nullptr;
#if CONFIG_USE_EMOTE_MESSAGE_STYLE
    std::unique_ptr<cyberai::DmaPanel> display_dma_panel_;
#endif
    PwmBacklight* backlight_ = nullptr;
    TaskHandle_t charge_task_handle_ = nullptr;
    TaskHandle_t touch_task_handle_ = nullptr;
    TaskHandle_t imu_task_handle_ = nullptr;
    TaskHandle_t head_touch_task_handle_ = nullptr;
    esp_timer_handle_t emotion_reset_timer_ = nullptr;
    bool motion_ready_ = false;
    bool was_charging_ = false;
    bool battery_status_initialized_ = false;
    uint8_t low_battery_alert_mask_ = 0;
    int low_battery_plays_left_ = 0;
    int64_t next_low_battery_play_ms_ = 0;

    static void emotion_reset_timer_callback(void* arg) {
        auto* self = static_cast<CyberVocat*>(arg);
        if (self && self->display_ != nullptr) {
            self->display_->SetEmotion("neutral");
        }
    }

    void ShowTemporaryEmotion(const char* emotion, uint32_t duration_ms) {
        if (display_ == nullptr || emotion == nullptr) {
            return;
        }
        display_->SetEmotion(emotion);
        if (emotion_reset_timer_ != nullptr) {
            esp_timer_stop(emotion_reset_timer_);
            esp_timer_start_once(emotion_reset_timer_,
                                 static_cast<uint64_t>(duration_ms) * 1000ULL);
        }
    }

    void ShowHappyTouchFeedback() {
        static int64_t s_last_us = 0;
        constexpr int64_t kCooldownUs = 1200000;
        const int64_t now = esp_timer_get_time();
        if ((now - s_last_us) < kCooldownUs) {
            return;
        }
        s_last_us = now;
        ShowTemporaryEmotion("happy", 2000);
    }

    void PlayBatteryEmotion(const char* emotion, uint32_t duration_ms) {
#if CONFIG_USE_EMOTE_MESSAGE_STYLE
        if (display_ == nullptr || emotion == nullptr) {
            return;
        }
        if (display_->InsertAnimDialog(emotion, duration_ms)) {
            return;
        }
#endif
        ShowTemporaryEmotion(emotion, duration_ms);
    }

    void StartLowBatteryAlertSequence() {
        constexpr int kMaxPlays = 3;
        constexpr int64_t kIntervalMs = 5 * 60 * 1000;

        low_battery_plays_left_ = kMaxPlays - 1;
        next_low_battery_play_ms_ = (esp_timer_get_time() / 1000) + kIntervalMs;
        PlayBatteryEmotion("low_battery", 4000);
    }

    void HandleBatteryEmotions() {
        if (charge_ == nullptr) {
            return;
        }

        int level;
        bool charging, discharging;
        if (!charge_->GetSnapshot(level, charging, discharging))
            return;
        if (!battery_status_initialized_) {
            was_charging_ = charging;
            battery_status_initialized_ = true;
        }

        if (charging && !was_charging_) {
            PlayBatteryEmotion("battery_connected", 4000);
            low_battery_alert_mask_ = 0;
            low_battery_plays_left_ = 0;
            next_low_battery_play_ms_ = 0;
        }
        was_charging_ = charging;

        if (charging) {
            return;
        }

        if (level > 12) {
            low_battery_alert_mask_ = 0;
            low_battery_plays_left_ = 0;
            next_low_battery_play_ms_ = 0;
            return;
        }

        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (low_battery_plays_left_ > 0 && now_ms >= next_low_battery_play_ms_) {
            PlayBatteryEmotion("low_battery", 4000);
            low_battery_plays_left_--;
            if (low_battery_plays_left_ > 0) {
                next_low_battery_play_ms_ = now_ms + 5 * 60 * 1000;
            }
        }

        if (level <= 5 && (low_battery_alert_mask_ & 0x02) == 0) {
            low_battery_alert_mask_ |= 0x02;
            StartLowBatteryAlertSequence();
            return;
        }

        if (level <= 10 && (low_battery_alert_mask_ & 0x01) == 0) {
            low_battery_alert_mask_ |= 0x01;
            StartLowBatteryAlertSequence();
        }
    }

    void CheckLowBatteryShutdown(bool healthy) {
        if (power_off_requested_.load()) {
            return;
        }
        int level = 0;
        int voltage_mv = 0;
        bool charging = false;
        bool discharging = false;
        const bool valid = healthy && charge_ != nullptr &&
                           charge_->GetSnapshot(level, charging, discharging, &voltage_mv);
        if (low_battery_shutdown_.Update(valid, level, voltage_mv, charging, discharging,
                                         esp_timer_get_time() / 1000)) {
            ESP_LOGW(TAG, "Empty battery confirmed: %d%%, %d mV; requesting power off", level,
                     voltage_mv);
            RequestPowerOff();
        }
    }

    static void battery_task(void* arg) {
        auto* self = static_cast<CyberVocat*>(arg);
        while (true) {
            bool healthy = false;
            if (self != nullptr && self->charge_ != nullptr) {
                healthy = self->charge_->Update();
                self->CheckLowBatteryShutdown(healthy);
                if (healthy && !self->power_off_requested_.load())
                    self->HandleBatteryEmotions();
            }
            vTaskDelay(pdMS_TO_TICKS(healthy ? 2000 : 5000));
        }
    }

    static void imu_event_task(void* arg) {
        auto* self = static_cast<CyberVocat*>(arg);
        if (self == nullptr || !self->motion_ready_) {
            vTaskDelete(NULL);
            return;
        }

        Motion::AccelSample prev;
        Motion::AccelSample cur;
        bool has_prev = false;
        int64_t last_shake_ms = 0;
        constexpr int kShakeDeltaThreshold = 2000;
        constexpr int64_t kShakeCooldownMs = 2000;

        while (true) {
            if (Motion::ReadAccel(cur)) {
                if (has_prev) {
                    int dx = abs(static_cast<int>(cur.x) - static_cast<int>(prev.x));
                    int dy = abs(static_cast<int>(cur.y) - static_cast<int>(prev.y));
                    int dz = abs(static_cast<int>(cur.z) - static_cast<int>(prev.z));
                    int shake_score = dx + dy + dz;

                    int64_t now_ms = esp_timer_get_time() / 1000;
                    if (shake_score > kShakeDeltaThreshold &&
                        (now_ms - last_shake_ms) > kShakeCooldownMs) {
                        last_shake_ms = now_ms;
                        // "dizzy/nauseated" are not guaranteed in current assets, use supported
                        // fallback.
                        Application::GetInstance().Schedule(
                            [self]() { self->ShowTemporaryEmotion("confused", 1800); });
                    }
                }
                prev = cur;
                has_prev = true;
            }
            vTaskDelay(pdMS_TO_TICKS(80));
        }
    }

    void InitializeI2c() {
        i2c_config_t i2c_cfg = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .sda_pullup_en = true,
            .scl_pullup_en = true,
            .master =
                {
                    .clk_speed = 400000,
                },
            .clk_flags = 0,
        };
        shared_i2c_bus_handle_ = i2c_bus_create(I2C_NUM_0, &i2c_cfg);
        if (!shared_i2c_bus_handle_) {
            ESP_LOGE(TAG, "Failed to create shared I2C bus");
            ESP_ERROR_CHECK(ESP_FAIL);
        }
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0) && !CONFIG_I2C_BUS_BACKWARD_CONFIG
        i2c_bus_ = i2c_bus_get_internal_bus_handle(shared_i2c_bus_handle_);
#else
#error "ESP-VoCat board requires i2c_bus_get_internal_bus_handle() support"
#endif
        if (!i2c_bus_) {
            ESP_LOGE(TAG, "Failed to get I2C master handle");
            ESP_ERROR_CHECK(ESP_FAIL);
        }

        temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
        ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));
        ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));
    }
    static void touch_isr_callback(void* arg) {
        Cst816s* touchpad = static_cast<Cst816s*>(arg);
        if (touchpad != nullptr) {
            touchpad->NotifyTouchEvent();
        }
    }

    static void touch_event_task(void* arg) {
        Cst816s* touchpad = static_cast<Cst816s*>(arg);
        if (touchpad == nullptr) {
            ESP_LOGE(TAG, "Invalid touchpad pointer in touch_event_task");
            vTaskDelete(NULL);
            return;
        }

        while (true) {
            if (touchpad->WaitForTouchEvent()) {
                auto& app = Application::GetInstance();
                auto& board = (CyberVocat&)Board::GetInstance();

                ESP_LOGD(TAG, "Touch event, TP_PIN_NUM_INT: %d", gpio_get_level(TP_PIN_NUM_INT));
                if (!touchpad->UpdateTouchPoint())
                    continue;
                auto touch_event = touchpad->CheckTouchEvent();

                if (touch_event == Cst816s::TOUCH_RELEASE) {
                    if (app.GetDeviceState() == kDeviceStateStarting) {
                        board.EnterWifiConfigMode();
                    } else {
                        app.ToggleChatState();
                    }
                }
            }
        }
    }

    void InitializeCharge() {
        charge_ = new Charge(i2c_bus_, 0x55);
        xTaskCreatePinnedToCore(battery_task, "batteryTask", 3 * 1024, this, 6,
                                &charge_task_handle_, 0);
    }

    void InitializeCst816sTouchPad() {
        cst816s_ = new Cst816s(i2c_bus_, 0x15);

        xTaskCreatePinnedToCore(touch_event_task, "touch_task", 4 * 1024, cst816s_, 5,
                                &touch_task_handle_, 1);

        const gpio_config_t int_gpio_config = {.pin_bit_mask = (1ULL << TP_PIN_NUM_INT),
                                               .mode = GPIO_MODE_INPUT,
                                               // .intr_type = GPIO_INTR_NEGEDGE
                                               .intr_type = GPIO_INTR_ANYEDGE};
        gpio_config(&int_gpio_config);
        gpio_install_isr_service(0);
        gpio_intr_enable(TP_PIN_NUM_INT);
        gpio_isr_handler_add(TP_PIN_NUM_INT, CyberVocat::touch_isr_callback, cst816s_);
    }

    void InitializeMotion() {
        esp_err_t ret = Motion::Initialize(i2c_bus_);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Motion sensor unavailable, shake feedback disabled: %s",
                     esp_err_to_name(ret));
            return;
        }
        motion_ready_ = true;
        ESP_ERROR_CHECK(xTaskCreatePinnedToCore(imu_event_task, "motion", 4096, this, 4,
                                                &imu_task_handle_, 1) == pdPASS
                            ? ESP_OK
                            : ESP_ERR_NO_MEM);
    }

    static void head_touch_task(void* arg) {
        auto* self = static_cast<CyberVocat*>(arg);
        int stable = gpio_get_level(HEAD_TOUCH_GPIO);
        int previous = stable;
        int count = 0;
        while (true) {
            const int level = gpio_get_level(HEAD_TOUCH_GPIO);
            count = level == previous ? count + 1 : 1;
            previous = level;
            if (count >= 3) {
                count = 3;
                if (level != stable && level == HEAD_TOUCH_ACTIVE_LEVEL) {
                    Application::GetInstance().Schedule(
                        [self]() { self->ShowHappyTouchFeedback(); });
                }
                stable = level;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    void InitializeHeadTouch() {
        gpio_config_t config = {};
        config.pin_bit_mask = 1ULL << HEAD_TOUCH_GPIO;
        config.mode = GPIO_MODE_INPUT;
        ESP_ERROR_CHECK(gpio_config(&config));
        ESP_ERROR_CHECK(xTaskCreatePinnedToCore(head_touch_task, "head_touch", 3072, this, 3,
                                                &head_touch_task_handle_, 1) == pdPASS
                            ? ESP_OK
                            : ESP_ERR_NO_MEM);
    }

    static void power_off_task(void* arg) {
        ESP_LOGI(TAG, "Power off requested");
        ESP_ERROR_CHECK(gpio_set_level(PG2_HOLD_GPIO, PG2_HOLD_ACTIVE_LEVEL));
        vTaskDelay(pdMS_TO_TICKS(PG2_POWER_OFF_PULSE_MS));
        ESP_ERROR_CHECK(gpio_set_level(PG2_HOLD_GPIO, PG2_RUNNING_LEVEL));
        // A USB/debug supply may keep the MCU alive. Do not restart or repeat the pulse.
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    void RequestPowerOff() {
        if (power_off_requested_.exchange(true)) {
            return;
        }
        if (xTaskCreate(power_off_task, "power_off", 3072, nullptr, 5, &power_off_task_handle_) !=
            pdPASS) {
            power_off_requested_.store(false);
            ESP_LOGW(TAG, "Power-off task creation failed; another request can retry");
        }
    }

    void InitializePowerKey() {
        power_button_ = std::make_unique<Button>(
            PG1_POWER_KEY_GPIO, PG1_POWER_KEY_ACTIVE_LEVEL == 1, POWER_KEY_LONG_PRESS_MS);
        // The press that powered the board must be released before accepting a new press.
        power_key_armed_ = gpio_get_level(PG1_POWER_KEY_GPIO) != PG1_POWER_KEY_ACTIVE_LEVEL;
        power_button_->OnPressDown([this]() { power_key_pressed_ = power_key_armed_; });
        power_button_->OnPressUp([this]() {
            power_key_pressed_ = false;
            if (!power_key_armed_) {
                power_key_armed_ = true;
                return;
            }
        });
        power_button_->OnLongPress([this]() {
            if (power_key_armed_ && power_key_pressed_) {
                RequestPowerOff();
            }
        });
    }

    void InitializePower() {
        // Preset the latch before enabling output: v2.0 must hold high; v2.1 must stay low.
        ESP_ERROR_CHECK(gpio_set_level(PG2_HOLD_GPIO, PG2_RUNNING_LEVEL));
        gpio_config_t config = {};
        config.pin_bit_mask = 1ULL << PG2_HOLD_GPIO;
        config.mode = GPIO_MODE_OUTPUT;
        ESP_ERROR_CHECK(gpio_config(&config));

#if CONFIG_USE_EMOTE_MESSAGE_STYLE
        // Hide uninitialized LCD RAM as soon as the firmware takes over the power latch.
        ESP_ERROR_CHECK(
            gpio_set_level(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT ? 1 : 0));
        config.pin_bit_mask = 1ULL << DISPLAY_BACKLIGHT_PIN;
        ESP_ERROR_CHECK(gpio_config(&config));
#endif

        // Keep the unused 4G module and speaking indicator off in this Wi-Fi baseline.
        ESP_ERROR_CHECK(gpio_set_level(ML307_EN_PIN, 0));
        ESP_ERROR_CHECK(gpio_set_level(SPEAKING_LED_GPIO, 1));
        config.pin_bit_mask = (1ULL << ML307_EN_PIN) | (1ULL << SPEAKING_LED_GPIO);
        ESP_ERROR_CHECK(gpio_config(&config));
    }

    void InitializeSpi() {
        const spi_bus_config_t bus_config = TAIJIPI_ST77916_PANEL_BUS_QSPI_CONFIG(
            QSPI_PIN_NUM_LCD_PCLK, QSPI_PIN_NUM_LCD_DATA0, QSPI_PIN_NUM_LCD_DATA1,
            QSPI_PIN_NUM_LCD_DATA2, QSPI_PIN_NUM_LCD_DATA3, QSPI_LCD_H_RES * 80 * sizeof(uint16_t));
        ESP_ERROR_CHECK(spi_bus_initialize(QSPI_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));
    }

#if CONFIG_USE_EMOTE_MESSAGE_STYLE
    static void ClearDisplayToBlack(esp_lcd_panel_handle_t panel) {
        constexpr int lines = 8;
        auto* black = static_cast<uint16_t*>(heap_caps_calloc(
            DISPLAY_WIDTH * lines, sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
        ESP_ERROR_CHECK(black ? ESP_OK : ESP_ERR_NO_MEM);
        for (int y = 0; y < DISPLAY_HEIGHT; y += lines) {
            const int end = y + lines < DISPLAY_HEIGHT ? y + lines : DISPLAY_HEIGHT;
            ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y, DISPLAY_WIDTH, end, black));
        }
        // ST77916 sends DISPON through SPI tx_param, which drains all queued color
        // transfers first. Keep the black buffer alive until this command completes.
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
        heap_caps_free(black);
        ESP_LOGI(TAG, "Boot display cleared to black");
    }
#endif

    void InitializeSt77916Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = QSPI_PIN_NUM_LCD_CS;
        io_config.dc_gpio_num = GPIO_NUM_NC;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 32;
        io_config.lcd_param_bits = 8;
        io_config.flags.quad_mode = true;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)QSPI_LCD_HOST,
                                                 &io_config, &panel_io));
        st77916_vendor_config_t vendor_config = {
            .init_cmds = vendor_specific_init_yysj,
            .init_cmds_size = sizeof(vendor_specific_init_yysj) / sizeof(st77916_lcd_init_cmd_t),
            .flags =
                {
                    .use_qspi_interface = 1,
                },
        };
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = QSPI_PIN_NUM_LCD_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = QSPI_LCD_BIT_PER_PIXEL;
        panel_config.flags.reset_active_high = false;
        panel_config.vendor_config = &vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st77916(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
#if CONFIG_USE_EMOTE_MESSAGE_STYLE
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, false));
#else
        esp_lcd_panel_disp_on_off(panel, true);
#endif
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

#if CONFIG_USE_EMOTE_MESSAGE_STYLE
        ClearDisplayToBlack(panel);
        // Match the shared renderer's 16-row regions without changing its configuration.
        constexpr size_t dma_bytes = DISPLAY_WIDTH * 16 * sizeof(uint16_t);
        display_dma_panel_ = std::make_unique<cyberai::DmaPanel>(panel, dma_bytes);
        ESP_ERROR_CHECK(display_dma_panel_->Initialize());
        ESP_LOGI(TAG, "LCD DMA staging buffer: %zu bytes (internal)", dma_bytes);
        display_ = new emote::EmoteDisplay(display_dma_panel_.get(), panel_io, DISPLAY_WIDTH,
                                           DISPLAY_HEIGHT);
#else
        display_ = new SpiLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X,
                                     DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
#endif
        backlight_ = new PwmBacklight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        backlight_->RestoreBrightness();
    }

public:
    ~CyberVocat() {
        power_button_.reset();
        if (power_off_task_handle_ != nullptr) {
            vTaskDelete(power_off_task_handle_);
        }
        // Stop tasks
        if (charge_task_handle_ != nullptr) {
            vTaskDelete(charge_task_handle_);
        }
        if (touch_task_handle_ != nullptr) {
            vTaskDelete(touch_task_handle_);
        }
        if (imu_task_handle_ != nullptr) {
            vTaskDelete(imu_task_handle_);
        }
        if (head_touch_task_handle_ != nullptr) {
            vTaskDelete(head_touch_task_handle_);
        }
        Motion::Deinitialize();

        // Delete objects
        delete charge_;
        delete cst816s_;
        delete display_;
        // The board and its backlight live for the entire device lifetime.

        // Remove GPIO ISR handler
        gpio_isr_handler_remove(TP_PIN_NUM_INT);
        if (emotion_reset_timer_ != nullptr) {
            esp_timer_stop(emotion_reset_timer_);
            esp_timer_delete(emotion_reset_timer_);
            emotion_reset_timer_ = nullptr;
        }

        // Disable temperature sensor
        if (temp_sensor != NULL) {
            temperature_sensor_disable(temp_sensor);
            temperature_sensor_uninstall(temp_sensor);
            temp_sensor = NULL;
        }
    }

    CyberVocat() {
        const esp_timer_create_args_t emotion_timer_args = {
            .callback = &CyberVocat::emotion_reset_timer_callback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "emotion_rst",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&emotion_timer_args, &emotion_reset_timer_));

        InitializePower();
        InitializePowerKey();
        InitializeI2c();
        InitializeSpi();
        InitializeSt77916Display();
        InitializeCharge();
        InitializeCst816sTouchPad();
        InitializeMotion();
        InitializeHeadTouch();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE, AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    Cst816s* GetTouchpad() { return cst816s_; }

    virtual Backlight* GetBacklight() override { return backlight_; }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (charge_ == nullptr) {
            return false;
        }
        return charge_->GetSnapshot(level, charging, discharging);
    }
};

DECLARE_BOARD(CyberVocat);
