---
name: xiaozhi-esp32
description: XiaoZhi AI Chatbot - ESP32 voice assistant with MCP, OTA, and multi-board support
---

# XiaoZhi ESP32 AI Chatbot

## Overview
Open-source ESP32-based AI voice chatbot using ESP-IDF v5.4+. Voice interaction pipeline: ASR → LLM → TTS via WebSocket or MQTT+UDP. Implements MCP (Model Context Protocol) for device/cloud control.

## Project Structure

```
xiaozhi-esp32/
├── CMakeLists.txt          # Project root (version 1.9.4)
├── main/
│   ├── main.cc             # Entry point (app_main)
│   ├── application.cc/.h   # Singleton app, event loop, state machine
│   ├── mcp_server.cc/.h    # MCP JSON-RPC 2.0 server for tool calling
│   ├── ota.cc/.h           # OTA firmware updates
│   ├── settings.cc/.h      # NVS persistent storage
│   ├── system_info.cc/.h   # Device diagnostics
│   ├── device_state.h      # State enum (Idle→Connecting→Listening→Speaking)
│   ├── device_state_event.cc/.h
│   ├── audio/
│   │   ├── audio_service.cc/.h  # Audio pipeline manager
│   │   ├── audio_codec.cc/.h    # Abstract codec interface
│   │   ├── codecs/              # ES8311, ES8374, ES8388, ES8389, Box, NoAudio
│   │   ├── processors/          # AFE (ESP-SR), no-op, audio debugger (UDP)
│   │   └── wake_words/          # AFE, ESP, or custom wake word
│   ├── protocols/
│   │   ├── protocol.cc/.h       # Abstract protocol + AudioStreamPacket
│   │   ├── mqtt_protocol.cc/.h  # MQTT+UDP hybrid
│   │   └── websocket_protocol.cc/.h  # WebSocket binary protocol
│   ├── display/
│   │   ├── display.cc/.h        # Abstract display (LVGL)
│   │   ├── lcd_display.cc/.h    # LCD via SPI
│   │   └── oled_display.cc/.h   # OLED via I2C
│   ├── led/
│   │   ├── led.h                # Abstract LED
│   │   ├── single_led.cc/.h     # GPIO LED
│   │   ├── gpio_led.cc/.h       # Another GPIO LED variant
│   │   └── circular_strip.cc/.h # WS2812 LED strip
│   ├── boards/                  # 90+ board configurations
│   │   ├── common/              # Shared: Board base class, WifiBoard, Network
│   │   ├── bread-compact-wifi/  # Reference board (SSD1306+INMP441+MAX98357)
│   │   └── .../                 # One dir per board type
│   └── assets/
│       ├── common/              # Shared OGG sounds
│       └── locales/             # 23 languages (zh-CN, en-US, ja-JP, ...)
├── board-files/              # ESP32-C6-DevKitC-1 board config (custom)
├── partitions/                # Partition table CSVs (4M, 8M, 16M, 32M)
├── scripts/                   # flash.sh, gen_lang.py, release.py
├── docs/                      # Protocol docs (websocket.md, mqtt-udp.md, mcp-*.md)
└── sdkconfig.defaults*        # Per-chip SDK defaults (esp32, esp32s3, esp32c3, esp32c6, esp32p4)
```

## Architecture

### State Machine
```
Idle → Connecting → Listening → Speaking → Idle
  ↑         ↓                          |
  |---  WakeWord / Button     Abort ----|
```

### Audio Pipeline
```
(MIC) → [Processors] → {Encode Queue} → [Opus Encoder] → {Send Queue} → (Server)
(Server) → {Decode Queue} → [Opus Decoder] → {Playback Queue} → (Speaker)
```

### Event-Driven (FreeRTOS Event Groups)
- `MAIN_EVENT_SCHEDULE` - Execute queued tasks
- `MAIN_EVENT_SEND_AUDIO` - Send OPUS packets
- `MAIN_EVENT_WAKE_WORD_DETECTED` - Wake word
- `MAIN_EVENT_VAD_CHANGE` - Voice activity
- `MAIN_EVENT_CLOCK_TICK` - 1-second tick (update status bar, heap stats)
- `MAIN_EVENT_ERROR` - Network error

### Protocols

#### WebSocket
- Binary v2/v3 headers wrapping OPUS frames
- JSON messages: hello, stt, llm, tts, mcp, system, alert, custom
- `docs/websocket.md` for full spec

#### MQTT + UDP
- MQTT for signaling (JSON)
- UDP for audio streaming
- `docs/mqtt-udp.md` for spec

### MCP Server
JSON-RPC 2.0 over the protocol channel. Implements:
- `initialize` - Handshake, protocol version 2024-11-05
- `tools/list` - List available tools (paginated, max 8KB payload)
- `tools/call` - Execute tool by name with arguments
- Built-in tools: `self.get_device_status`, `self.audio_speaker.set_volume`, `self.screen.set_brightness`, `self.screen.set_theme`, `self.camera.take_photo`
- Boards can add custom tools via `Board::InitializeTools()`

### Board Abstraction
Each board in `main/boards/<name>/` implements:
- `Board` abstract class (hardware factory)
- GPIO pin config via `board_config.h`
- Audio codec (I2S codec chip or raw I2S)
- Display (OLED I2C or LCD SPI)
- Buttons (click/long-press handlers via espressif/button)
- LED(s)
- Network (WiFi or ML307 4G)
- Declared via `DECLARE_BOARD(ClassName)` macro

## Building

### Prerequisites
- ESP-IDF v5.4+
- Linux recommended (faster builds)

### Commands
```bash
# Configure
idf.py set-target esp32c6
idf.py menuconfig  # Select board under "Xiaozhi Assistant" > "Board Type"

# Build
idf.py build

# Flash
idf.py -p /dev/ttyACM0 flash monitor
```

### SDK Config Defaults
| File | Target | Flash |
|------|--------|-------|
| `sdkconfig.defaults` | Shared | 16M custom |
| `sdkconfig.defaults.esp32` | ESP32 | 4M |
| `sdkconfig.defaults.esp32s3` | ESP32-S3 | 16M QIO |
| `sdkconfig.defaults.esp32c3` | ESP32-C3 | 16M |
| `sdkconfig.defaults.esp32c6` | ESP32-C6 | 16M QIO |
| `sdkconfig.defaults.esp32p4` | ESP32-P4 | 16M |

## Key Conventions
- C++17 with Google C++ style
- Singleton pattern for Application, Board, McpServer
- Event-driven architecture via FreeRTOS event groups
- Board-specific code isolated per directory; shared logic in `boards/common/`
- LVGL v9.2.2 for display GUI
- OPUS 60ms frames (16kHz uplink / 24kHz downlink)
- MCP tools send results via `Application::SendMcpMessage()`

## Adding a New Board
1. Create `main/boards/<name>/` with `board_config.h` and board class
2. Add `CONFIG_BOARD_TYPE_<NAME>` to `Kconfig.projbuild`
3. Add board selection to `main/CMakeLists.txt`
4. Enable the board in `menuconfig`
5. See `main/boards/README.md` for details
