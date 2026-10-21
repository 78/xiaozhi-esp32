#ifndef RETERMINAL_D1001_CAMERA_TUNING_H
#define RETERMINAL_D1001_CAMERA_TUNING_H

// Manual camera tuning for boards where the closed-source esp_ipa pipeline
// cannot run (ESP32-P4 < v3.0 has no Zb extension, so the IDF6 esp_ipa libs
// crash): no AE/AWB is available, and the sensor's default exposure/gain is
// far too dark indoors. These helpers program static values instead:
//   - exposure/analog gain: sensor controls on the CSI capture device
//   - red/blue channel gains: ISP controls on the ISP video device
struct CameraTuning {
    int exposure_pct = -1;  // 0..100, percent of the sensor's max exposure
    int gain_index = -1;    // index into the sensor's absolute gain table
    int red_milli = -1;     // red balance gain x1000 (1000 = 1.0)
    int blue_milli = -1;    // blue balance gain x1000
};

// Applies the fields that are >= 0; returns false if every attempted control
// failed. Safe to call any time after the camera has been initialized.
bool ApplyCameraTuning(const CameraTuning& tuning);

#endif  // RETERMINAL_D1001_CAMERA_TUNING_H
