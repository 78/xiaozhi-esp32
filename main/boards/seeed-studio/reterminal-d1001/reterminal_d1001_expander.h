#ifndef XIAOZHI_BOARDS_SEEED_STUDIO_RETERMINAL_D1001_EXPANDER_H_
#define XIAOZHI_BOARDS_SEEED_STUDIO_RETERMINAL_D1001_EXPANDER_H_

#include <driver/i2c_master.h>
#include <esp_io_expander.h>
#include <mutex>

/**
 * Owns the D1001 audio-control I2C bus (I2C1) and the PCA9535 IO expander
 * that drives the board power and panel control signals.
 *
 * Only the signals needed by the current XiaoZhi port are exposed here: the
 * power hold, the panel rails, the audio power amplifier and the camera
 * module. The LTE, battery and SD related expander pins are intentionally
 * left untouched so this wrapper does not grow beyond the port scope.
 */
class ReTerminalD1001Expander {
public:
    ReTerminalD1001Expander() = default;
    ~ReTerminalD1001Expander();

    /// Create the I2C1 master bus and the PCA9535 device.
    /// On failure, prints the reason and leaves the expander disabled.
    void Initialize();

    bool IsInitialized() const { return expander_ != nullptr; }

    /// Raw access for board-level users that need the shared I2C bus
    /// (e.g. the audio codec control interface).
    i2c_master_bus_handle_t GetI2cBus() const { return i2c_bus_; }
    esp_io_expander_handle_t GetHandle() const { return expander_; }

    /// Set an output pin. Logs and ignores when the expander is unavailable.
    void SetLevel(uint32_t pin_mask, bool level);

    // Named power and panel controls.
    void SetPowerHold(bool on);
    void SetLcdPower(bool on);
    void SetLcdReset(bool asserted);
    void SetBacklightPower(bool on);
    void SetTouchReset(bool asserted);
    void SetPowerAmp(bool on);
    // Battery voltage divider gate (BAT_READ_EN, bit 6).
    void SetBatteryReadEnable(bool on);
    // Charger enable (CHG_ENBn, active low). The BQ25616 self-terminates
    // at full charge, so keeping this enabled is safe.
    void SetBatteryChargeEnable(bool on);
    void SetCameraPower(bool on);
    void SetCameraPowerDown(bool asserted);
    void SetCameraReset(bool asserted);

    /// Run the Seeed BSP camera power-up sequence: rail on, 50 ms settle,
    /// release power-down and reset, then a 10 ms reset pulse followed by a
    /// 50 ms wait. Must run before esp_video probes the sensor over SCCB.
    void PowerUpCamera();

    /// Hold the sensor in reset and cut its rail. Only safe once the video
    /// device that owns the sensor has been destroyed.
    void PowerDownCamera();

    /// Assert power-hold and park every other controlled signal in its safe
    /// state: power amplifier off, panel and backlight rails off, LCD and
    /// touch held in reset, camera rail off and held in reset. The panel
    /// rails are turned on later by PowerUpDisplayRails(), the camera by
    /// PowerUpCamera().
    void ApplyMinimalPowerSequence();

    /// Turn on the panel and backlight rails and wait for them to settle.
    /// Must run before the MIPI-DSI panel is created.
    void PowerUpDisplayRails();

    /// Drive the BSP LCD reset pulse: high 5 ms, low 10 ms, high 120 ms.
    void ResetLcdPanel();

    /// Drop power-hold so the board can power off.
    void PowerOff();

private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    esp_io_expander_handle_t expander_ = nullptr;
    mutable std::mutex mutex_;
};

#endif  // XIAOZHI_BOARDS_SEEED_STUDIO_RETERMINAL_D1001_EXPANDER_H_
