#ifndef _SENSOR_TOOLS_H_
#define _SENSOR_TOOLS_H_

#include "aht30.h"
#include "radar_ms58.h"
#include "ir_driver.h"

// Registers MCP tools for the BOX-3 SENSOR sub-board:
//   self.env.temperature   → {"temp_c": float, "humidity_pct": float}
//   self.radar.presence    → {"present": bool, "last_motion_at_s": float}
//   self.ir.emit           → {"protocol": "NEC", "code": uint32}
//   self.ir.learn_start    → returns {"handle": "ir-XXXX"}; arms RX for 5s
//   self.ir.learn_result   → {"handle": str} → {"protocol": str, "code": uint32}
//                             or null when no signal yet / timed out
//
// The driver instances must outlive the registered lambdas — the Board
// class owns them as members. If the SENSOR sub-board is not physically
// attached, drivers fail their first call with an I²C / GPIO error and
// the MCP tool returns an error JSON; backend treats that as
// "no reading available" / "tool unavailable".
void InitializeSensorTools(Aht30* aht30, RadarMs58* radar, IrDriver* ir);

#endif  // _SENSOR_TOOLS_H_
