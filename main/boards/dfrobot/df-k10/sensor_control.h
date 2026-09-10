#ifndef SENSOR_CONTROL_H
#define SENSOR_CONTROL_H

#include <driver/i2c_master.h>

// Registers the self.sense MCP tool for the K10's onboard environmental
// sensors: AHT20 (temperature/humidity), LTR303-ALS (ambient light) and
// SC7A20H (accelerometer). Call once at startup, after the I2C bus used by
// i2c_bus has been initialized.
void InitializeSensorTool(i2c_master_bus_handle_t i2c_bus);

#endif  // SENSOR_CONTROL_H
