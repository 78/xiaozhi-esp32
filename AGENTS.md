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
| `main/audio/` | Audio codecs, processors, wake words |
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

`2.2.6` (PROJECT_VER in root `CMakeLists.txt`). v1 branch (`git checkout v1`) maintained until Feb 2026 — **partition tables are incompatible between v1 and v2**.

## Testing

No formal test framework — embedded firmware. Verify by building for the target board.

## Other Notes

- `sdkconfig` is auto-generated; do not edit manually
- Per-chip defaults in `sdkconfig.defaults.esp32*` files
- Release zips go to `releases/` directory (gitignored)
- OTA URL configurable via `CONFIG_OTA_URL` in Kconfig
- `docs/` has protocol specs, MCP usage, blufi setup
