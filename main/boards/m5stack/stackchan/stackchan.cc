#include "wifi_board.h"
#include "../core-s3/cores3_audio_codec.h"
#include "display/emote_display.h"
#include "display/lcd_display.h"
#include "application.h"
#include "config.h"
#include "power_save_timer.h"
#include "i2c_device.h"
#include "axp2101.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_ili9341.h>
#include <esp_timer.h>
#include "esp_video.h"
#include <cmath>           // lroundf, for the neck easing in ServoNeck::Advance
#include "feetech_scs.h"   // the MIT SCS bus driver
#include "mcp_server.h"    // the device-side tool surface
#include <atomic>          // the power-save pin, written by tool calls and read on app tasks

#define TAG "StackChanBoard"

// The MIT clean-room driver (necobit/feetech_scs_esp_idf), vendored beside this
// file with its two measured fixes. See feetech_scs_LICENSE.txt.
using ScsBus = FeetechScs;

class Pmic : public Axp2101 {
public:
    // Power Init
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        uint8_t data = ReadReg(0x90);
        data |= 0b10110100;
        WriteReg(0x90, data);
        WriteReg(0x99, (0b11110 - 5));
        WriteReg(0x97, (0b11110 - 2));
        WriteReg(0x69, 0b00110101);
        WriteReg(0x30, 0b111111);
        WriteReg(0x90, 0xBF);
        WriteReg(0x94, 33 - 5);
        WriteReg(0x95, 33 - 5);
    }

    void SetBrightness(uint8_t brightness) {
        brightness = ((brightness + 641) >> 5);
        WriteReg(0x99, brightness);
    }

    // The die temperature, read from where the die temperature actually lives.
    //
    // The shared Axp2101 helper returns the raw byte of register 0xA5, which is not a
    // temperature; on this board it reads a flat zero. So every board using that helper reports
    // a number that looks like Celsius and is not one.
    //
    // Registers, read order and scaling follow the AXP2101 datasheet V1.0, sections 6.10 and
    // 6.13.2.38-39: the high six bits must be read before the low eight, and bits 7:6 of the
    // high byte are undocumented, so they are masked off. The ADC channel itself is already
    // switched on by the constructor above - 0x30 bit 4, which powers up disabled.
    //
    // The conversion to Celsius is NOT in the datasheet. It is vendor convention, written
    // identically in XPowersLib and M5Unified, so it is followed here and labelled as
    // convention: 7274 counts is 22 degrees, and each further count is a twentieth of a degree
    // colder. Deliberately in floating point - M5Unified does this subtraction in unsigned
    // integers, which wraps into the hundreds for every temperature below 22 degrees, that is,
    // for most rooms.
    bool ReadDieTemperature(float& celsius) {
        uint16_t high = ReadReg(0x3C);
        uint16_t low = ReadReg(0x3D);
        uint16_t raw = (uint16_t)(((high & 0x3F) << 8) | low);
        float value = 22.0f + (7274.0f - (float)raw) / 20.0f;
        // A silent I2C failure reads as zero, which converts to 386 degrees. Report nothing
        // rather than nonsense: the caller then drops the field, and a missing reading is honest.
        if (raw == 0 || value < -40.0f || value > 125.0f) {
            return false;
        }
        celsius = value;
        return true;
    }
};

class CustomBacklight : public Backlight {
public:
    CustomBacklight(Pmic *pmic) : pmic_(pmic) {}

    void SetBrightnessImpl(uint8_t brightness) override {
        pmic_->SetBrightness(target_brightness_);
        brightness_ = target_brightness_;
    }

private:
    Pmic *pmic_;
};

class Aw9523 : public I2cDevice {
public:
    // Exanpd IO Init
    Aw9523(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x02, 0b00000111);  // P0
        WriteReg(0x03, 0b10001111);  // P1
        WriteReg(0x04, 0b00011000);  // CONFIG_P0
        WriteReg(0x05, 0b00001100);  // CONFIG_P1
        WriteReg(0x11, 0b00010000);  // GCR P0 port is Push-Pull mode.
        WriteReg(0x12, 0b11111111);  // LEDMODE_P0
        WriteReg(0x13, 0b11111111);  // LEDMODE_P1
    }

    void ResetAw88298() {
        ESP_LOGI(TAG, "Reset AW88298");
        WriteReg(0x02, 0b00000011);
        vTaskDelay(pdMS_TO_TICKS(10));
        WriteReg(0x02, 0b00000111);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    void ResetIli9342() {
        ESP_LOGI(TAG, "Reset IlI9342");
        WriteReg(0x03, 0b10000001);
        vTaskDelay(pdMS_TO_TICKS(20));
        WriteReg(0x03, 0b10000011);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
};


// The servo neck: two Feetech SCS0009 on a half-duplex serial bus, interpolated on this side.
//
// Non-blocking by design. StartMove stages a request and returns; a task calls Tick() every
// MOTION_TICK_MS to advance it. That matters because this robot moves its head WHILE doing other
// things - it poses before speaking, wanders when idle, and moves to the beat - so a MoveHead
// that blocked for the duration of the move would stall speech and expressions with it.
// Upstream's nearest equivalent, electron-bot's Otto::MoveServos, blocks in a vTaskDelay loop,
// which is fine for a dancing robot and not for this one.
//
// Linear advance with a cubic ease-out, and no spring: the fork's spring mode needs
// smooth_ui_toolkit (110 files, 13570 lines, a git submodule) for one interpolator, and upstream
// has no submodules. The curve below is upstream's own, from electron-bot/movements.cc.
class ServoNeck {
public:
    struct Axis {
        int target_deg = 0;
        int start_deg = 0;
        int current_deg = 0;
        uint32_t move_start_ms = 0;
        uint32_t move_duration_ms = 0;
        bool moving = false;
        // Monotonic request id. Tick() snapshots the axis, drops the mutex to use the bus, then
        // re-checks this before trusting what it computed. move_start_ms has millisecond
        // resolution, which cannot separate two rapid requests; a counter can.
        uint64_t request_token = 0;
    };

    static constexpr uint32_t MOTION_TICK_MS = 20;
    // Each WritePos carries its own small move time, so a dropped tick glides rather than steps.
    static constexpr int MOTION_PER_WRITE_TIME_MS = 30;
    static constexpr int BOOT_READPOS_MAX_ATTEMPTS = 8;
    static constexpr int BOOT_READPOS_RETRY_MS = 40;

    // Upstream's curve, from main/boards/electron-bot/movements.cc. Fast at the start, settling
    // into the target without overshooting it.
    static float EaseOutCubic(float t) {
        float inv = 1.0f - t;
        return 1.0f - inv * inv * inv;
    }

    static int YawDegToPos(int deg) {
        int pos = 460 + deg * 16 / 5;
        if (pos < 0) pos = 0;
        if (pos > 1000) pos = 1000;
        return pos;
    }

    static int PitchDegToPos(int deg) {
        // Issue #80: defence in depth - clamp at the servo-write boundary too, so interpolation
        // and any future caller cannot bypass the input-layer clamp.
        if (deg < SAFE_PITCH_MIN) deg = SAFE_PITCH_MIN;
        if (deg > SAFE_PITCH_MAX) deg = SAFE_PITCH_MAX;
        int pos = 620 + deg * 16 / 5;
        if (pos < 0) pos = 0;
        if (pos > 1000) pos = 1000;
        return pos;
    }

    bool Begin() {
        bus_mutex_ = xSemaphoreCreateMutex();
        motion_mutex_ = xSemaphoreCreateMutex();
        if (bus_mutex_ == nullptr || motion_mutex_ == nullptr) {
            ESP_LOGE(TAG_NECK, "could not create mutexes");
            return false;
        }
        bus_.begin(SERVO_UART_NUM, SERVO_BAUDRATE, SERVO_TX_PIN, SERVO_RX_PIN);
        // ACK reading stays on (SCS::Level defaults to 1): with it off, a WritePos arriving while
        // the servo is still processing the previous one is silently dropped.
        ready_ = true;

        // Hold whatever position the servos are already in, before anything else.
        //
        // The SCS0009 remembers its last commanded set-point across a reset of the ESP32 but not
        // across its own power cycle, so on boot the head can drop or snap into an end stop
        // (issues #121/#123). Reading the current position and immediately writing it back with
        // time=0 pins it. Pitch first, because pitch is the axis with a stop to snap into.
        HoldCurrent(SERVO_PITCH_ID, pitch_, &ServoNeck::PitchDegToPos, true);
        HoldCurrent(SERVO_YAW_ID, yaw_, &ServoNeck::YawDegToPos, false);
        return true;
    }

    bool ready() const { return ready_; }

    // Stage a move. Returns immediately; Tick() does the work.
    void StartMove(int yaw_deg, int pitch_deg, uint32_t duration_ms) {
        if (!ready_) {
            return;
        }
        if (pitch_deg < SAFE_PITCH_MIN) pitch_deg = SAFE_PITCH_MIN;
        if (pitch_deg > SAFE_PITCH_MAX) pitch_deg = SAFE_PITCH_MAX;
        if (duration_ms == 0) duration_ms = MOTION_TICK_MS;
        uint32_t now_ms = NowMs();
        xSemaphoreTake(motion_mutex_, portMAX_DELAY);
        uint64_t token = ++next_token_;
        Stage(yaw_, yaw_deg, now_ms, duration_ms, token);
        Stage(pitch_, pitch_deg, now_ms, duration_ms, token);
        xSemaphoreGive(motion_mutex_);
    }

    // Invalidate in-flight requests without staging a new one - used when torque is dropped or
    // the position is re-synced from the servo, neither of which goes through StartMove.
    void InvalidateRequests() {
        if (motion_mutex_ == nullptr) {
            return;
        }
        xSemaphoreTake(motion_mutex_, portMAX_DELAY);
        ++next_token_;
        yaw_.moving = false;
        pitch_.moving = false;
        xSemaphoreGive(motion_mutex_);
    }

    int yaw_deg() const { return yaw_.current_deg; }
    int pitch_deg() const { return pitch_.current_deg; }
    bool moving() const { return yaw_.moving || pitch_.moving; }

    bool SetTorque(bool on) {
        if (!ready_) {
            return false;
        }
        if (!on) {
            // Anything in flight would fight the release.
            InvalidateRequests();
        }
        xSemaphoreTake(bus_mutex_, portMAX_DELAY);
        // EnableTorque takes uint8_t, not bool - made explicit rather than left to an
        // implicit conversion.
        uint8_t flag = on ? 1 : 0;
        bool ok = bus_.EnableTorque(SERVO_YAW_ID, flag) == 0 &&
                  bus_.EnableTorque(SERVO_PITCH_ID, flag) == 0;
        xSemaphoreGive(bus_mutex_);
        if (ok) {
            torque_on_ = on;
        }
        return ok;
    }

    bool torque_on() const { return torque_on_; }

    // One step of interpolation for both axes. Call every MOTION_TICK_MS from a task.
    void Tick() {
        if (!ready_) {
            return;
        }
        uint32_t now_ms = NowMs();

        Axis yaw_local, pitch_local;
        uint64_t token;
        xSemaphoreTake(motion_mutex_, portMAX_DELAY);
        if (!yaw_.moving && !pitch_.moving) {
            xSemaphoreGive(motion_mutex_);
            return;
        }
        yaw_local = yaw_;
        pitch_local = pitch_;
        token = next_token_;
        int yaw_next = yaw_.current_deg;
        bool yaw_moving = yaw_.moving;
        Advance(yaw_local, now_ms, yaw_next, yaw_moving);
        int pitch_next = pitch_.current_deg;
        bool pitch_moving = pitch_.moving;
        Advance(pitch_local, now_ms, pitch_next, pitch_moving);
        xSemaphoreGive(motion_mutex_);

        // The bus is slow relative to the mutex, so it is used without holding motion_mutex_.
        // A request arriving in this window would make the writes below stale, which is what the
        // token re-check after them is for.
        xSemaphoreTake(bus_mutex_, portMAX_DELAY);
        if (yaw_local.moving) {
            bus_.WritePos(SERVO_YAW_ID, YawDegToPos(yaw_next), MOTION_PER_WRITE_TIME_MS, 0);
        }
        if (pitch_local.moving) {
            bus_.WritePos(SERVO_PITCH_ID, PitchDegToPos(pitch_next), MOTION_PER_WRITE_TIME_MS, 0);
        }
        xSemaphoreGive(bus_mutex_);

        xSemaphoreTake(motion_mutex_, portMAX_DELAY);
        // Only commit if this is still the request we computed from.
        if (next_token_ == token) {
            yaw_.current_deg = yaw_next;
            yaw_.moving = yaw_moving;
            pitch_.current_deg = pitch_next;
            pitch_.moving = pitch_moving;
        }
        xSemaphoreGive(motion_mutex_);
    }

    // Ask a servo where it actually is, rather than where we last told it to go. The only way
    // to tell "the head is at zero" from "the head is not plugged in": every other position
    // report on this board is the commanded value echoed back.
    //
    // Takes the same mutex the motion task does - the bus is half duplex, so a read interleaved
    // with a write returns a corrupted frame rather than an error.
    int Ping(uint8_t id) {
        if (!ready_) {
            return -1;
        }
        xSemaphoreTake(bus_mutex_, portMAX_DELAY);
        int pos = bus_.ReadPos(id);
        xSemaphoreGive(bus_mutex_);
        return pos;
    }

private:
    static uint32_t NowMs() {
        return static_cast<uint32_t>(esp_timer_get_time() / 1000);
    }

    static void Stage(Axis& axis, int target_deg, uint32_t now_ms,
                      uint32_t duration_ms, uint64_t token) {
        axis.start_deg = axis.current_deg;
        axis.target_deg = target_deg;
        axis.move_start_ms = now_ms;
        axis.move_duration_ms = duration_ms;
        axis.moving = (target_deg != axis.current_deg);
        axis.request_token = token;
    }

    // Wall-clock linear progress through the eased curve. Wall clock rather than accumulated
    // ticks so bus latency or mutex contention cannot stretch a move indefinitely.
    static void Advance(const Axis& axis, uint32_t now_ms,
                        int& next_deg, bool& still_moving) {
        if (!axis.moving) {
            return;
        }
        uint32_t elapsed = now_ms - axis.move_start_ms;
        if (axis.move_duration_ms == 0 || elapsed >= axis.move_duration_ms) {
            next_deg = axis.target_deg;
            still_moving = false;
            return;
        }
        float t = static_cast<float>(elapsed) / static_cast<float>(axis.move_duration_ms);
        float eased = EaseOutCubic(t);
        int delta = axis.target_deg - axis.start_deg;
        next_deg = axis.start_deg + static_cast<int>(lroundf(delta * eased));
    }

    // Read where an axis actually is and immediately command it to stay there.
    void HoldCurrent(uint8_t id, Axis& axis, int (*to_pos)(int), bool clamp_pitch) {
        int pos = -1;
        for (int i = 0; i < BOOT_READPOS_MAX_ATTEMPTS; ++i) {
            xSemaphoreTake(bus_mutex_, portMAX_DELAY);
            pos = bus_.ReadPos(id);
            xSemaphoreGive(bus_mutex_);
            if (pos >= 0) {
                break;
            }
            // The chip needs roughly 200 ms after power-up before it answers (issue #138).
            vTaskDelay(pdMS_TO_TICKS(BOOT_READPOS_RETRY_MS));
        }
        if (pos < 0) {
            ESP_LOGW(TAG_NECK, "servo %u did not report its position; not holding it", id);
            return;
        }
        // Convert back to degrees using the same scale the write path uses.
        int centre = clamp_pitch ? 620 : 460;
        int deg = (pos - centre) * 5 / 16;
        if (clamp_pitch) {
            if (deg < SAFE_PITCH_MIN) deg = SAFE_PITCH_MIN;
            if (deg > SAFE_PITCH_MAX) deg = SAFE_PITCH_MAX;
        }
        xSemaphoreTake(bus_mutex_, portMAX_DELAY);
        // time=0: hold it exactly here, right now.
        bus_.WritePos(id, to_pos(deg), 0, 0);
        xSemaphoreGive(bus_mutex_);
        axis.current_deg = deg;
        axis.target_deg = deg;
        axis.moving = false;
        ESP_LOGI(TAG_NECK, "servo %u held at raw %d (%d deg)", id, pos, deg);
    }

    static constexpr const char* TAG_NECK = "ServoNeck";

    ScsBus bus_;
    SemaphoreHandle_t bus_mutex_ = nullptr;
    SemaphoreHandle_t motion_mutex_ = nullptr;
    Axis yaw_;
    Axis pitch_;
    uint64_t next_token_ = 0;
    bool ready_ = false;
    bool torque_on_ = true;
};

// The PY32 companion microcontroller on the internal bus (Marco, D-440 era). It owns the
// base LED ring through its own WS2812 engine - the ESP32 writes packed RGB565 into the
// chip's LED RAM and asks it to latch - and it also provides the GPIO registers the
// diagnostic tools use. Upstream has no equivalent for either half.
//
// It adds its own I2C device at 100 kHz rather than inheriting I2cDevice, because the PY32
// wants 100 kHz while the rest of this bus runs at 400 kHz; the IDF driver carries the
// speed per device, so both live on one bus.
class Py32IoExpander {
public:
    static constexpr uint8_t  DEFAULT_ADDR = 0x6F;
    static constexpr uint32_t I2C_FREQ_HZ  = 100000;  // 100 kHz (M5 default)
    static constexpr uint8_t  REG_GPIO_O_L_PUBLIC = 0x05;  // exposed for verify

    Py32IoExpander(i2c_master_bus_handle_t i2c_bus, uint8_t addr = DEFAULT_ADDR) {
        i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address  = addr,
            .scl_speed_hz    = I2C_FREQ_HZ,
            .scl_wait_us     = 0,
            .flags           = { .disable_ack_check = 0 },
        };
        ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &cfg, &i2c_device_));
    }

    // Probe the chip. On success, returns true and (if non-null) writes the
    // version byte to out_version. Internal reads use SafeReadReg, which
    // already retries on transient I2C errors — so the chip is genuinely
    // unreachable / not yet ready when this returns false.
    bool Begin(uint8_t* out_version = nullptr) {
        uint8_t version = 0;
        if (!SafeReadReg(REG_VERSION, &version)) {
            return false;
        }
        if (version == 0x00 || version == 0xFF) {
            return false;
        }
        if (out_version != nullptr) {
            *out_version = version;
        }
        return true;
    }

    // direction: false=input, true=output. Accepts pin 0..15 (PY32 has 14
    // GPIOs; the WS2812 data line is on pin 13, in the high byte).
    bool SetDirection(uint8_t pin, bool output) {
        return WriteBitWideSafe(REG_GPIO_M_L, REG_GPIO_M_H, pin, output);
    }

    // mode: false=pull down, true=pull up. Accepts pin 0..15.
    bool SetPullMode(uint8_t pin, bool up) {
        if (up) {
            bool a = WriteBitWideSafe(REG_GPIO_PD_L, REG_GPIO_PD_H, pin, false);
            bool b = WriteBitWideSafe(REG_GPIO_PU_L, REG_GPIO_PU_H, pin, true);
            return a && b;
        } else {
            bool a = WriteBitWideSafe(REG_GPIO_PU_L, REG_GPIO_PU_H, pin, false);
            bool b = WriteBitWideSafe(REG_GPIO_PD_L, REG_GPIO_PD_H, pin, true);
            return a && b;
        }
    }

    bool DigitalWrite(uint8_t pin, bool level) {
        return WriteBitSafe(REG_GPIO_O_L, pin, level);
    }

    // Read back the current output low-byte register (pins 0..7) for
    // verification after DigitalWrite. Returns false if the read failed.
    bool ReadOutputLow(uint8_t* out) {
        return SafeReadReg(REG_GPIO_O_L, out);
    }

    // Drive mode for any pin (0..15). false=push-pull, true=open-drain.
    // The WS2812 data line on pin 13 must be push-pull.
    bool SetDriveMode(uint8_t pin, bool open_drain) {
        return WriteBitWideSafe(REG_GPIO_DRV_L, REG_GPIO_DRV_H, pin, open_drain);
    }


private:
    // Owned device handle (NOT inherited from I2cDevice — see class comment).
    // Registered at 100 kHz in the constructor so this transport runs slower
    // than the rest of the bus.
    i2c_master_dev_handle_t i2c_device_ = nullptr;

    static constexpr uint8_t REG_VERSION  = 0x02;
    static constexpr uint8_t REG_GPIO_M_L = 0x03;  // Direction (mode) low byte
    static constexpr uint8_t REG_GPIO_M_H = 0x04;  // Direction (mode) high byte
    static constexpr uint8_t REG_GPIO_O_L = 0x05;  // Output low byte
    static constexpr uint8_t REG_GPIO_O_H = 0x06;  // Output high byte
    static constexpr uint8_t REG_GPIO_PU_L = 0x09; // Pull-up low byte
    static constexpr uint8_t REG_GPIO_PU_H = 0x0A; // Pull-up high byte
    static constexpr uint8_t REG_GPIO_PD_L = 0x0B; // Pull-down low byte
    static constexpr uint8_t REG_GPIO_PD_H = 0x0C; // Pull-down high byte
    static constexpr uint8_t REG_GPIO_DRV_L = 0x13; // Drive mode low byte
    static constexpr uint8_t REG_GPIO_DRV_H = 0x14; // Drive mode high byte
    static constexpr uint8_t REG_LED_CFG       = 0x24;  // count[5:0] + latch[6]
    static constexpr uint8_t REG_LED_RAM_START = 0x30;  // 2 bytes per LED, RGB565 LE

    // I2C op transient-retry parameters. Total budget per failed op is
    // (I2C_INNER_RETRIES - 1) * I2C_RETRY_DELAY_MS, i.e. ~30 ms here.
    static constexpr int I2C_INNER_RETRIES  = 3;
    static constexpr int I2C_RETRY_DELAY_MS = 15;

    // Safe I2C read — retries up to I2C_INNER_RETRIES on transient errors,
    // logs WARN only on the final failure to keep the log readable.
    bool SafeReadReg(uint8_t reg, uint8_t* out) {
        esp_err_t err = ESP_FAIL;
        for (int i = 0; i < I2C_INNER_RETRIES; i++) {
            err = i2c_master_transmit_receive(i2c_device_, &reg, 1, out, 1, 100);
            if (err == ESP_OK) {
                return true;
            }
            if (i + 1 < I2C_INNER_RETRIES) {
                vTaskDelay(pdMS_TO_TICKS(I2C_RETRY_DELAY_MS));
            }
        }
        ESP_LOGW("Py32IoExpander", "I2C read reg 0x%02X failed after %d tries: 0x%X",
                 reg, I2C_INNER_RETRIES, err);
        return false;
    }

    // Safe I2C write — same retry semantics as SafeReadReg.
    bool SafeWriteReg(uint8_t reg, uint8_t value) {
        uint8_t buffer[2] = {reg, value};
        esp_err_t err = ESP_FAIL;
        for (int i = 0; i < I2C_INNER_RETRIES; i++) {
            err = i2c_master_transmit(i2c_device_, buffer, 2, 100);
            if (err == ESP_OK) {
                return true;
            }
            if (i + 1 < I2C_INNER_RETRIES) {
                vTaskDelay(pdMS_TO_TICKS(I2C_RETRY_DELAY_MS));
            }
        }
        ESP_LOGW("Py32IoExpander", "I2C write reg 0x%02X failed after %d tries: 0x%X",
                 reg, I2C_INNER_RETRIES, err);
        return false;
    }

    // Read-Modify-Write a single bit using safe I2C. Pin 0..7 only.
    // Returns false if either the read or the write step ultimately failed.
    bool WriteBitSafe(uint8_t reg, uint8_t pin, bool value) {
        if (pin >= 8) {
            return false;
        }
        uint8_t v = 0;
        if (!SafeReadReg(reg, &v)) {
            return false;
        }
        if (value) {
            v |= (uint8_t)(1u << pin);
        } else {
            v &= (uint8_t)~(1u << pin);
        }
        return SafeWriteReg(reg, v);
    }

    // 16-bit RMW: pin 0..7 -> reg_l, pin 8..15 -> reg_h. Used for any pin
    // beyond pin 7 (LED data line is on pin 13, so all the LED setup goes
    // through this path).
    bool WriteBitWideSafe(uint8_t reg_l, uint8_t reg_h, uint8_t pin, bool value) {
        if (pin >= 16) return false;
        uint8_t reg = (pin < 8) ? reg_l : reg_h;
        uint8_t bit = (uint8_t)(pin & 0x07);
        uint8_t v = 0;
        if (!SafeReadReg(reg, &v)) return false;
        if (value) v |= (uint8_t)(1u << bit);
        else       v &= (uint8_t)~(1u << bit);
        return SafeWriteReg(reg, v);
    }

    // Burst write: ship a pre-built {reg, ...payload} buffer in a single
    // i2c_master_transmit. Used by the LED RAM writes which would otherwise
    // require dozens of individual register writes. Same retry semantics
    // as SafeWriteReg.
    bool SafeWriteRaw(const uint8_t* buf, size_t len) {
        esp_err_t err = ESP_FAIL;
        for (int i = 0; i < I2C_INNER_RETRIES; i++) {
            err = i2c_master_transmit(i2c_device_, buf, len, 100);
            if (err == ESP_OK) {
                return true;
            }
            if (i + 1 < I2C_INNER_RETRIES) {
                vTaskDelay(pdMS_TO_TICKS(I2C_RETRY_DELAY_MS));
            }
        }
        ESP_LOGW("Py32IoExpander", "I2C raw write (len=%u) failed after %d tries: 0x%X",
                 (unsigned)len, I2C_INNER_RETRIES, err);
        return false;
    }
};


class StackChanBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Pmic* pmic_;
    Aw9523* aw9523_;
    // The PY32 on the StackChan base. Null when it did not answer; the board still boots,
    // but the neck will not move - see EnableServoPower().
    Py32IoExpander* py32_ = nullptr;
    // VM EN, the StackChan base's servo power switch, and whether it read back high.
    static constexpr uint8_t SERVO_POWER_PIN = 0;
    static constexpr uint32_t SERVO_POWER_WAKE_MS = 250;
    bool servo_power_on_ = false;
    // The power-save pin. Written by a tool call, read inside SetPowerSaveLevel, which the
    // application calls from twelve places on its own tasks.
    std::atomic<bool> power_save_pinned_{true};
    std::atomic<int> power_save_pinned_level_{(int)PowerSaveLevel::PERFORMANCE};
    // How many times the pin has overruled the application. Reported rather than logged: the
    // decline logs at debug, which the default log level strips, so without this the one
    // mechanism carried over BECAUSE it was verified by observation would be unobservable.
    std::atomic<uint32_t> power_save_declines_{0};
    // Idle shutdowns declined because the robot was on external power. Counted since boot and
    // reported in the device status, so the gate can be checked without a serial cable.
    std::atomic<uint32_t> idle_shutdown_declines_{0};
    // Logged once per idle period rather than once per second: the shutdown request repeats on
    // every tick while the robot sits idle on external power. Atomic because the decline runs on
    // the esp_timer task while the reset runs on whoever woke the device.
    std::atomic<bool> idle_shutdown_logged_{false};

    // The neck. Null when the bus did not come up: a robot that cannot turn its head should
    // still talk.
    ServoNeck* neck_ = nullptr;
    TaskHandle_t neck_task_ = nullptr;
    // Display*, not LcdDisplay*: in an emote build this holds an emote::EmoteDisplay, which is
    // not an LcdDisplay. Only ever read through GetDisplay(), and nothing LcdDisplay-specific is
    // called on it.
    Display* display_;
    EspVideo* camera_;
    PowerSaveTimer* power_save_timer_;

    void InitializePowerSaveTimer() {
        // 180 to dim, not 60. The face never stops animating - EmoteDisplay::SetPowerSaveMode is
        // an empty function - so what "no idle expression" actually meant was the backlight
        // dropping to an eighth of normal one minute in. Three minutes is longer than a pause in
        // a conversation, and the floor below is high enough to read from across a desk.
        //
        // The shutdown threshold stays at 300 s and still only fires on battery (see
        // OnShutdownRequest), so nothing about this changes when the robot switches off.
        power_save_timer_ = new PowerSaveTimer(-1, 180, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            // 35 of 100, against a normal 75. Dim enough to be obviously resting, bright enough
            // that the robot still has a face. 10 was a guess and it read as a dead screen.
            GetBacklight()->SetBrightness(35);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
            idle_shutdown_logged_.store(false, std::memory_order_relaxed);
        });
        power_save_timer_->OnShutdownRequest([this]() {
            // Only on battery. Four upstream boards gate this the same way
            // (alientek atk-dnesp32s3-box0 / box2-4g / box2-wifi / box3, each with
            // `if (power_status_ == kDeviceBatterySupply)`), and PowerSaveTimer's own default
            // for seconds_to_shutdown is -1 - upstream does not switch a board off unless the
            // board asks for it.
            //
            // The PMIC is asked here rather than relied on through GetBatteryLevel, which also
            // disables the timer when charging but only runs when something requests the device
            // status. While the gateway was attached that happened constantly; with nothing
            // polling, the gate never ran and a robot on USB power switched itself off after
            // five idle minutes. A check that only works while someone is watching is not a
            // check.
            //
            // Dimming still happens on USB power - that is what power saving should look like
            // for a robot that sits on a desk. Switching off is not power saving when the only
            // thing that can undo it is a person pressing a button.
            //
            // Said once per idle period, not once per second. PowerSaveCheck has no one-shot
            // guard and does not reset ticks_, so once the count passes seconds_to_shutdown
            // this callback is invoked on every tick. The four alientek boards avoid the
            // problem by logging nothing on this path; a count plus one line keeps the gate
            // observable without flooding the log.
            if (pmic_ == nullptr || !pmic_->IsDischarging()) {
                idle_shutdown_declines_.fetch_add(1, std::memory_order_relaxed);
                if (!idle_shutdown_logged_.exchange(true, std::memory_order_relaxed)) {
                    ESP_LOGI(TAG, "idle shutdown declined: running on external power");
                }
                return;
            }
            ESP_LOGW(TAG, "idle on battery; powering off");
            pmic_->PowerOff();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void I2cDetect() {
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(200));
                if (ret == ESP_OK) {
                    printf("%02x ", address);
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU ");
                } else {
                    printf("-- ");
                }
            }
            printf("\r\n");
        }
    }

    void InitializeAxp2101() {
        ESP_LOGI(TAG, "Init AXP2101");
        pmic_ = new Pmic(i2c_bus_, 0x34);
    }

    void InitializeAw9523() {
        ESP_LOGI(TAG, "Init AW9523");
        aw9523_ = new Aw9523(i2c_bus_, 0x58);
        vTaskDelay(pdMS_TO_TICKS(50));
    }


    // ---- device status ---------------------------------------------------------------------
    //
    // Upstream's WifiBoard fills this with audio_speaker, screen, battery, network and
    // chip.temperature - the last via Board::GetTemperature, which this board overrides, so the
    // AXP2101 die temperature is already answered here and needs no tool of its own.
    //
    // Adding readings here rather than giving each one a tool is upstream's own shape: one call
    // answers "what is going on", and the model does not have to know which reading lives behind
    // which tool name. Five other upstream boards override this the same way.
    //
    // Absent on purpose, each checked against upstream rather than discovered at compile time:
    // room_audio (TakeRoomAudio is a fork addition to audio_service, inside upstream's AudioEngine
    // rewrite), clock (upstream has no RTC driver at all and this board has not ported one),
    // display flush counters and listen_auto_stop_ms (no upstream source), and the servo
    // auto-release and wobble flags (behaviours this neck does not have - a flag describing
    // machinery that is not there would be a lie rather than a port).
    static const char* PowerSaveLevelName(PowerSaveLevel level) {
        switch (level) {
            case PowerSaveLevel::LOW_POWER:   return "low_power";
            case PowerSaveLevel::BALANCED:    return "balanced";
            case PowerSaveLevel::PERFORMANCE: return "performance";
        }
        return "unknown";
    }

    virtual std::string GetDeviceStatusJson() override {
        std::string base = WifiBoard::GetDeviceStatusJson();
        cJSON* root = cJSON_Parse(base.c_str());
        if (root == nullptr) {
            // Never observed. A parse failure must still not cost the caller the base status.
            ESP_LOGW(TAG, "could not parse base device status; returning it unextended");
            return base;
        }

        cJSON_AddNumberToObject(root, "uptime_seconds",
                                (double)(esp_timer_get_time() / 1000000));

        if (cJSON* servo = BuildServoJson()) {
            cJSON_AddItemToObject(root, "servo", servo);
        }
        cJSON_AddItemToObject(root, "audio", BuildAudioJson());
        // Onto the base status's own network object: whether the radio is being held awake is a
        // fact about the network, and the reason audio does or does not break up.
        if (cJSON* network = cJSON_GetObjectItem(root, "network")) {
            bool pin = power_save_pinned_.load(std::memory_order_acquire);
            cJSON_AddStringToObject(network, "power_save", pin
                ? PowerSaveLevelName((PowerSaveLevel)power_save_pinned_level_.load(
                      std::memory_order_acquire))
                : "auto");
            // Not cleared by reading, unlike the touch and IMU counters: this one is a
            // since-boot total, and two callers asking should get the same answer.
            cJSON_AddNumberToObject(network, "power_save_declines",
                (double)power_save_declines_.load(std::memory_order_relaxed));
        }

        // Charge-complete is distinct from "charging": the base status reports the latter, and a
        // battery sitting full on the dock reads as not charging, same as one on no power at all.
        if (pmic_ != nullptr) {
            cJSON* battery = cJSON_GetObjectItem(root, "battery");
            if (battery != nullptr) {
                cJSON_AddBoolToObject(battery, "charge_done", pmic_->IsChargingDone());
                // A since-boot total, not cleared by reading: nonzero means the robot reached
                // its idle shutdown threshold and stayed on because it was plugged in.
                cJSON_AddNumberToObject(battery, "idle_shutdown_declines",
                    (double)idle_shutdown_declines_.load(std::memory_order_relaxed));
            }
        }

        char* text = cJSON_PrintUnformatted(root);
        std::string result = text ? std::string(text) : base;
        if (text) {
            cJSON_free(text);
        }
        cJSON_Delete(root);
        return result;
    }


    // Whether the neck is holding, and where. The board has always known; until there was a way
    // to ask, a torque release issued by one process was invisible to the next - and a caller
    // that had not issued it would drive the head straight out of somebody's hand.
    cJSON* BuildServoJson() {
        if (neck_ == nullptr) {
            return nullptr;
        }
        cJSON* servo = cJSON_CreateObject();
        cJSON_AddBoolToObject(servo, "torque", neck_->torque_on());
        // False means the base never switched the servo supply on, which looks from the
        // outside exactly like a dead bus or a jammed head.
        cJSON_AddBoolToObject(servo, "power", servo_power_on_);
        cJSON_AddBoolToObject(servo, "moving", neck_->moving());
        cJSON_AddNumberToObject(servo, "yaw", neck_->yaw_deg());
        cJSON_AddNumberToObject(servo, "pitch", neck_->pitch_deg());
        return servo;
    }


    // "It did not wake up" is otherwise diagnosed by reasoning about logs after the fact.
    // AudioService has known the answer all along; nothing could ask it.
    //
    // All four come from upstream accessors (audio_service.h:125/128/131,
    // device_state_machine.h:64). The two the fork never surfaced - the VAD flag and the
    // audio-processor bit - separate "deaf" from "hearing nothing".
    cJSON* BuildAudioJson() {
        cJSON* audio = cJSON_CreateObject();
        auto& app = Application::GetInstance();
        auto& audio_service = app.GetAudioService();
        // Named for what the bit means. The fork called this one "requested" and reported a
        // second flag beside it, but IsWakeWordRunning reads an event-group bit: it is the
        // detector's actual state, not a record of what was asked for.
        cJSON_AddBoolToObject(audio, "wake_word_running", audio_service.IsWakeWordRunning());
        cJSON_AddBoolToObject(audio, "processor_running",
                              audio_service.IsAudioProcessorRunning());
        cJSON_AddBoolToObject(audio, "voice_detected", audio_service.IsVoiceDetected());
        cJSON_AddStringToObject(audio, "device_state",
                                DeviceStateMachine::GetStateName(app.GetDeviceState()));
        return audio;
    }

    // ---- MCP tools -------------------------------------------------------------------------
    //
    // Registered in upstream's shape: McpServer::GetInstance(), declarative Property ranges, and
    // std::unexpected for failures. The error channel is the point - the fork returned cJSON
    // with ok:false because the older callback signature had nowhere to put a failure, and that
    // is how a refused face reported success for weeks (D-476).
    void InitializeNeckTools() {
        auto& mcp_server = McpServer::GetInstance();


        mcp_server.AddTool(
            "self.neck.get_angles",
            "Where the head is now: yaw (horizontal, negative is left) and pitch (vertical, "
            "larger is further up), both in degrees, plus whether a move is still in progress.",
            PropertyList(),
            [this](const PropertyList&) -> ToolResult {
                if (neck_ == nullptr) {
                    return std::unexpected("the servo neck did not start; the head cannot move");
                }
                cJSON* root = cJSON_CreateObject();
                cJSON_AddNumberToObject(root, "yaw", neck_->yaw_deg());
                cJSON_AddNumberToObject(root, "pitch", neck_->pitch_deg());
                cJSON_AddBoolToObject(root, "moving", neck_->moving());
                cJSON_AddBoolToObject(root, "torque", neck_->torque_on());
                return root;
            });

        mcp_server.AddTool(
            "self.neck.set_angles",
            "Turn the head to an absolute yaw and pitch over a given time. Non-blocking: this "
            "returns as soon as the move is accepted, and the head keeps moving while the robot "
            "speaks or changes expression. Pitch is limited to the range the neck can reach "
            "without hitting a stop.",
            PropertyList({
                Property("yaw", kPropertyTypeInteger, -90, 90),
                // The same bounds the write path clamps to, declared here so an out-of-range
                // request is refused with a message instead of being silently clamped.
                Property("pitch", kPropertyTypeInteger, SAFE_PITCH_MIN, SAFE_PITCH_MAX),
                // 120 ms is about as fast as the bus and the servo will usefully go; 5 s is a
                // slow deliberate sweep. Default chosen to read as a natural turn.
                Property("duration_ms", kPropertyTypeInteger, 400, 120, 5000),
            }),
            [this](const PropertyList& properties) -> ToolResult {
                if (neck_ == nullptr) {
                    return std::unexpected("the servo neck did not start; the head cannot move");
                }
                if (!neck_->torque_on()) {
                    return std::unexpected("torque is off; enable it with self.neck.set_torque "
                                           "before asking the head to move");
                }
                int yaw = properties["yaw"].value<int>();
                int pitch = properties["pitch"].value<int>();
                int duration = properties["duration_ms"].value<int>();
                neck_->StartMove(yaw, pitch, static_cast<uint32_t>(duration));
                cJSON* root = cJSON_CreateObject();
                cJSON_AddNumberToObject(root, "yaw", yaw);
                cJSON_AddNumberToObject(root, "pitch", pitch);
                cJSON_AddNumberToObject(root, "duration_ms", duration);
                return root;
            });

        mcp_server.AddTool(
            "self.neck.set_torque",
            "Hold the head in place, or let go of it. With torque off the head can be posed by "
            "hand and will stay where it is put; any move in progress is abandoned. Turning it "
            "back on holds wherever the head currently is.",
            PropertyList({
                Property("on", kPropertyTypeBoolean),
            }),
            [this](const PropertyList& properties) -> ToolResult {
                if (neck_ == nullptr) {
                    return std::unexpected("the servo neck did not start");
                }
                bool on = properties["on"].value<bool>();
                if (!neck_->SetTorque(on)) {
                    return std::unexpected("the servos did not acknowledge the torque change");
                }
                return on;
            });
    }

    static constexpr const char* IDLE_FACE_REST = "neutral";


    // How long a posture is left alone before the wander may move off it. The companion used 20
    // seconds for the same reason: a posture wiped a second later was never read.
    static constexpr int64_t IDLE_FACE_COOLDOWN_US = 18LL * 1000000LL;
    static constexpr int64_t IDLE_FACE_MIN_GAP_US = 20LL * 1000000LL;
    static constexpr int64_t IDLE_FACE_MAX_GAP_US = 45LL * 1000000LL;

    static constexpr uint32_t IDLE_MOTION_DURATION_MS = 1400;

    // Step away from `current` by min..max degrees, staying within `span` of `centre`.
    // Direction is random when both ways fit and forced when only one does, so the head turns
    // back rather than pressing against the edge of the band.
    static int Clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }


    void InitializeServoNeck() {
        neck_ = new ServoNeck();
        if (!neck_->Begin()) {
            delete neck_;
            neck_ = nullptr;
            ESP_LOGW(TAG, "servo neck unavailable; the head will not move");
            return;
        }
        // Tick on its own task. StartMove is non-blocking precisely so that speaking, expressions
        // and touch keep running while the head is in motion.
        xTaskCreate([](void* self) {
            auto* board = static_cast<StackChanBoard*>(self);
            while (true) {
                board->neck_->Tick();
                vTaskDelay(pdMS_TO_TICKS(ServoNeck::MOTION_TICK_MS));
            }
        }, "servo_neck", 3072, this, 4, &neck_task_);
        // Now that the boot hold has secured wherever the servos were, move to the resting pose.
        neck_->StartMove(BOOT_INIT_YAW_DEG, BOOT_INIT_PITCH_DEG, 1500);
        ESP_LOGI(TAG, "servo neck ready; centring to yaw %d pitch %d",
                 BOOT_INIT_YAW_DEG, BOOT_INIT_PITCH_DEG);
        InitializeNeckTools();
    }

    void InitializePy32() {
        ESP_LOGI(TAG, "init PY32 IO expander (I2C 0x%02X)", Py32IoExpander::DEFAULT_ADDR);
        py32_ = new Py32IoExpander(i2c_bus_);
        uint8_t version = 0;
        if (!py32_->Begin(&version)) {
            delete py32_;
            py32_ = nullptr;
            // Same rule as the sensors: a missing base ring costs the robot a light, not a boot.
            ESP_LOGW(TAG, "PY32 unavailable; base LEDs and GPIO diagnostics are off");
            return;
        }
        ESP_LOGI(TAG, "PY32 ready (version 0x%02X)", version);
        EnableServoPower();
    }

    // The StackChan base gates servo power behind PY32 pin 0 - VM EN. Until this runs, the two
    // SCS0009 have no supply at all: they answer no reads, ignore every write, and can be
    // pushed around by hand because they are not holding torque. That is exactly what this
    // board did from u1 until now, and the boot warning "servo N did not report its position"
    // was it saying so every time.
    //
    // Each step is reported separately rather than left to a combined bool, because "direction
    // set but the level did not latch" and "the chip never answered" need different next moves.
    void EnableServoPower() {
        if (py32_ == nullptr) {
            return;
        }
        bool ok_dir = py32_->SetDirection(SERVO_POWER_PIN, true);
        bool ok_pull = py32_->SetPullMode(SERVO_POWER_PIN, true);
        bool ok_write = py32_->DigitalWrite(SERVO_POWER_PIN, true);
        if (!ok_dir || !ok_pull || !ok_write) {
            ESP_LOGE(TAG, "servo power: failed at %s; the head will not move",
                     !ok_dir ? "SetDirection" : (!ok_pull ? "SetPullMode" : "DigitalWrite"));
            return;
        }
        // The SCS0009 needs roughly 200 ms after its supply comes up before it answers. Waiting
        // here rather than letting the neck's own boot read fail is the whole point: that read
        // happens a few hundred milliseconds from now and is what pins the head where it
        // already is, so it must not be the one paying for the wake-up.
        vTaskDelay(pdMS_TO_TICKS(SERVO_POWER_WAKE_MS));

        // Read the output register back. The chip can acknowledge a write whose level never
        // latched, and the difference matters: one means the servos are live, the other means
        // every move from here on is silent.
        uint8_t out_low = 0;
        if (!py32_->ReadOutputLow(&out_low)) {
            ESP_LOGW(TAG, "servo power: asserted, but the readback failed - cannot confirm");
            servo_power_on_ = true;  // the writes were acknowledged; assume they took
            return;
        }
        servo_power_on_ = (out_low & (1u << SERVO_POWER_PIN)) != 0;
        if (servo_power_on_) {
            ESP_LOGI(TAG, "servo power on (VM EN high, REG_GPIO_O_L=0x%02X)", out_low);
        } else {
            ESP_LOGE(TAG, "servo power: write acknowledged but pin %d reads low "
                          "(REG_GPIO_O_L=0x%02X) - the head will not move",
                     SERVO_POWER_PIN, out_low);
        }
    }


    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_37;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_36;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeIli9342Display() {
        ESP_LOGI(TAG, "Init IlI9342");

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_3;
        io_config.dc_gpio_num = GPIO_NUM_35;
        io_config.spi_mode = 2;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);
        aw9523_->ResetIli9342();

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        // Turn the panel on here, not in the display. LcdDisplay does this itself (its
        // "Turning display on" line) and EmoteDisplay does not, so while this board created an
        // lvgl display the omission was invisible - and the moment it created an emote display
        // instead, the screen went black while SetEmotion and SetChatMessage carried on being
        // delivered perfectly. The three upstream emote boards all call it right here, before
        // the display exists, which is why it works for either one.
        esp_lcd_panel_disp_on_off(panel, true);

        // The emote build has to actually build the emote display. config.json sets
        // CONFIG_USE_EMOTE_MESSAGE_STYLE=y and this used to create a SpiLcdDisplay regardless,
        // so the firmware believed it was an emote build while running an lvgl display - and
        // because Display::MountAssets only does anything on an EmoteDisplay, the assets
        // partition was marked invalid, which in turn meant srmodels.bin was never read and the
        // device could not hear its own name. The faces and the wake word were one bug.
        //
        // This is the shape six upstream boards use (esp32-s3-box, esp32-s3-box-3, esp-vocat,
        // esp-sensairshuttle, lckfb/szpi-esp32s3, rymcu/bigsmart).
        //
        // Mind the argument order: EmoteDisplay takes the panel first, SpiLcdDisplay takes the
        // panel_io first. Both are opaque handle typedefs, so getting it wrong compiles cleanly
        // and fails on the device.
#if CONFIG_USE_EMOTE_MESSAGE_STYLE
        display_ = new emote::EmoteDisplay(panel, panel_io, DISPLAY_WIDTH, DISPLAY_HEIGHT);
#else
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
#endif
    }

     void InitializeCamera() {
        static esp_cam_ctlr_dvp_pin_config_t dvp_pin_config = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io = {
                [0] = CAMERA_PIN_D0,
                [1] = CAMERA_PIN_D1,
                [2] = CAMERA_PIN_D2,
                [3] = CAMERA_PIN_D3,
                [4] = CAMERA_PIN_D4,
                [5] = CAMERA_PIN_D5,
                [6] = CAMERA_PIN_D6,
                [7] = CAMERA_PIN_D7,
            },
            .vsync_io = CAMERA_PIN_VSYNC,
            .de_io = CAMERA_PIN_HREF,
            .pclk_io = CAMERA_PIN_PCLK,
            .xclk_io = CAMERA_PIN_XCLK,
        };

        esp_video_init_sccb_config_t sccb_config = {
            .init_sccb = false,
            .i2c_handle = i2c_bus_,
            .freq = 100000,
        };

        esp_video_init_dvp_config_t dvp_config = {
            .sccb_config = sccb_config,
            .reset_pin = CAMERA_PIN_RESET,
            .pwdn_pin = CAMERA_PIN_PWDN,
            .dvp_pin = dvp_pin_config,
            .xclk_freq = XCLK_FREQ_HZ,
        };

        esp_video_init_config_t video_config = {
            .dvp = &dvp_config,
        };

        camera_ = new EspVideo(video_config);
    }

public:
    StackChanBoard() {
        InitializePowerSaveTimer();
        InitializeI2c();
        InitializeAxp2101();
        InitializeAw9523();
        I2cDetect();
        InitializeSpi();
        InitializeIli9342Display();
        InitializeCamera();
        InitializePy32();
        InitializeServoNeck();
        GetBacklight()->RestoreBrightness();
    }

    // Was NoLed from the base class until now, so every OnStateChanged() upstream makes went
    // nowhere and the ring only lit when a tool asked it to by name.

    virtual AudioCodec* GetAudioCodec() override {
        static CoreS3AudioCodec audio_codec(i2c_bus_,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_AW88298_ADDR,
            AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }

    // Board::GetTemperature defaults to returning false, and core-s3 does not override it. This
    // board can answer, so it does - and the answer comes from 0x3C/0x3D rather than the shared
    // helper's 0xA5, which is not a temperature at all.
    virtual bool GetTemperature(float& esp32temp) override {
        if (pmic_ == nullptr) {
            return false;
        }
        return pmic_->ReadDieTemperature(esp32temp);
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        static bool last_discharging = false;
        charging = pmic_->IsCharging();
        discharging = pmic_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }

        level = pmic_->GetBatteryLevel();
        return true;
    }

    // application.cc sets the power-save level from twelve places, dropping to LOW_POWER every
    // time it goes idle, so a level set from outside survives only until the application next
    // changes state - which is what made push-frame audio stutter on this board. No upstream
    // board overrides this to hold a level.
    //
    // Pinned at PERFORMANCE by default. There is no tool to release the pin in this PR; the
    // atomics are here because the tool that sets them belongs with the rest of the device
    // controls rather than with the board bring-up.
    virtual void SetPowerSaveLevel(PowerSaveLevel level) override {
        // Two different things arrive through this one call, and they must stay separate.
        //
        // The level the application ASKED for is an activity signal: it asks for PERFORMANCE when
        // something is happening, which is the cue for the screen-dim timer. That stays driven by
        // the request, whatever the radio ends up doing - otherwise pinning the radio awake would
        // pin the screen on too, and declining a LOW_POWER request would stop the display ever
        // dimming.
        if (level != PowerSaveLevel::LOW_POWER) {
            power_save_timer_->WakeUp();
        }

        // The radio is the pin's business (wn9-29). application.cc sets the level from twelve
        // places and drops to LOW_POWER at every idle, so a level set from outside survives only
        // until the next state change unless it is held here.
        if (power_save_pinned_.load(std::memory_order_acquire)) {
            PowerSaveLevel pinned =
                (PowerSaveLevel)power_save_pinned_level_.load(std::memory_order_acquire);
            if (level != pinned) {
                // Debug rather than info: the application asks a dozen times a conversation, and
                // at info this would bury everything else in the log.
                power_save_declines_.fetch_add(1, std::memory_order_relaxed);
                ESP_LOGD(TAG, "declining power save %s; pinned at %s",
                         PowerSaveLevelName(level), PowerSaveLevelName(pinned));
                // Re-asserted, not merely refused. The companion process needed its own periodic
                // re-assert for the same reason: the level is not reliably sticky just because
                // nothing visible changed it.
                WifiBoard::SetPowerSaveLevel(pinned);
                return;
            }
        }
        WifiBoard::SetPowerSaveLevel(level);
    }

    virtual Backlight *GetBacklight() override {
        static CustomBacklight backlight(pmic_);
        return &backlight;
    }
};

DECLARE_BOARD(StackChanBoard);
