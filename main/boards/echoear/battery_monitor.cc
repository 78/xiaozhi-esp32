/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "i2c_bus.h"
#include "battery_monitor.h"

#define BATTERY_SHUTDOWN_SOC (1)
#define I2C_MASTER_SCL_IO          GPIO_NUM_1         /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO          GPIO_NUM_2         /*!< gpio number for I2C master data  */
static const parameter_cedv_t g_cedv = {
    .full_charge_cap = 1150,
    .design_cap = 1150,
    .reserve_cap = 0,
    .near_full = 200,
    .self_discharge_rate = 20,
    .EDV0 = 3490,
    .EDV1 = 3511,
    .EDV2 = 3535,
    .EMF = 3670,
    .C0 = 115,
    .R0 = 968,
    .T0 = 4547,
    .R1 = 4764,
    .TC = 11,
    .C1 = 0,
    .DOD0 = 4147,
    .DOD10 = 4002,
    .DOD20 = 3969,
    .DOD30 = 3938,
    .DOD40 = 3880,
    .DOD50 = 3824,
    .DOD60 = 3794,
    .DOD70 = 3753,
    .DOD80 = 3677,
    .DOD90 = 3574,
    .DOD100 = 3490,
};

// Default Gauging Config
static const gauging_config_t g_cfg = {
    .CCT = 1,
    .CSYNC = 0,
    .EDV_CMP = 0,
    .SC = 1,
    .FIXED_EDV0 = 0,
    .FCC_LIM = 1,
    .FC_FOR_VDQ = 1,
    .IGNORE_SD = 1,
    .SME0 = 0,
};

static const char *TAG = "battery_monitor";

void BatteryMonitor::check_shutdown()
{
    if (battery_status.DSG == 0) {
        return; // Battery is charging, no need to check shutdown.
    }
    if (this->getBatterySOC() <= BATTERY_SHUTDOWN_SOC) {
        ESP_LOGW(TAG, "Battery SOC is low, going to sleep");
        this->printInfo();
        if (this->shutdown_cb) {
            this->shutdown_cb();
        }
        esp_deep_sleep_start();
    }
}

void BatteryMonitor::printInfo() const
{
    battery_status_t status = {};
    bq27220_get_battery_status(bq27220Handle, &status);
    ESP_LOGI(TAG, "Battery Status - DSG: %d, SYSDWN: %d, TDA: %d, BATTPRES: %d, AUTH_GD: %d, OCVGD: %d, TCA: %d, RSVD: %d, CHGINH: %d, FC: %d, OTD: %d, OTC: %d, SLEEP: %d, OCVFAIL: %d, OCVCOMP: %d, FD: %d",
             status.DSG, status.SYSDWN, status.TDA, status.BATTPRES,
             status.AUTH_GD, status.OCVGD, status.TCA, status.RSVD,
             status.CHGINH, status.FC, status.OTD, status.OTC,
             status.SLEEP, status.OCVFAIL, status.OCVCOMP, status.FD);

    uint16_t vol = bq27220_get_voltage(bq27220Handle);
    int16_t current = bq27220_get_current(bq27220Handle);
    uint16_t rc = bq27220_get_remaining_capacity(bq27220Handle);
    uint16_t full_cap = bq27220_get_full_charge_capacity(bq27220Handle);
    uint16_t temp = bq27220_get_temperature(bq27220Handle) / 10 - 273; // Convert from 0.1K to Celsius
    uint16_t cycle_cnt = bq27220_get_cycle_count(bq27220Handle);
    uint16_t soc = bq27220_get_state_of_charge(bq27220Handle);
    int16_t avg_power = bq27220_get_average_power(bq27220Handle); // in mW
    int16_t max_load = bq27220_get_maxload_current(bq27220Handle); // in mA
    uint16_t time_to_empty = bq27220_get_time_to_empty(bq27220Handle);
    uint16_t time_to_full = bq27220_get_time_to_full(bq27220Handle);

    ESP_LOGI(TAG, "Battery Info - Vol: %dmv, Current: %dmA, Power: %dmW, Remaining Capacity: %dmAh, Full Charge Capacity: %dmAh, Temperature: %dC, Cycle Count: %d, SOC: %d%%, Max Load: %dmA, Time to empty: %dmin, Time to full: %dmin",
             vol, current, avg_power, rc, full_cap, temp, cycle_cnt, soc, max_load, time_to_empty, time_to_full);
}

BatteryMonitor::BatteryMonitor() : bq27220Handle(nullptr),
    timer(nullptr),
    status_cb(nullptr),
    shutdown_cb(nullptr) {}

BatteryMonitor::~BatteryMonitor()
{
    if (bq27220Handle) {
        bq27220_delete(bq27220Handle);
    }
}

bool BatteryMonitor::init()
{
    const i2c_config_t i2c_bus_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = 400000}, // 400kHz
        .clk_flags = 0,
    };
    i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_NUM_0, &i2c_bus_conf);

    bq27220_config_t bq27220_cfg = {
        .i2c_bus = i2c_bus,
        .cfg = &g_cfg,
        .cedv = &g_cedv, // Use default configuration.
    };

    bq27220Handle = bq27220_create(&bq27220_cfg);
    if (!bq27220Handle) {
        ESP_LOGE(TAG, "Failed to initialize BQ27220");
        i2c_bus_delete(&i2c_bus);
        return false;
    }

    getBatteryStatus(this->battery_status);
    check_shutdown();

    // create timer to monitor battery status
    timer = xTimerCreate(TAG, pdMS_TO_TICKS(1000), pdTRUE, this, monitor_period);
    // ESP_UTILS_CHECK_FALSE_RETURN(timer != nullptr, false, "Failed to create battery monitor timer");
    if (timer == nullptr) {
        ESP_LOGE(TAG, "Failed to create battery monitor timer");
        return false;
    }   

    xTimerStart(timer, 0);
    return true;
}

uint8_t BatteryMonitor::getBatterySOC() const
{
    return bq27220_get_state_of_charge(bq27220Handle);
}

uint16_t BatteryMonitor::getCapacity() const
{
    return bq27220_get_design_capacity(bq27220Handle);
}

uint16_t BatteryMonitor::getFCC() const
{
    return bq27220_get_full_charge_capacity(bq27220Handle);
}

uint16_t BatteryMonitor::getVoltage() const
{
    return bq27220_get_voltage(bq27220Handle);
}

int16_t BatteryMonitor::getCurrent() const
{
    return bq27220_get_current(bq27220Handle);
}

uint16_t BatteryMonitor::getTemperature() const
{
    uint16_t temp = bq27220_get_temperature(bq27220Handle);
    return static_cast<int16_t>(temp / 10 - 273); // Convert from 0.1K to Celsius.
}

bool BatteryMonitor::getBatteryStatus(battery_status_t &status)
{
    return bq27220_get_battery_status(bq27220Handle, &status);
}

void BatteryMonitor::monitor_period(TimerHandle_t xTimer)
{

    BatteryMonitor &bm = *static_cast<BatteryMonitor *>(pvTimerGetTimerID(xTimer));
    bm.getBatteryStatus(bm.battery_status);
    if (bm.status_cb) {
        bm.status_cb(bm.battery_status);
    }

    static uint32_t count = 0;
    if (count++ % 5 == 0) { // check every 5 seconds
        // check battery low power
        bm.check_shutdown();
        // bm.printInfo();
        if (bm.period_cb) {
            bm.period_cb();
        }
    }
}
