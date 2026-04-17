# MCP IoT Control Usage

> This document describes how to implement IoT control for ESP32 devices using the MCP protocol. For the detailed wire protocol, see [`mcp-protocol.md`](./mcp-protocol.md).

## Introduction

MCP (Model Context Protocol) is the recommended protocol for IoT control in this project. It uses JSON-RPC 2.0 to let the backend discover and invoke "tools" registered by the device, giving you a flexible way to expose device functionality.

## Typical Flow

1. The device boots and connects to the backend over WebSocket or MQTT.
2. The backend sends an `initialize` call to start the MCP session.
3. The backend issues `tools/list` to discover available tools and their input schemas.
4. The backend calls individual tools with `tools/call` to control the device.

See [`mcp-protocol.md`](./mcp-protocol.md) for the exact message format.

## Registering Tools on the Device

Tools are registered through the `McpServer` singleton. There are two registration APIs:

- `McpServer::AddTool` - regular tool, visible in the default `tools/list` response and callable by the AI model.
- `McpServer::AddUserOnlyTool` - hidden tool, only returned when the backend lists tools with `withUserTools=true`. Use this for privileged or user-initiated actions (reboot, firmware upgrade, snapshots, etc.) that must not be invoked autonomously by the model.

Both APIs share the same signature:

```cpp
void AddTool(
    const std::string& name,           // unique tool name, e.g. self.dog.forward
    const std::string& description,    // short description for the model
    const PropertyList& properties,    // input parameters (may be empty); supported types: bool, int, string
    std::function<ReturnValue(const PropertyList&)> callback // implementation
);

void AddUserOnlyTool(
    const std::string& name,
    const std::string& description,
    const PropertyList& properties,
    std::function<ReturnValue(const PropertyList&)> callback
);
```

- `name` - unique identifier. A `module.action` naming style works well.
- `description` - natural-language description; used by the AI to decide when to call the tool.
- `properties` - input parameters. Supported property types are boolean, integer, and string, with optional min/max and default values.
- `callback` - implementation. Return values may be `bool`, `int`, or `std::string`.

## Example (ESP-Hi)

```cpp
void InitializeTools() {
    auto& mcp_server = McpServer::GetInstance();

    // Example 1: no arguments - move the robot forward
    mcp_server.AddTool("self.dog.forward",
        "Move the robot forward",
        PropertyList(),
        [this](const PropertyList&) -> ReturnValue {
            servo_dog_ctrl_send(DOG_STATE_FORWARD, NULL);
            return true;
        });

    // Example 2: with arguments - set RGB light color
    mcp_server.AddTool("self.light.set_rgb",
        "Set the RGB color of the light",
        PropertyList({
            Property("r", kPropertyTypeInteger, 0, 255),
            Property("g", kPropertyTypeInteger, 0, 255),
            Property("b", kPropertyTypeInteger, 0, 255)
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            int r = properties["r"].value<int>();
            int g = properties["g"].value<int>();
            int b = properties["b"].value<int>();
            led_on_ = true;
            SetLedColor(r, g, b);
            return true;
        });
}
```

## Example - Registering a User-only Tool

```cpp
mcp_server.AddUserOnlyTool("self.display.clear_cache",
    "Clear locally cached images. User-only action.",
    PropertyList(),
    [](const PropertyList&) -> ReturnValue {
        ClearLocalCache();
        return true;
    });
```

A tool registered this way will not appear in a regular `tools/list` response. The backend must set `params.withUserTools = true` to see it.

## Built-in Tools

`McpServer::AddCommonTools` and `McpServer::AddUserOnlyTools` register a number of tools automatically:

### Default (AI-callable) tools - from `AddCommonTools`

| Tool | Description |
|------|-------------|
| `self.get_device_status` | Returns the current volume, screen, battery, network, etc. |
| `self.audio_speaker.set_volume` | Set speaker volume (`volume`: 0-100). |
| `self.screen.set_brightness` | Set screen brightness when a backlight is available (`brightness`: 0-100). |
| `self.screen.set_theme` | Switch UI theme (`theme`: `"light"` or `"dark"`), when LVGL is enabled. |
| `self.camera.take_photo` | Take a picture with the on-board camera (when the board has one) and answer the given `question` about it. |

Board-specific tools are appended after these by each board's `InitializeTools()`.

### User-only tools - from `AddUserOnlyTools`

These tools are hidden by default. The backend must pass `withUserTools=true` to `tools/list` to see them. They are intended for companion apps / end users rather than the AI model.

| Tool | Description |
|------|-------------|
| `self.get_system_info` | Return a JSON blob describing the system. |
| `self.reboot` | Reboot the device after a short delay. |
| `self.upgrade_firmware` | Download firmware from `url` and install it, then reboot. |
| `self.screen.get_info` | Return the current screen width, height, and whether it is monochrome (LVGL boards only). |
| `self.screen.snapshot` | Snapshot the screen as JPEG and upload it to `url` (LVGL boards, when `CONFIG_LV_USE_SNAPSHOT=y`). |
| `self.screen.preview_image` | Download and display an image from `url` on the screen. |
| `self.assets.set_download_url` | Set the download URL for the assets partition. |

## JSON-RPC Examples

### 1. Get the tools list

```json
{
  "jsonrpc": "2.0",
  "method": "tools/list",
  "params": { "cursor": "", "withUserTools": false },
  "id": 1
}
```

### 2. Move the chassis forward

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.chassis.go_forward",
    "arguments": {}
  },
  "id": 2
}
```

### 3. Switch the light mode

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.chassis.switch_light_mode",
    "arguments": { "light_mode": 3 }
  },
  "id": 3
}
```

### 4. Reboot the device (user-only)

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.reboot",
    "arguments": {}
  },
  "id": 4
}
```

## Notes

- Tool names, parameters, and return values must match what the device registers via `AddTool` / `AddUserOnlyTool`.
- Prefer MCP for any new IoT control.
- For the wire protocol and advanced topics, see [`mcp-protocol.md`](./mcp-protocol.md).
