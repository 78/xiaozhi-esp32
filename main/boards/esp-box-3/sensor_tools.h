#ifndef _SENSOR_TOOLS_H_
#define _SENSOR_TOOLS_H_

#include "aht30.h"
#include "radar_ms58.h"

// Registers MCP tools for the BOX-3 SENSOR sub-board:
//   self.env.temperature → {"temp_c": float, "humidity_pct": float}
//   self.radar.presence  → {"present": bool, "last_motion_at_s": float}
//
// The driver instances must outlive the registered lambdas — the Board
// class owns them as members. If the SENSOR sub-board is not physically
// attached, both drivers will fail their first read with an I²C / GPIO
// error and the MCP tool returns an error JSON; backend treats that as
// "no reading available".
void InitializeSensorTools(Aht30* aht30, RadarMs58* radar);

#endif  // _SENSOR_TOOLS_H_
