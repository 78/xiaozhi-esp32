# xiaozhi-esp32 — Agent Guide

## Project

ESP-IDF (v5.5.2) firmware for the XiaoZhi AI voice assistant. C++ (`.cc`/`.h`), Google style with clang-format. Runs on ESP32, ESP32-C3, ESP32-S3, ESP32-C5, ESP32-C6, ESP32-P4.

## Build & Flash

```bash
# Per-board build (recommended, uses config.json):
python scripts/release.py <board-dir-name>   # e.g. python scripts/release.py bread-compact-wifi

# Manual:
idf.py set-target esp32s3   # target: esp32, esp32s3, esp32c3, esp32c6, esp32p4
idf.py menuconfig           # Xiaozhi Assistant > Board Type
idf.py build
idf.py flash monitor

# Build with OTA upload (auto-pushes firmware to voice_gateway):
python scripts/release.py <board-dir-name> --upload   # e.g. python scripts/release.py freenove-esp32s3-display-2.8-lcd --upload

# Or upload manually after build:
scripts/ota-upload.sh   # uploads build/xiaozhi.bin to OTA server

# List all board variants:
python scripts/release.py --list-boards

# Build all boards:
python scripts/release.py all
```

`idf.py -DBOARD_NAME=<name> -DBOARD_TYPE=<board> build` for override at build time.

## Board System

- Each board lives in `main/boards/<board-dir>/` or `main/boards/<manufacturer>/<board-dir>/`
- Requires: `config.h` (pins), `xxx_board.cc` (class), `config.json` (build metadata)
- Board class extends `WifiBoard` / `Ml307Board` / `DualNetworkBoard` / `Nt26Board` / `RndisBoard`
- Registered via `DECLARE_BOARD(ClassName)` at end of board `.cc` file
- See `docs/custom-board.md` for adding new boards

## Key Directories

| Path | Purpose |
|------|---------|
| `main/` | Application core, entrypoint `main.cc:app_main()` |
| `main/boards/` | All board definitions (70+) |
| `main/boards/common/` | Shared board components (button, backlight, battery, etc.) |
| `main/audio/engines/afe_audio_engine.cc` | AFE config: `AFE_TYPE_FD`/`AFE_MODE_LOW_COST` + `agc_init=true` |
| `main/display/` | Display drivers (OLED, LCD, LVGL) |
| `main/protocols/` | WebSocket + MQTT+UDP communication |
| `partitions/v2/` | Partition tables — **v2 only** (4m, 8m, 16m, 16m_c3, 32m) |
| `scripts/` | Build/release helpers |
| `sdkconfig.defaults` | Global SDK config |
| `sdkconfig.defaults.esp32*` | Per-chip overrides |

## Architecture

```
app_main() → Application::Initialize() → Board::GetInstance() → Setup display, audio, network
           → Application::Run() → event loop (FreeRTOS event group)
```

State machine: `Starting → WifiConfiguring / Connecting → Idle → Listening → Speaking`

Communication protocols: WebSocket (default) or MQTT+UDP. Audio codec: OPUS.

## Configuration

- Board selection: `idf.py menuconfig → Xiaozhi Assistant → Board Type`
- Language: `Xiaozhi Assistant → Default Language` (30+ languages)
- Assets: `Xiaozhi Assistant → Flash Assets` (default/custom/emote/none)
- Assets partition is auto-built by CMake from board config (fonts, emoji, wake word model)
- Wake word: ESP-SR (AFE for S3/P4 with PSRAM; Wakenet for C3/C5/C6/ESP32)

## Formatting

```bash
find main -iname '*.h' -o -iname '*.cc' | xargs clang-format -i
```

`.clang-format` at root (Google style, 4-space indent, 100 col, left-aligned pointers).

## Dependencies

Managed via `main/idf_component.yml`. Fetch with `idf.py reconfigure` (automatic on build). Key: LVGL 9.5.0, esp-sr 2.3.0, ESP-IDF >= 5.5.2.

## CI

GitHub Actions in `.github/workflows/build.yml`. Uses `espressif/idf:v5.5.2` Docker image. Builds affected boards on PR, all boards on push to `main`.

## Version

`2.3.0` (PROJECT_VER in root `CMakeLists.txt`). v1 branch (`git checkout v1`) maintained until Feb 2026 — **partition tables are incompatible between v1 and v2**.

## Testing

No formal test framework — embedded firmware. Verify by building for the target board.

## v1.2-freenove Branch Context

Branch: `v1.2-freenove`, tags `v1.3-freenove`, `v1.4-freenove`. Freenove ESP32-S3 2.8" (ILI9341, ES8311, XPT2046 touch on I2C 0x38), 16MB flash. WiFi `home4`. Device MACs: `3c:0f:02:dd:c1:a4` (original), `44:1b:f6:cf:78:b8` (new). IP `192.168.22.205`. Host IP `192.168.22.249`. OTA URL: `http://192.168.22.102:18792/ota`. Build with `python scripts/release.py freenove-esp32s3-display-2.8-lcd`.

### USB Detection (Battery)
TP4054 CHRG pin NOT routed to GPIO (confirmed). Voltage-trend detection via `AdcBatteryMonitor` on `ADC_UNIT_1, ADC_CHANNEL_8`, 200k/200k divider (1:2). Custom `adc_oneshot` handle + `curve_fitting` calibration. State machine: `kIdle`/`kCharging`/`kFull`/`kDischarging` with rolling median (5 samples × 5s), 30mV hysteresis, 60s >4150mV for full heuristic.

### MCP Tools Added
- `self.audio_wake_word.set_state` — `start`/`stop`, calls `AudioService::EnableWakeWordDetection()`, idempotent
- `self.audio_wake_word.get_status` — returns `running`/`stopped` + `mic_active` via event group + codec state
- `self.audio_pipeline.reset` — `input`/`output`/`idle`, calls `codec->EnableInput/EnableOutput()`

### Custom Assets
`assets.bin` (7.78MB, 8 GIF + 30pt font). `CONFIG_FLASH_DEFAULT_ASSETS=n`, `CONFIG_FLASH_CUSTOM_ASSETS=y`, `CUSTOM_ASSETS_FILE="boards/freenove-esp32s3-display-2.8-lcd/assets.bin"`.

### Key Files
- `main/boards/freenove-esp32s3-display-2.8-lcd/freenove-esp32s3-display-2.8-lcd.cc` — Board class, touch, battery monitor, MCP tools
- `main/boards/freenove-esp32s3-display-2.8-lcd/config.json` — OTA URL + custom assets config
- `main/boards/freenove-esp32s3-display-2.8-lcd/assets.bin` — 7.78MB custom assets
- `main/boards/common/adc_battery_monitor.h` — `ChargeState` enum, state machine, median filter
- `main/boards/common/adc_battery_monitor.cc` — Voltage-trend state machine, `GetVoltageMv()` via adc_oneshot + curve_fitting
- `main/audio/audio_service.h/.cc` — `EnableWakeWordDetection()`, `IsWakeWordRunning()`, `AudioInputTask()`

### Release Workflow (After Code Change)

1. `python scripts/release.py freenove-esp32s3-display-2.8-lcd --upload`
   — builds + uploads firmware to OTA server
2. Flash via USB: `idf.py -p /dev/ttyACM0 flash` (repeat for `/dev/ttyACM1`)
3. Wait for device to OTA-update automatically (if version bumped) or restart manually
4. Tag: `git tag v1.<N>-freenove && git push origin v1.<N>-freenove`
5. Push: `git push origin HEAD:v1.2-freenove`

### OTA Update Mechanism

- Device checks `CONFIG_OTA_URL` (`http://192.168.22.102:18792/ota`) on every boot
- Server returns `{"firmware":{"version":"X.Y.Z","url":"http://..."}}` 
- Device auto-downloads + applies if version > current
- No need to flash USB for subsequent updates — just bump `PROJECT_VER` in `CMakeLists.txt`, rebuild, then `python scripts/release.py <board> --upload`
- Upload endpoint: `POST /api/firmware/upload` (multipart, field `file`)
- See `docs/ota-api.md` for server-side API contract

### Other Notes

- `sdkconfig` is auto-generated; do not edit manually
- Per-chip defaults in `sdkconfig.defaults.esp32*` files
- Release zips go to `releases/` directory (gitignored)
- OTA URL configurable via `CONFIG_OTA_URL` in Kconfig
- `docs/` has protocol specs, MCP usage, blufi setup
