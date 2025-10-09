# MCP (Model Context Protocol) Interaction Process

NOTICE: AI-assisted generation. Please refer to the code for details when implementing backend services!!

The MCP protocol in this project is used for communication between the backend API (MCP client) and the ESP32 device (MCP server), enabling the backend to discover and invoke device-provided functions (tools).

## Protocol Format

According to the code (`main/protocols/protocol.cc`, `main/mcp_server.cc`), MCP messages are encapsulated within the message body of the underlying communication protocol (such as WebSocket or MQTT). Its internal structure complies with the [JSON-RPC 2.0](https://www.jsonrpc.org/specification) specification.

Example of the overall message structure:

```json
{
"session_id": "...", // Session ID
"type": "mcp", // Message type, fixed to "mcp"
"payload": { // JSON-RPC 2.0 payload
"jsonrpc": "2.0",
"method": "...", // Method name (e.g., "initialize", "tools/list", "tools/call")
"params": { ... }, // Method parameters (for request)
"id": ..., // Request ID (for request and response)
"result": { ... }, // Method execution result (for success response)
"error": { ... } // Error message (for error response)
}
}
```

The `payload` part is a standard JSON-RPC 2.0 message:

- `jsonrpc`: Fixed string "2.0".
- `method`: The name of the method to be called (for a Request).
- `params`: The method parameters, a structured value, typically an object (for a Request).
- `id`: The request identifier, provided by the client when sending a request and returned unchanged by the server in the response. Used to match requests and responses.
- `result`: The result if the method is successfully executed (for a Success Response).
- `error`: The error message if the method fails (for an Error Response).

## Interaction Process and Sending Timing

MCP interaction primarily revolves around the client (backend API) discovering and invoking "Tools" on the device.

1. **Connection Establishment and Capability Advertisement**

- **Timing:** After the device boots up and successfully connects to the backend API.
- **Sender:** The device.
- **Message:** The device sends a "hello" message using the basic protocol to the backend API. The message includes a list of capabilities supported by the device, for example, by indicating support for the MCP protocol (`"mcp": true`). - **Example (not an MCP payload, but a basic protocol message):**
```json
{
"type": "hello",
"version": ...,
"features": {
"mcp": true,
...
},
"transport": "websocket", // or "mqtt"
"audio_params": { ... },
"session_id": "..." // The device may set this after receiving the server's hello message.
}
```

2. **Initialize an MCP Session**

- **When:** After the backend API receives the device's "hello" message and confirms that the device supports MCP, it is typically sent as the first request in an MCP session.
- **Sender:** Backend API (client)
- **Method:** `initialize`
- **Message (MCP payload):**

```json
{
"jsonrpc": "2.0",
"method": "initialize",
"params": {
"capabilities": {
// Client capabilities, optional

// Camera vision related
"vision": {
"url": "...", // Camera: Image processing address (must be an HTTP address, not a websocket address)
"token": "..." // URL token
}

// ... Other client capabilities
}
},
"id": 1 // Request ID
}
```

- **Device response timing:** After the device receives and processes the `initialize` request.
- **Device Response Message (MCP Payload):**
```json
{
"jsonrpc": "2.0",
"id": 1, // Matches the request ID
"result": {
"protocolVersion": "2024-11-05",
"capabilities": {
"tools": {} // Tools doesn't seem to list detailed information here; tools/list is required
},
"serverInfo": {
"name": "...", // Device name (BOARD_NAME)
"version": "..." // Device firmware version
}
}
}
```

3. **Discovering the Device Tool List**

- **When:** When the backend API needs to obtain a list of specific functions (tools) currently supported by the device and how to call them.
- **Sender:** Backend API (client).
- **Method:** `tools/list`
- **Message (MCP payload):**
```json
{
"jsonrpc": "2.0",
"method": "tools/list",
"params": {
"cursor": "" // Used for paging, empty string for the first request
},
"id": 2 // Request ID
}
```
- **Device Response Timing:** After the device receives the `tools/list` request and generates the tool list.
- **Device response message (MCP payload):**
```json
{
"jsonrpc": "2.0",
"id": 2, // Match request ID
"result": {
"tools": [ // Tool object list
{
"name": "self.get_device_status",
"description": "...",
"inputSchema": { ... } // Parameter schema
},
{
"name": "self.audio_speaker.set_volume",
"description": "...",
"inputSchema": { ... } // Parameter schema
}
// ... More tools
],
"nextCursor": "..." // If the list is large and needs paging, this will contain the cursor value for the next request.
}
}
```
- **Paging:** If the `nextCursor` field is not empty, the client needs to send a `tools/list` request again with this `cursor` in `params`. Value to get the next page of tools.

4. **Calling Device Tools**

- **When:** When the backend API needs to execute a specific function on the device.
- **Sender:** Backend API (client).
- **Method:** `tools/call`
- **Message (MCP payload):**
```json
{
"jsonrpc": "2.0",
"method": "tools/call",
"params": {
"name": "self.audio_speaker.set_volume", // Tool name to call
"arguments": {
// Tool parameters, object format
"volume": 50 // Parameter name and value
}
},
"id": 3 // Request ID
}
```
- **Device Response Time:** After the device receives the `tools/call` request and executes the corresponding tool function.
- **Device Success Response Message (MCP Payload):**
```json
{
"jsonrpc": "2.0",
"id": 3, // Matching request ID
"result": {
"content": [
// Tool execution result content
{ "type": "text", "text": "true" } // Example: set_volume returns bool
],
"isError": false // Indicates success
}
}
```
- **Device Failure Response Message (MCP Payload):**
```json
{
"jsonrpc": "2.0",
"id": 3, // Matching request ID
"error": {
"code": -32601, // JSON-RPC error code, e.g., Method not found (-32601)
"message": "Unknown tool: self.non_existent_tool" // Error description
}
}
```

5. **Device Actively Sends Messages (Notifications)**
- **When:** When an event occurs within the device that requires notification to the background API (for example, a state change, although the code While the code example doesn't explicitly show a tool for sending such messages, the presence of `Application::SendMcpMessage` suggests that the device may proactively send MCP messages.
- **Sender:** Device (server).
- **Method:** This may be a method name starting with `notifications/` or a custom method.
- **Message (MCP payload):** This follows the JSON-RPC Notification format, but does not have an `id` field.
```json
{
"jsonrpc": "2.0",
"method": "notifications/state_changed", // Example method name
"params": {
"newState": "idle",
"oldState": "connecting"
}
// No id field
}
```
- **Backend API processing:** After receiving the Notification, the backend API performs the corresponding processing but does not respond.

## Interaction Diagram

Below is a simplified interaction sequence diagram showing the main MCP message flow:
... result: { tools: [...], nextCursor: ... } 

loop Optional Pagination 
BackendAPI->>Device: MCP Get Tools List Request 
Note over BackendAPI: method: tools/list 
Note over BackendAPI: params: { cursor: "..." } 
Device->>BackendAPI: MCP Get Tools List Response 
Note over Device: result: { tools: [...], nextCursor: "" } 
end 

BackendAPI->>Device: MCP Call Tool Request 
Note over BackendAPI: method: tools/call 
Note over BackendAPI: params: { name: "...", arguments: { ... } } 

alt Tool Call Successful 
Device->>BackendAPI: MCP Tool Call Success Response 
Note over Device: result: { content: [...], isError: false } 
else Tool Call Failed 
Device->>BackendAPI: MCP Tool Call Error Response 
Note over Device: error: { code: ..., message: ... } 
end opt Device Notification
Device->>BackendAPI: MCP Notification
Note over Device: method: notifications/...
Note over Device: params: { ... }
end
```

This document outlines the main MCP protocol interaction flow in this project. For detailed parameter details and tool functionality, please refer to `McpServer::AddCommonTools` in `main/mcp_server.cc` and the implementation of each tool.
