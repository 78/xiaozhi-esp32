# JonnyBot Architecture

This document records the initial firmware layout for the JonnyBot fork. The
repository root is the firmware root; there is no additional `firmware/`
directory.

## Planned modules

```text
main/
├── application.cc / .h             # Application lifecycle and agent FSM
├── device_state_machine.cc / .h    # Legal runtime state transitions
├── boards/
│   ├── common/                     # Shared board interfaces and helpers
│   └── seeed-xiao-esp32s3-sense/   # JonnyBot board implementation
├── robot/                          # Robot control, sensors, memory, intent, skills
├── net/                            # HTTP, DeepSeek, Yandex STT/TTS clients
├── audio/                          # Reuse/adapt XiaoZhi audio components
├── display/                        # Reuse/adapt display components; robot face
└── mcp_server.cc / .h              # MCP tool registry and dispatch

scripts/                            # Build and asset tooling
partitions/                         # Flash partition tables
docs/                               # Architecture and protocol documentation
```

## Robot modules

The planned `main/robot/` modules are `motors`, `safety`, `sensors`, `memory`,
`intent`, `skills`, `parser`, `context`, and `base64`.

Motor commands should be queued for a dedicated motor task. Safety stop must be
able to interrupt motion independently of normal command processing. MCP tools
should enqueue requests rather than execute blocking motion in their callbacks.

## Network modules

The planned `main/net/` modules are a shared HTTP client and DeepSeek, Yandex
STT, and Yandex TTS clients. REST requests must not block the application event
loop or audio tasks.

## Confirmed hardware target

| Component | Specification |
| --- | --- |
| Board | Seeed XIAO ESP32S3 Sense, 8 MB flash, 8 MB PSRAM |
| Motors | CNMAWAY GA12-N20, 6 V, 150 RPM, approximately 1:100 gearbox |
| Motor driver | DRV8833, dual channel |
| Encoder | One AS5600 with a 6 x 2.5 mm diametric magnet, I2C |
| Range sensor | VL53L0X GY-530, I2C, default address 0x29 |
| Display | 1.69-inch ST7789V3, 240 x 280, SPI |
| Audio output | MAX98357A I2S mono amplifier and 8-ohm speaker |

Board pin assignments and peripheral bus topology still need to be recorded
before the board implementation is wired into the build.

## Decisions pending before implementation

- Define configuration and persistent-data formats and size limits.
- Choose storage for mutable robot data and verify the required ESP-IDF
  filesystem component.
- Confirm flash size, OTA requirements, and a non-overlapping partition layout.
  The existing `partitions/v2/8m.csv` must not be changed until these are known.
- Decide where robot MCP tools are registered in the application lifecycle.

This is a planning scaffold only. The new board and modules are not yet wired
into `main/CMakeLists.txt`, Kconfig, or the build script.
