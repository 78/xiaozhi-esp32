# Common Problems and Fixes

This document summarizes the main issues we fixed during the ESP32-C3 Super Mini board support work.

## 1. Wi-Fi AP not visible / STA not stable on ESP32-C3 Super Mini

### Problem
- The device built and flashed successfully, but the Wi-Fi configuration AP did not appear reliably.
- Station mode also struggled to connect or maintain a stable link.

### Root cause
- The ESP32-C3 Super Mini hardware required lower Wi-Fi TX power for reliable operation.
- The firmware used the default/maximum transmit power, causing RF instability on this board.

### Fix
- In `managed_components/78__esp-wifi-connect/wifi_configuration_ap.cc`:
  - Set a default TX power of `40` (10 dBm) when there is no saved NVS value.
  - Call `esp_wifi_set_max_tx_power(max_tx_power_)` after `esp_wifi_start()` in AP mode.
- In `managed_components/78__esp-wifi-connect/wifi_station.cc`:
  - Call `esp_wifi_set_max_tx_power(max_tx_power_)` after `esp_wifi_start()` in STA mode.
- Remove manual PHY antenna override code that interfered with Wi-Fi startup.

### Result
- The Wi-Fi config AP became visible again.
- Station mode became much more stable.

## 2. Device auto-connected to a hardcoded SSID instead of starting config AP

### Problem
- Firmware bypassed the normal provisioning flow and attempted to join a hardcoded `ZOWIE` SSID.
- When no user SSID was stored, the device did not fall back cleanly to the configuration AP.

### Fix
- In `main/boards/common/wifi_board.cc`:
  - Remove the hardcoded `ZOWIE` auto-connect fallback.
  - Restore default logic: if no saved SSID exists, start the Wi-Fi config AP.

### Result
- First boot starts the standard configuration AP.
- The board no longer tries to auto-join an external network by default.

## 3. Single combined flash image for easier flashing

### Problem
- The ESP-IDF build creates multiple binaries: `bootloader.bin`, `xiaozhi.bin`, and `generated_assets.bin`.
- A single merged image was desired for simpler flashing.

### Fix
- Use `esptool merge_bin` with the correct address/file pairs:

```powershell
esptool.exe merge_bin --output build\xiaozhi_all.bin \
    0x1000 build\bootloader\bootloader.bin \
    0x10000 build\xiaozhi.bin \
    0x600000 build\generated_assets.bin
```

### Result
- `build\xiaozhi_all.bin` contains all required flash parts in one file.
- Flashing is simpler when using a single merged binary.

## 4. Custom board pin mapping and hardware wiring

### Board wiring for ESP32-C3 Super Mini
- SSD1306 OLED I2C: `GPIO8` = SDA, `GPIO9` = SCL
- MAX98357A speaker: `GPIO7` = BCLK, `GPIO5` = LRCK, `GPIO6` = DOUT
- INMP441 microphone: `GPIO3` = SCK, `GPIO2` = WS, `GPIO4` = DIN

### Fixes
- Updated `main/boards/esp32-c3-custom/config.h` with these pin assignments.
- Implemented the board-specific I2C and I2S setup in `main/boards/esp32-c3-custom/esp32_c3_custom_board.cc`.

## 5. Known build and asset issues

### Problem
- Flash or asset generation could fail if `generated_assets.bin` was missing or the build command used incorrect project root paths.

### Fix
- Verify `main/CMakeLists.txt` and custom asset generation paths.
- Ensure `build/generated_assets.bin` is generated successfully during `idf.py build`.

## 6. Notes for future debugging

- Use `idf.py monitor` to inspect boot logs and confirm Wi-Fi initialization.
- If the config AP does not appear, check that `esp_wifi_set_max_tx_power()` is called after `esp_wifi_start()`.
- If the board resets during init, verify pin assignments and avoid reserved GPIOs.

## Files involved
- `managed_components/78__esp-wifi-connect/wifi_configuration_ap.cc`
- `managed_components/78__esp-wifi-connect/wifi_station.cc`
- `main/boards/common/wifi_board.cc`
- `main/boards/esp32-c3-custom/config.h`
- `main/boards/esp32-c3-custom/esp32_c3_custom_board.cc`
- `main/CMakeLists.txt`

---

This summary captures the key fixes applied during ESP32-C3 Super Mini board support and Wi-Fi provisioning issue resolution.
