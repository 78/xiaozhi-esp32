# ESP-P4-Function-EV-Board

Board support for ESP-P4-Function-EV-Board. Wi‑Fi uses ESP‑Hosted via the on‑board ESP32‑C6. LCD is supported via the official MIPI‑DSI LCD adapter.

## Features
- Wi‑Fi: `esp_wifi_remote` + `esp_hosted` (SDIO) with ESP32‑C6 co‑processor
- Display: 7" MIPI‑DSI LCD (1024×600) via adapter; can also run headless
- Audio: ES8311 codec with speaker and microphone support
- Touch: GT911 capacitive touch controller
- SD Card: MicroSD card support (MMC mode)
- Camera: MIPI-CSI camera interface with fallback DVP configuration (OV5647, SC2336 sensors supported)
- USB: USB host/device support
- SPIFFS: Built-in flash filesystem support
- Fonts: Custom font support with Unicode characters (Vietnamese, Chinese, etc.)

## Configure
In `menuconfig`: Xiaozhi Assistant -> Board Type -> ESP-P4-Function-EV-Board

Ensure these are set (auto-set when building via config.json):
- `CONFIG_SLAVE_IDF_TARGET_ESP32C6=y`
- `CONFIG_ESP_HOSTED_P4_DEV_BOARD_FUNC_BOARD=y`
- `CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y`
- `CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y`

## LCD Connection (from Espressif user guide)
- Connect the LCD adapter board J3 to the board’s MIPI DSI connector (reverse ribbon).
- Wire `RST_LCD` (adapter J6) to `GPIO27` (board J1).
- Wire `PWM` (adapter J6) to `GPIO26` (board J1).
- Optionally power the LCD adapter via its USB or provide `5V` and `GND` from the board.

These pins are pre-configured in `config.h` as `PIN_NUM_LCD_RST=GPIO27` and `DISPLAY_BACKLIGHT_PIN=GPIO26`. Resolution is set to 1024×600.

## Build (example)
```powershell
idf.py set-target esp32p4
idf.py menuconfig
idf.py build
```

Tip: In menuconfig, choose Xiaozhi Assistant -> Board Type -> ESP-P4-Function-EV-Board.
If building a release via scripts, the `config.json` in this folder appends the required Hosted options.
