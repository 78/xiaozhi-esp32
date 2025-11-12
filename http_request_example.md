# HTTP Request Tool Usage Example

The MCP server now includes a general HTTP request tool (`self.http.request`) that can be used for cloud registration and API calls.

## Tool Specification

**Name:** `self.http.request`
**Description:** Make HTTP requests to cloud services or APIs. Supports GET, POST, PUT, DELETE methods with optional headers and body.

### Parameters:
- `url` (string, required): The URL to make the request to
- `method` (string, optional): HTTP method (GET, POST, PUT, DELETE). Default: "GET"
- `headers` (string, optional): JSON string of headers. Example: `{"Content-Type": "application/json", "Authorization": "Bearer token"}`
- `body` (string, optional): Request body for POST/PUT requests
- `timeout` (integer, optional): Timeout in seconds. Default: 30, Range: 1-300

### Return Value:
JSON object containing:
- `status_code`: HTTP status code
- `body`: Response body as string
- `success`: Boolean indicating if request was successful (status code 200-299)

## Cloud Registration Example

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.http.request",
    "arguments": {
      "url": "https://api.example.com/devices/register",
      "method": "POST",
      "headers": "{\"Content-Type\": \"application/json\", \"X-API-Key\": \"your-api-key\"}",
      "body": "{\"device_id\": \"ESP32_001\", \"device_type\": \"xiaozhi-esp32\", \"firmware_version\": \"2.0.4\"}",
      "timeout": 30
    }
  },
  "id": 123
}
```

Expected response:
```json
{
  "jsonrpc": "2.0",
  "id": 123,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "{\"status_code\": 200, \"body\": \"{\\\"device_id\\\": \\\"ESP32_001\\\", \\\"status\\\": \\\"registered\\\", \\\"token\\\": \\\"abc123\\\"}\", \"success\": true}"
      }
    ],
    "isError": false
  }
}
```

## Simple GET Request Example

```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.http.request",
    "arguments": {
      "url": "https://api.example.com/devices/ESP32_001/status"
    }
  },
  "id": 124
}
```

## Error Handling

If the request fails, you'll receive an error response:

```json
{
  "jsonrpc": "2.0",
  "id": 125,
  "error": {
    "code": -32602,
    "message": "HTTP request failed: Failed to open URL: https://invalid-url.com"
  }
}
```

## Security Notes

- This tool is registered as a user-only tool, meaning it requires user authentication
- The tool validates HTTP methods and only allows GET, POST, PUT, DELETE
- All requests are logged for debugging purposes
- HTTPS URLs are recommended for secure communications