/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#ifndef _BATTERY_MONITOR_H_
#define _BATTERY_MONITOR_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "bq27220.h"

#ifdef __cplusplus
#include <functional>
#include <cstdint>

/**
 * @brief BatteryMonitor class for managing battery-related operations.
 */
class BatteryMonitor {
public:
    /**
     * @brief Constructor for BatteryMonitor.
     */
    BatteryMonitor();

    /**
     * @brief Destructor for BatteryMonitor.
     */
    ~BatteryMonitor();

    /**
     * @brief Initialize the battery monitor.
     * @return esp_err_t ESP_OK on success, or an error code on failure.
     */
    bool init();

    /**
     * @brief Get the current battery level.
     * @return uint8_t Battery level percentage (0-100).
     */
    uint8_t getBatterySOC() const;

    /**
     * @brief Get the current battery voltage.
     * @return uint16_t Battery voltage in millivolts.
     */
    uint16_t getVoltage() const;

    /**
     * @brief Get the current battery current.
     * @return int16_t Battery current in milliamperes. negative value indicates discharging.
     */
    int16_t getCurrent() const;

    /**
     * @brief Get the current battery temperature.
     * @return int16_t Battery temperature in Celsius.
     */
    uint16_t getTemperature() const;

    /**
     * @brief Get the battery design capacity.
     * @return uint16_t
     */
    uint16_t getCapacity() const;

    /**
     * @brief Get the Full Charge Capacity (FCC).
     * @return uint16_t Full Charge Capacity in milliamperes.
     */
    uint16_t getFCC() const;

    /**
     * @brief Get the battery charge status.
     *
     * @return
     *  - true if the battery is charging.
     *  - false if the battery is not charging.
     */
    bool is_charging() const
    {
        return battery_status.DSG == 0;
    }

    /**
     * @brief Check if the battery is charging.
     * @return bool True if charging, false otherwise.
     */
    bool getBatteryStatus(battery_status_t &status);

    /**
     * @brief Print battery information.
     *
     */
    void printInfo() const;

    /**
     * @brief Set the Battery Status Callback object
     *
     * @param callback
     */
    void setBatteryStatusCallback(std::function<void(const battery_status_t &)> callback)
    {
        status_cb = callback;
    }

    void setBatteryShutdownCallback(std::function<void(void)> callback)
    {
        shutdown_cb = callback;
    }

    void setMonitorPeriodCallback(std::function<void(void)> callback)
    {
        period_cb = callback;
    }

    bq27220_handle_t getHandle() const
    {
        return bq27220Handle;
    }

private:
    // Add private members for battery monitoring implementation.
    bq27220_handle_t bq27220Handle; // Handle for the BQ27220 driver.
    TimerHandle_t timer;
    battery_status_t battery_status; // Current battery status.
    static void monitor_period(TimerHandle_t xTimer);
    void check_shutdown(void);
    std::function<void(const battery_status_t &)> status_cb; // Callback for battery status updates.
    std::function<void(void)> shutdown_cb; // Callback for shutdown event.
    std::function<void(void)> period_cb; // Callback for monitor period updates.
};

#endif // __cplusplus

#endif // _BATTERY_MONITOR_H_
