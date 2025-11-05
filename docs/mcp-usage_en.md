# MCP protocol IoT control usage instructions

> This document describes how to implement IoT control of ESP32 devices based on the MCP protocol. For detailed protocol process, please refer to [`mcp-protocol.md`](./mcp-protocol.md).

## Introduction

MCP (Model Context Protocol) is a new generation protocol recommended for IoT control. It discovers and calls "Tools" between the background and the device through the standard JSON-RPC 2.0 format to achieve flexible device control.

## Typical usage process

1. After the device is started, it establishes a connection with the background through basic protocols (such as WebSocket/MQTT).
2. The background initializes the session through the `initialize` method of the MCP protocol.
3. The background obtains all tools (functions) and parameter descriptions supported by the device through `tools/list`.
4. The background calls specific tools through `tools/call` to control the device.

For detailed protocol format and interaction, please see [`mcp-protocol.md`](./mcp-protocol.md).

## Description of device-side tool registration method

The device registers "tools" that can be called in the background through the `McpServer::AddTool` method. Its commonly used function signatures are as follows:

```cpp
void AddTool(
    const std::string& name, // Tool name, it is recommended to be unique and hierarchical, such as self.dog.forward
    const std::string& description, // Tool description, concise description of functions, easy to understand large models
    const PropertyList& properties, // Input parameter list (can be empty), supported types: Boolean, integer, string
    std::function<ReturnValue(const PropertyList&)> callback //Callback implementation when the tool is called
);
```
- name: The unique identifier of the tool. It is recommended to use the "module.function" naming style.
- description: Natural language description, easy for AI/users to understand.
- properties: parameter list, supported types are Boolean, integer, string, range and default value can be specified.
- callback: the actual execution logic when receiving the call request, the return value can be bool/int/string.

## Typical registration example (taking ESP-Hi as an example)

```cpp
void InitializeTools() {
    auto& mcp_server = McpServer::GetInstance();
    //Example 1: No parameters, control the robot to move forward
    mcp_server.AddTool("self.dog.forward", "Robot moves forward", PropertyList(), [this](const PropertyList&) -> ReturnValue {
        servo_dog_ctrl_send(DOG_STATE_FORWARD, NULL);
        return true;
    });
    //Example 2: With parameters, set the RGB color of the light
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

## Common tool call JSON-RPC examples

### 1. Get the tool list
```json
{
  "jsonrpc": "2.0",
  "method": "tools/list",
  "params": { "cursor": "" },
  "id": 1
}
```

### 2. Control the chassis to move forward
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

### 3. Switch lighting mode
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

### 4. Camera flip
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

## Remark
- For the tool name, parameters and return value, please refer to the `AddTool` registration on the device side.
- It is recommended that all new projects uniformly use the MCP protocol for IoT control.
- For detailed protocol and advanced usage, please refer to [`mcp-protocol.md`](./mcp-protocol.md).