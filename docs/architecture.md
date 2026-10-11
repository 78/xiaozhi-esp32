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

## Decisions pending before implementation

- Define configuration and persistent-data formats and size limits.
- Choose storage for mutable robot data and verify the required ESP-IDF
  filesystem component.
- Confirm flash size, OTA requirements, and a non-overlapping partition layout.
  The existing `partitions/v2/8m.csv` must not be changed until these are known.
- Confirm the exact microphone, amplifier, display, motor driver, encoders, and
  range sensor, then record their pin assignments in the board configuration.
- Decide where robot MCP tools are registered in the application lifecycle.

This is a planning scaffold only. The new board and modules are not yet wired
into `main/CMakeLists.txt`, Kconfig, or the build script.
