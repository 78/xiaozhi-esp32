# MCP Protocol IoT Control Usage Instructions

> This document describes how to implement IoT control of ESP32 devices based on the MCP protocol. For detailed protocol flow, please refer to [`mcp-protocol.md`](./mcp-protocol.md).

## Introduction

MCP (Model Context Protocol) is a next-generation, recommended protocol for IoT control. It uses the standard JSON-RPC 2.0 format to discover and call "tools" between the backend and the device, enabling flexible device control.

## Typical Usage Flow

1. After booting, the device establishes a connection with the backend using a basic protocol (such as WebSocket/MQTT).

2. The backend initializes the session using the MCP protocol's `initialize` method.

3. The backend retrieves all supported tools (functions) and parameter descriptions for the device using `tools/list`.

4. The backend calls a specific tool using `tools/call` to control the device.

For detailed protocol format and interaction, see [`mcp-protocol.md`](./mcp-protocol.md).

## Device-side Tool Registration Method Description

The device registers a "tool" that can be called by the backend through the `McpServer::AddTool` method. Its common function signature is as follows:

```cpp
void AddTool(
const std::string& name, // Tool name, recommended to be unique and structured, such as self.dog.forward
const std::string& description, // Tool description, briefly describing its functionality for easier understanding in large models
const PropertyList& properties, // Input parameter list (optional), supported types: Boolean, integer, string
std::function<ReturnValue(const PropertyList&)> callback // Callback implementation for when the tool is called
);
```
- name: Unique identifier for the tool. "Module.Function" naming style is recommended.
- description: Natural language description for easier understanding by the AI/user. - properties: A list of parameters. Supported types include booleans, integers, and strings. Ranges and default values ​​can be specified.
- callback: The actual execution logic upon receiving a call request. The return value can be bool, int, or string.

## Typical Registration Example (Using ESP-Hi)

```cpp
void InitializeTools() {
auto& mcp_server = McpServer::GetInstance();
// Example 1: No parameters, controlling the robot forward
mcp_server.AddTool("self.dog.forward", "Robot moves forward", PropertyList(), [this](const PropertyList&) -> ReturnValue {
servo_dog_ctrl_send(DOG_STATE_FORWARD, NULL);
return true;
});
// Example 2: With parameters, setting the light's RGB color
mcp_server.AddTool("self.light.set_rgb", "Set RGB color", PropertyList({
Property("r", kPropertyTypeInteger, 0, 255),
Property("g", kPropertyTypeInteger, 0, 255),
Property("b", kPropertyTypeInteger, 0, 255)
}), [this](const PropertyList& properties) -> ReturnValue {
int r = properties["r"].value<int>();
int g = properties["g"].value<int>();
int b = properties["b"].value<int>();
led_on_ = true;
SetLedColor(r, g, b);
return true;
});
}
```

## Common Tool Call JSON-RPC Examples

### 1. Get Tool List
```json
{
"jsonrpc": "2.0",
"method": "tools/list",
"params": { "cursor": "" },
"id": 1
}
```

### 2. Control Chassis Forward
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

### 3. Switch Light Mode
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

### 4. Flip Camera
```json
{
"jsonrpc": "2.0",
"method": "tools/call",
"params": {
"name": "self.camera.set_camera_flipped",
"arguments": {}
},
"id": 4
}
```

## Notes
- Tool names, parameters, and return values ​​should be based on the device's settings. Register with `AddTool`.
- We recommend that all new projects adopt the MCP protocol for IoT control.
- For detailed protocol information and advanced usage, please refer to [`mcp-protocol.md`](./mcp-protocol.md).
