# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

XiaoZhi ESP32 is an AI voice chatbot project supporting 80+ ESP32-based development boards with offline wake word detection, streaming ASR+LLM+TTS, MCP (Model Context Protocol) integration, and multi-language support (Chinese, English, Japanese). It's an open-source MIT-licensed project for AI hardware development education.

## Build System and Development Commands

### ESP-IDF Based Build System
This project uses ESP-IDF 5.4+ with CMake build system:

```bash
# Build for specific board (replace with actual board name)
idf.py build

# Flash to device 
idf.py -p /dev/ttyUSB0 flash monitor

# Clean build
idf.py clean

# Configure project (menuconfig)
idf.py menuconfig
```

### Release Build System
The project includes a custom release system for multiple board configurations:

```bash
# Build firmware for specific board type
python scripts/release.py [board-directory-name]

# Example board names: lichuang-dev, esp-box-3, m5stack-core-s3
python scripts/release.py lichuang-dev
```

### Board Configuration
Each supported board has its own directory under `main/boards/[board-name]/` containing:
- `config.h` - Hardware pin mapping and configuration
- `config.json` - Build configuration with target chip and sdkconfig options
- `[board-name].cc` - Board-specific initialization code
- `README.md` - Board documentation

## Code Architecture

### Main Application Structure
- **Entry Point**: `main/main.cc` - Minimal ESP32 entry point
- **Core Application**: `main/application.cc/.h` - Singleton orchestrating entire system
- **Device States**: Idle, listening, speaking, connecting, upgrading with event-driven transitions
- **Event System**: FreeRTOS event groups for coordinated task synchronization

### Audio Processing Pipeline
Multi-stage bidirectional audio pipeline:
```
Input: MIC → [Audio Processor] → [Opus Encoder] → Server
Output: Server → [Opus Decoder] → Speaker
```

Key components:
- `audio/audio_service.cc` - Manages three separate tasks with FreeRTOS queues
- `audio/processors/` - Configurable audio processing (AFE or no-op)
- `audio/wake_words/` - Multiple wake word implementations (AFE, ESP, Custom)
- `audio/codecs/` - Various audio codec support (ES8311, ES8374, etc.)

### Display System
- **Base Class**: `display/display.cc` - Abstract interface with LVGL integration
- **Implementations**: LCD, OLED, ESP logging displays
- **Thread Safety**: DisplayLockGuard for LVGL access
- **UI Elements**: Status bar, chat messages, emotions, notifications

### Communication Protocols
- **Protocol Abstraction**: `protocols/protocol.cc` - Common interface
- **MQTT Protocol**: `protocols/mqtt_protocol.cc`
- **WebSocket Protocol**: `protocols/websocket_protocol.cc`
- **Binary Protocols**: Version 2/3 with timestamp support for AEC

### Board Abstraction Layer
- **Base Classes**: `boards/common/board.cc`, `wifi_board.cc`, `ml307_board.cc`
- **Hardware Abstraction**: Audio codec, display, LED, camera, backlight interfaces
- **Network Management**: WiFi/4G network interface abstraction
- **80+ Board Support**: Each board directory contains complete hardware configuration

### MCP Server Implementation
- **MCP Integration**: `mcp_server.cc` - Full JSON-RPC 2.0 MCP compliance
- **Tool System**: Dynamic tool registration with type-safe parameters
- **Common Tools**: Device status, volume, brightness, theme, camera control
- **Thread Safety**: Separate execution threads with configurable stack sizes

## Development Workflows

### Adding New Board Support
1. Create new directory: `main/boards/my-custom-board/`
2. Add configuration files: `config.h`, `config.json`, board implementation file
3. Update `main/CMakeLists.txt` with new board type mapping
4. Test build: `python scripts/release.py my-custom-board`

### Audio Codec Integration
- Inherit from `AudioCodec` base class in `audio/audio_codec.h`
- Implement codec-specific initialization and I2S configuration
- Add codec files to `audio/codecs/` directory
- Register in board implementation

### Display Driver Addition
- Inherit from `Display` base class in `display/display.h` 
- Implement LVGL buffer management and drawing operations
- Add driver to `display/` directory
- Configure in board's display initialization

### MCP Tool Development
- Inherit from `McpTool` base class with JSON-RPC parameter system
- Implement tool-specific functionality with thread-safe operations
- Register tool in board's MCP initialization
- Support type-safe parameters (boolean, integer, string arrays)

## Common File Patterns

### Board Implementation Pattern
```cpp
class MyBoard : public WifiBoard {
    // Hardware initialization in constructor
    // Override virtual methods: GetAudioCodec(), GetDisplay(), GetBacklight()
    // Register with DECLARE_BOARD(MyBoard) macro
};
```

### Audio Codec Pattern
```cpp
class MyCodec : public AudioCodec {
    // I2S and I2C configuration
    // Implement Initialize(), SetVolume(), SetMute() methods
    // Handle power management integration
};
```

### MCP Tool Pattern
```cpp
class MyTool : public McpTool {
    // Define parameter schema in constructor
    // Implement Call() method with JSON return values
    // Handle error cases with proper JSON-RPC error responses
};
```

## Testing and Quality Assurance

### Build Validation
- Test builds across multiple board configurations
- Verify partition table configurations for different flash sizes
- Check sdkconfig compatibility with target chips

### Audio Pipeline Testing
- Use audio debugging server: `python scripts/audio_debug_server.py`
- Test wake word detection across different noise conditions
- Validate AEC modes (off, device-side, server-side)

### MCP Tool Testing
- Verify JSON-RPC compliance with parameter validation
- Test tool execution in separate threads
- Validate error handling and response formatting

## Key Dependencies and Components

### External Components (managed_components/)
- LVGL for display graphics
- ESP-SR for wake word detection
- ESP-LCD drivers for various display controllers
- ESP-CODEC for audio processing
- Opus encoder/decoder for audio compression

### Language Assets
- Multi-language sound files in `main/assets/[lang]/`
- Generated language configuration headers
- Embedded P3 audio format support

### Build Dependencies
- ESP-IDF 5.4+
- Python 3.7+ for build scripts
- Git for component management

## Important Notes

### Board Type Uniqueness
Never modify existing board configurations for custom hardware - always create new board types to avoid OTA conflicts. Each board has unique firmware upgrade channels.

### Memory Management
- Use RAII patterns for resource management (DisplayLockGuard, etc.)
- Prefer stack allocation for small objects
- Use smart pointers for dynamic allocation when necessary

### Thread Safety
- Main application uses single-threaded event loop with scheduled callbacks
- Audio service uses separate FreeRTOS tasks with queue-based communication
- MCP tools execute in separate threads with proper synchronization