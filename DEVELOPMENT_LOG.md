# Development Log

## 2026-03-11 — OLED controller mismatch debugging (brief)

- Symptom: On ESP-IDF builds, OLED showed "snow" / noisy pixels; Arduino `esp32-eyes` sketch worked.
- Root cause: Display controller mismatch and driver behavior differences (module behaves as SH1106-like, not plain SSD1306 path).
- What was tested:
  - SSD1306 + raw buffer writes (failed: snow)
  - SSD1306 + LVGL/esp_lcd (still incorrect)
  - SH1106 + LVGL/esp_lcd (image stable, no snow)
  - Polarity adjustment: switched scene colors to correct black/white appearance.
- Current working setup for IDF demo:
  - I2C pins: SDA=GPIO41, SCL=GPIO42
  - Controller path: SH1106
- Mark-one integration updates:
  - Added compile-time switch in `main/boards/mark-one/config.h`:
    - `DISPLAY_USE_SH1106 1` (set `0` for SSD1306)
  - Added dual init path in `mark_one.cc` (SH1106/SSD1306 + 0x3C/0x3D probing)
  - Fixed pin conflict by moving default servo pins to GPIO39/GPIO40.

## 2026-03-11 — df-s3-no-cam stabilization + OLED orientation fixes

- Created new board profile `df-s3-no-cam` (DFRobot S3 without camera) based on local hardware behavior.
- Added OLED support with controller switch and address probing:
  - SH1106/SSD1306 selectable (`DISPLAY_USE_SH1106`)
  - I2C probe at `0x3C` and `0x3D`
- Found repeated boot loop root cause: mixed flash state from app-only updates across different firmware families.
  - Resolution: performed **full flash** (bootloader + partition table + ota_data + app + assets), which restored stable boot.
- Investigated orientation/polarity issues on SH1106 path:
  - Existing `sh1106-esp-idf` mirror implementation incorrectly tied mirror-X to display reverse/normal (polarity command).
  - Patched managed component driver:
    - `panel_sh1106_invert_color()` now actually sends `A6/A7` (normal/reverse)
    - `panel_sh1106_mirror()` now uses SEG remap + COM scan commands (not polarity)
- Board init now forces normal polarity at startup.
- Orientation required extra tuning after that (left-right and up-down mirror calibration); latest adjustments were applied via `DISPLAY_MIRROR_X` / `DISPLAY_MIRROR_Y` in `df-s3-no-cam/config.h`.
- Current status: display is stable and readable; polarity issue fixed, orientation calibration in progress.

## 2026-03-12 — Provisioning reliability + startup connectivity debugging

- Symptom: Device stayed on "Initializing"; Xiaozhi AP intermittently missing after firmware iterations.
- Verified behavior via monitor logs:
  - Wi-Fi station init/scanning occurred.
  - Network-event/UI path could hit LVGL display lock failures and watchdog backtraces in `sys_evt` context.
- Actions taken:
  - Captured serial monitor traces to identify startup sequence and failure points.
  - Adjusted provisioning fallback logic in custom board profile (`df-s3-no-cam`) and iterated.
  - Temporarily tested AP-only provisioning mode, then reverted to AP+STA (required for immediate STA validation in current flow).
  - Cleared `nvs` partition multiple times to reset saved Wi-Fi credentials and force provisioning AP reappearance.
  - Reduced network-event display side effects in `application.cc` (removed non-essential Wi-Fi notification UI calls in callback path) to avoid LVGL lock contention.
- Current status:
  - Device AP provisioning works when `nvs` is reset.
  - Router now shows connected device entry (`espressif`), confirming Wi-Fi join success.
  - Remaining user-visible state likely at cloud activation/binding stage (xiaozhi.me), not local Wi-Fi bring-up.
