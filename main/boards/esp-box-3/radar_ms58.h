#ifndef _RADAR_MS58_H_
#define _RADAR_MS58_H_

#include <driver/gpio.h>
#include <esp_err.h>

// MS58-3909S68U4-3V3-G-NLS-IIC mmWave radar on the BOX-3 SENSOR sub-board.
//
// The module exposes two interfaces:
//   1. A digital OUT pin (RI_OUT, GPIO 21) that goes HIGH when presence
//      is detected — fast and protocol-free.
//   2. An I²C interface (shared SDA/SCL with the AHT30) that lets you
//      reconfigure detection threshold / dwell time and read additional
//      diagnostics. NOT used in v1 — keep it simple.
//
// We track "last motion at" by sampling the OUT pin in the read call and
// also (when wired) via a GPIO interrupt — but the interrupt path is
// optional; the basic Read() is sufficient for "is someone in the room
// right now" queries.

class RadarMs58 {
public:
    RadarMs58();
    ~RadarMs58();

    // Returns ESP_OK + writes present + last_motion_at_ms.
    // last_motion_at_ms is the millisecond timestamp (esp_timer_get_time / 1000)
    // of the last time the OUT pin was observed HIGH; -1 if never observed.
    esp_err_t Read(bool* present, int64_t* last_motion_at_ms);

private:
    gpio_num_t out_pin_;
    int64_t last_motion_at_ms_;  // -1 = never
    bool ok_;
};

#endif  // _RADAR_MS58_H_
