# AGENTS.md

## Project

XiaoZhi is an ESP-IDF C/C++ voice-assistant firmware supporting many chips, boards, displays, audio devices, and network transports. A build selects exactly one board implementation.

Use ESP-IDF v6.0.2 when possible. IDF 5.5.x is retained only for documented legacy boards.

## Architecture

- `main/application.*`: main event loop, protocol lifecycle, and high-level behavior.
- `main/device_state_machine.*`: legal runtime state transitions.
- `main/boards/common/`: board interfaces and reusable hardware/network helpers.
- `main/boards/**/`: board-specific pins, initialization, and build variants.
- `main/audio/`: codecs, audio tasks, engines, wake words, and queues.
- `main/protocols/`: transport-neutral API plus WebSocket and MQTT/UDP.
- `main/display/` and `main/led/`: reusable UI implementations.
- `main/mcp_server.*`: common device-side MCP tools and dispatch.
- `main/Kconfig.projbuild`: board and feature configuration.
- `main/CMakeLists.txt`: source, board, locale, font, and asset selection.
- `scripts/release.py`: canonical board/variant build entry point.

Read the closest existing implementation before adding a new one. Prefer the narrowest owning layer; do not put board-specific behavior into core modules.

## Required Rules

- Preserve unrelated worktree changes and keep patches focused.
- A build must export exactly one board factory through `DECLARE_BOARD(...)`.
- Never alter an existing board's pins to support different hardware. Add a uniquely named board or release variant; board identity affects OTA compatibility.
- Core code depends on `Board` interfaces, never a concrete board class or board `config.h`.
- Treat camera, backlight, display, LED, battery, and similar capabilities as optional.
- Change runtime state through `Application::SetDeviceState()` and the state machine.
- Callbacks may run outside the main task. Schedule application mutations with `Application::Schedule()` or event bits.
- Do not block the main event loop or audio tasks. Avoid unbounded queues and repeated large allocations in audio paths.
- Keep shared message semantics in `Protocol`; verify both transports when changing its contract.
- Validate network input and preserve `cJSON` ownership. NVS keys are persistent API and require migration when changed.
- Guard target-specific features with Kconfig/component rules. Do not assume every target has PSRAM or S3/P4 resources.
- Do not manually edit generated/vendor output: `build/`, `releases/`, `managed_components/`, `components/`, `sdkconfig*`, `main/assets/lang_config.h`, or generated mmap headers.
- Format only touched C/C++ files with the repository `.clang-format`; avoid unrelated mass formatting.

## Boards and Configuration

Board selection is a coupled chain:

`config.json` -> `scripts/release.py` -> `main/Kconfig.projbuild` -> `main/CMakeLists.txt` -> board source and `config.h`.

When adding a board or variant, update every relevant link in that chain. Include a unique board identity, correct chip target, flash/partition settings, exactly one `DECLARE_BOARD`, and board documentation. Follow `docs/custom-board.md`.

## Commands

Source the intended ESP-IDF environment first:

```sh
source /path/to/esp-idf/export.sh
idf.py --version
```

```sh
# Discover exact board and variant names
python3 scripts/release.py --list-boards

# Canonical variant build
python3 scripts/release.py <board-directory> --name <variant-name>

# Host-side release tests
python3 -m unittest discover -s scripts/tests -v

# Format/check touched files
clang-format -i <files>
clang-format --dry-run -Werror <files>
```

The release script changes local `sdkconfig` and build state. Do not assume the build directory still represents a previous target.

## Validation

- Board-only change: build affected variants and smoke-test changed hardware.
- Core, common-board, audio, protocol, display, dependency, Kconfig, or CMake change: run host tests and build representative affected chip/network paths.
- Protocol changes: verify WebSocket and MQTT/UDP when shared behavior changes.
- Audio changes: verify capture, playback, wake/VAD, interruption, reconnect, and applicable AEC modes.
- UI/assets changes: verify applicable no-display/OLED/LVGL paths and partition size.
- Always report what was tested and what still needs physical hardware. A successful build is not hardware validation.

## Authoritative Documentation

- Overview and SDK policy: `README.md`
- SDK compatibility: `docs/esp-idf-6-migration.md`
- Board guide: `docs/custom-board.md`
- Audio design: `main/audio/README.md`
- Code style: `docs/code_style.md`
- Protocols: `docs/websocket.md`, `docs/mqtt-udp.md`, `docs/mcp-protocol.md`
- CI matrix: `.github/workflows/build.yml`

Keep detailed or fast-changing information in those files, not here. Add a nested `AGENTS.md` only when a subsystem needs specialized instructions.

## freenove-esp32s3-display-2.8-lcd Branch Context

Branch: `v1.2-freenove`. Freenove ESP32-S3 2.8" (ILI9341, ES8311, XPT2046 touch on I2C 0x38), 16MB flash. Build: `python scripts/release.py freenove-esp32s3-display-2.8-lcd` (single-thread, weak CPU).

### Persist WebSocket
- `ConnectControlChannel()` after activation, ping/pong 30s, reconnect with exponential backoff (1→60s), `server_initiated_session_` flag
- See `main/protocols/websocket_protocol.cc`, `main/application.cc`

### AFE Audio Config
- `AFE_TYPE_FD` + `AFE_MODE_LOW_COST` + `agc_init=true` — do not change to HIGH_PERF or SR (breaks voice after wake word)
- See `main/audio/engines/afe_audio_engine.cc`

### Battery
- Voltage-trend via `AdcBatteryMonitor` on `ADC_UNIT_1, ADC_CHANNEL_8`, 200k/200k divider (1:2)
- TP4054 CHRG pin NOT routed to GPIO

### Custom Assets
- Pre-built `assets.bin` (7.78MB, 8 GIF + 30pt font) — compatible with v2.4.0
- `CONFIG_FLASH_CUSTOM_ASSETS=y`, `CUSTOM_ASSETS_FILE="boards/freenove-esp32s3-display-2.8-lcd/assets.bin"`

### OTA
- Server: `http://192.168.22.102:18792/ota`, upload: `POST /api/firmware/upload`
- Build + upload: `python scripts/release.py freenove-esp32s3-display-2.8-lcd --upload`
- See `docs/ota-api.md` for server API contract

### Key Files
- `main/boards/freenove-esp32s3-display-2.8-lcd/` — board definition, config, assets
- `main/boards/common/adc_battery_monitor.cc/.h` — battery state machine
- `docs/server-persistent-ws-spec.md` — persistent WS server spec
- `docs/ota-api.md` — OTA upload API contract
- `scripts/ota-upload.sh` — OTA upload script
