# Quy trình tương tác MCP (Model Context Protocol)

（Tiếng Việt | [中文](mcp-protocol_zh.md)）

LƯU Ý: Được tạo với sự hỗ trợ của AI, khi triển khai dịch vụ backend, vui lòng tham khảo code để xác nhận chi tiết!!

Giao thức MCP trong dự án này được sử dụng để giao tiếp giữa backend API (MCP client) và thiết bị ESP32 (MCP server), để backend có thể khám phá và gọi các chức năng (công cụ) mà thiết bị cung cấp.

## Định dạng giao thức

Theo mã nguồn (`main/protocols/protocol.cc`, `main/mcp_server.cc`), tin nhắn MCP được đóng gói trong phần thân tin nhắn của giao thức truyền thông cơ bản (như WebSocket hoặc MQTT). Cấu trúc bên trong tuân thủ đặc tả [JSON-RPC 2.0](https://www.jsonrpc.org/specification).

Ví dụ cấu trúc tin nhắn tổng thể:

```json
{
  "session_id": "...", // ID phiên làm việc
  "type": "mcp",       // Loại tin nhắn, cố định là "mcp"
  "payload": {         // Payload JSON-RPC 2.0
    "jsonrpc": "2.0",
    "method": "...",   // Tên phương thức (như "initialize", "tools/list", "tools/call")
    "params": { ... }, // Tham số phương thức (cho request)
    "id": ...,         // ID yêu cầu (cho request và response)
    "result": { ... }, // Kết quả thực thi phương thức (cho success response)
    "error": { ... }   // Thông tin lỗi (cho error response)
  }
}
```

Trong đó, phần `payload` là tin nhắn JSON-RPC 2.0 tiêu chuẩn:

- `jsonrpc`: Chuỗi cố định "2.0".
- `method`: Tên phương thức cần gọi (cho Request).
- `params`: Tham số của phương thức, một giá trị có cấu trúc, thường là đối tượng (cho Request).
- `id`: Định danh của yêu cầu, được cung cấp bởi client khi gửi yêu cầu, server trả về nguyên vẹn khi phản hồi. Dùng để ghép nối yêu cầu và phản hồi.
- `result`: Kết quả khi phương thức thực thi thành công (cho Success Response).
- `error`: Thông tin lỗi khi phương thức thực thi thất bại (cho Error Response).

## Quy trình tương tác và thời điểm gửi

Tương tác MCP chủ yếu xoay quanh việc client (backend API) khám phá và gọi các "công cụ" (Tool) trên thiết bị.

1.  **Thiết lập kết nối và thông báo khả năng**

    - **Thời điểm:** Sau khi thiết bị khởi động và kết nối thành công với backend API.
    - **Người gửi:** Thiết bị.
    - **Tin nhắn:** Thiết bị gửi tin nhắn "hello" của giao thức cơ bản cho backend API, tin nhắn chứa danh sách khả năng mà thiết bị hỗ trợ, ví dụ thông qua việc hỗ trợ giao thức MCP (`"mcp": true`).
    - **Ví dụ (không phải MCP payload, mà là tin nhắn giao thức cơ bản):**
      ```json
      {
        "type": "hello",
        "version": ...,
        "features": {
          "mcp": true,
          ...
        },
        "transport": "websocket", // hoặc "mqtt"
        "audio_params": { ... },
        "session_id": "..." // Thiết bị có thể đặt sau khi nhận hello từ server
      }
      ```

2.  **Khởi tạo phiên làm việc MCP**

    - **Thời điểm:** Sau khi backend API nhận tin nhắn "hello" từ thiết bị, xác nhận thiết bị hỗ trợ MCP, thường được gửi như yêu cầu đầu tiên của phiên làm việc MCP.
    - **Người gửi:** Backend API (client).
    - **Phương thức:** `initialize`
    - **Tin nhắn (MCP payload):**

      ```json
      {
        "jsonrpc": "2.0",
        "method": "initialize",
        "params": {
          "capabilities": {
            // Khả năng của client, tùy chọn

            // Liên quan đến thị giác camera
            "vision": {
              "url": "...", // Camera: địa chỉ xử lý hình ảnh (phải là địa chỉ http, không phải websocket)
              "token": "..." // token url
            }

            // ... Khả năng client khác
          }
        },
        "id": 1 // ID yêu cầu
      }
      ```

    - **Thời điểm phản hồi thiết bị:** Sau khi thiết bị nhận và xử lý yêu cầu `initialize`.
    - **Tin nhắn phản hồi thiết bị (MCP payload):**
      ```json
      {
        "jsonrpc": "2.0",
        "id": 1, // Ghép với ID yêu cầu
        "result": {
          "protocolVersion": "2024-11-05",
          "capabilities": {
            "tools": {} // Phần tools ở đây có vẻ không liệt kê thông tin chi tiết, cần tools/list
          },
          "serverInfo": {
            "name": "...", // Tên thiết bị (BOARD_NAME)
            "version": "..." // Phiên bản firmware thiết bị
          }
        }
      }
      ```

3.  **Khám phá danh sách công cụ thiết bị**

    - **Thời điểm:** Khi backend API cần lấy danh sách chức năng (công cụ) cụ thể mà thiết bị hiện hỗ trợ và cách gọi chúng.
    - **Người gửi:** Backend API (client).
    - **Phương thức:** `tools/list`
    - **Tin nhắn (MCP payload):**
      ```json
      {
        "jsonrpc": "2.0",
        "method": "tools/list",
        "params": {
          "cursor": "" // Dùng cho phân trang, yêu cầu đầu tiên là chuỗi rỗng
        },
        "id": 2 // ID yêu cầu
      }
      ```
    - **Thời điểm phản hồi thiết bị:** Sau khi thiết bị nhận yêu cầu `tools/list` và tạo danh sách công cụ.
    - **Tin nhắn phản hồi thiết bị (MCP payload):**
      ```json
      {
        "jsonrpc": "2.0",
        "id": 2, // Ghép với ID yêu cầu
        "result": {
          "tools": [ // Danh sách đối tượng công cụ
            {
              "name": "self.get_device_status",
              "description": "...",
              "inputSchema": { ... } // Schema tham số
            },
            {
              "name": "self.audio_speaker.set_volume",
              "description": "...",
              "inputSchema": { ... } // Schema tham số
            }
            // ... Nhiều công cụ khác
          ],
          "nextCursor": "..." // Nếu danh sách lớn cần phân trang, đây sẽ chứa giá trị cursor cho yêu cầu tiếp theo
        }
      }
      ```
    - **Xử lý phân trang:** Nếu trường `nextCursor` không rỗng, client cần gửi yêu cầu `tools/list` lần nữa với giá trị `cursor` này trong `params` để lấy trang công cụ tiếp theo.

4.  **Gọi công cụ thiết bị**

    - **Thời điểm:** Khi backend API cần thực thi một chức năng cụ thể trên thiết bị.
    - **Người gửi:** Backend API (client).
    - **Phương thức:** `tools/call`
    - **Tin nhắn (MCP payload):**
      ```json
      {
        "jsonrpc": "2.0",
        "method": "tools/call",
        "params": {
          "name": "self.audio_speaker.set_volume", // Tên công cụ cần gọi
          "arguments": {
            // Tham số công cụ, định dạng đối tượng
            "volume": 50 // Tên tham số và giá trị
          }
        },
        "id": 3 // ID yêu cầu
      }
      ```
    - **Thời điểm phản hồi thiết bị:** Sau khi thiết bị nhận yêu cầu `tools/call` và thực thi hàm công cụ tương ứng.
    - **Tin nhắn phản hồi thành công thiết bị (MCP payload):**
      ```json
      {
        "jsonrpc": "2.0",
        "id": 3, // Ghép với ID yêu cầu
        "result": {
          "content": [
            // Nội dung kết quả thực thi công cụ
            { "type": "text", "text": "true" } // Ví dụ: set_volume trả về bool
          ],
          "isError": false // Biểu thị thành công
        }
      }
      ```
    - **Tin nhắn phản hồi lỗi thiết bị (MCP payload):**
      ```json
      {
        "jsonrpc": "2.0",
        "id": 3, // Ghép với ID yêu cầu
        "error": {
          "code": -32601, // Mã lỗi JSON-RPC, ví dụ Method not found (-32601)
          "message": "Unknown tool: self.non_existent_tool" // Mô tả lỗi
        }
      }
      ```

5.  **Thiết bị chủ động gửi tin nhắn (Notifications)**
    - **Thời điểm:** Khi có sự kiện bên trong thiết bị cần thông báo cho backend API (ví dụ: thay đổi trạng thái, mặc dù trong ví dụ code không có công cụ rõ ràng nào gửi loại tin nhắn này, nhưng sự tồn tại của `Application::SendMcpMessage` gợi ý rằng thiết bị có thể chủ động gửi tin nhắn MCP).
    - **Người gửi:** Thiết bị (server).
    - **Phương thức:** Có thể là tên phương thức bắt đầu bằng `notifications/`, hoặc phương thức tùy chỉnh khác.
    - **Tin nhắn (MCP payload):** Tuân thủ định dạng JSON-RPC Notification, không có trường `id`.
      ```json
      {
        "jsonrpc": "2.0",
        "method": "notifications/state_changed", // Tên phương thức ví dụ
        "params": {
          "newState": "idle",
          "oldState": "connecting"
        }
        // Không có trường id
      }
      ```
    - **Xử lý backend API:** Sau khi nhận Notification, backend API thực hiện xử lý tương ứng nhưng không phản hồi.

## Biểu đồ tương tác

Dưới đây là biểu đồ trình tự đơn giản hóa, thể hiện quy trình tin nhắn MCP chính:

```mermaid
sequenceDiagram
    participant Device as ESP32 Device
    participant BackendAPI as Backend API (Client)

    Note over Device, BackendAPI: Thiết lập kết nối WebSocket / MQTT

    Device->>BackendAPI: Hello Message (chứa "mcp": true)

    BackendAPI->>Device: MCP Initialize Request
    Note over BackendAPI: method: initialize
    Note over BackendAPI: params: { capabilities: ... }

    Device->>BackendAPI: MCP Initialize Response
    Note over Device: result: { protocolVersion: ..., serverInfo: ... }

    BackendAPI->>Device: MCP Get Tools List Request
    Note over BackendAPI: method: tools/list
    Note over BackendAPI: params: { cursor: "" }

    Device->>BackendAPI: MCP Get Tools List Response
    Note over Device: result: { tools: [...], nextCursor: ... }

    loop Phân trang tùy chọn
        BackendAPI->>Device: MCP Get Tools List Request
        Note over BackendAPI: method: tools/list
        Note over BackendAPI: params: { cursor: "..." }
        Device->>BackendAPI: MCP Get Tools List Response
        Note over Device: result: { tools: [...], nextCursor: "" }
    end

    BackendAPI->>Device: MCP Call Tool Request
    Note over BackendAPI: method: tools/call
    Note over BackendAPI: params: { name: "...", arguments: { ... } }

    alt Gọi công cụ thành công
        Device->>BackendAPI: MCP Tool Call Success Response
        Note over Device: result: { content: [...], isError: false }
    else Gọi công cụ thất bại
        Device->>BackendAPI: MCP Tool Call Error Response
        Note over Device: error: { code: ..., message: ... }
    end

    opt Thông báo từ thiết bị
        Device->>BackendAPI: MCP Notification
        Note over Device: method: notifications/...
        Note over Device: params: { ... }
    end
```

Tài liệu này tổng quan về quy trình tương tác chính của giao thức MCP trong dự án. Chi tiết tham số cụ thể và chức năng công cụ cần tham khảo `main/mcp_server.cc` trong `McpServer::AddCommonTools` cũng như triển khai của từng công cụ.