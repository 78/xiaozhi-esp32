# Quy trình Tùy chỉnh Từ đánh thức & Nhận lệnh trên Xiaozhi ESP32

Tài liệu này chi tiết hóa việc thực hiện kỹ thuật cách thiết bị Xiaozhi ESP32 giao tiếp với server để nhận lệnh và đặc biệt là cách từ đánh thức được tùy chỉnh động.

## Phần 1: Cách Xiaozhi nhận lệnh từ Server

Việc giao tiếp giữa thiết bị và server `xiaozhi.me` dựa trên một đường ống có cấu trúc qua kết nối WebSocket. Luồng đi của một lệnh từ server đến khi thực thi trên thiết bị như sau:

### 1. Tầng Giao vận (Transport Layer - WebSocket)

- **Giao thức**: Secure WebSocket (WSS).
- **Định dạng**: JSON-RPC 2.0 (được bọc trong cấu trúc JSON riêng của ứng dụng).
- **Điểm nhập**: `main/protocols/websocket_protocol.cc`
  - Hàm `WebsocketProtocol::OpenAudioChannel` thiết lập kết nối.
  - Callback `websocket_->OnData` nhận dữ liệu thô.
  - Nó kiểm tra xem tin nhắn có phải format JSON không.
  - Nếu phát hiện tin nhắn **JSON-RPC 2.0** chuẩn (chứa `"jsonrpc": "2.0"`), nó đóng gói tin nhắn này vào một đối tượng JSON nội bộ với `type: "mcp"`.

### 2. Tầng Ứng dụng (Application Layer - Dispatch)

- **Định tuyến**: `main/application.cc`
  - Lớp `Application` khởi tạo giao thức và đăng ký một callback thông qua `protocol_->OnIncomingJson`.
  - Bên trong callback này, các tin nhắn JSON hợp lệ được kiểm tra trường `type`.
  - Khi `type` là `"mcp"`, mã nguồn trích xuất `payload` và chuyển tiếp nó đến MCP Server.

```cpp
// Trích đoạn từ Application::Start (application.cc)
} else if (strcmp(type->valuestring, "mcp") == 0) {
    auto payload = cJSON_GetObjectItem(root, "payload");
    if (cJSON_IsObject(payload)) {
        McpServer::GetInstance().ParseMessage(payload); // Chuyền tới MCP Server
    }
}
```

### 3. Tầng Thực thi (Execution Layer - MCP Server)

- **Thực thi**: `main/mcp_server.cc`
  - `McpServer::ParseMessage` phân tích cú pháp payload JSON-RPC (xác định `method` và `params`).
  - Nó khớp tên phương thức (ví dụ: `self.assets.set_download_url`) với các công cụ (tools) đã đăng ký.
  - Nếu tìm thấy khớp, hàm lambda C++ tương ứng gắn với công cụ đó sẽ được thực thi.

---

## Phần 2: Quy trình Tùy chỉnh Từ đánh thức Động (Dynamic Wake Word)

Dựa trên cơ sở hạ tầng lệnh đã mô tả ở trên, phần này phác thảo cách từ đánh thức được thay đổi động thông qua giao diện web.

### Nguyên lý Cốt lõi

Khác với firmware truyền thống sử dụng mô hình **WakeNet** tĩnh (yêu cầu huấn luyện lại cho từ mới), dự án này sử dụng engine **MultiNet** (Nhận diện Câu lệnh Giọng nói). Từ đánh thức được coi là một "câu lệnh giọng nói" động, cho phép thay đổi tại runtime bằng cách cập nhật cấu hình văn bản.

### Quy trình Chi tiết

#### 1. Kích hoạt từ phía Server (qua MCP)

Khi người dùng cập nhật từ đánh thức trên `xiaozhi.me`, server sử dụng đường ống MCP mô tả trong Phần 1 để gửi một lệnh cụ thể:

- **Tên Tool**: `self.assets.set_download_url`
- **Payload**: `{ "url": "https://..." }` (Link tới file ZIP chứa cấu hình mới).
- **Tác động**: Thiết bị lưu URL này vào cài đặt thuộc tính.

#### 2. Tải & Giải nén Tài nguyên (Assets)

Vì lệnh yêu cầu cập nhật tài nguyên, máy trạng thái của thiết bị xử lý quy trình:

1. **Đổi trạng thái**: Thiết bị vào trạng thái `"Upgrading"` (Đang nâng cấp).
2. **Tải xuống**: `main/application.cc` (`CheckAssetsVersion`) kích hoạt `Assets::Download`.
3. **Giải nén**: File ZIP chứa `index.json` được giải nén vào phân vùng assets.

#### 3. Cập nhật Cấu hình & Engine

Đây là nơi việc thay đổi "Wake Word" thực sự diễn ra.

- **File chính**: `index.json` (bên trong assets vừa tải).
- **Bộ phân tích**: `main/audio/wake_words/custom_wake_word.cc`.

Lớp `CustomWakeWord` phân tích `index.json` cho cấu hình `multinet_model`:

```json
{
  "multinet_model": {
    "commands": [
      {
        "command": "NI HAO XIAO ZHI",
        "text": "你好小智",
        "action": "wake"
      }
    ]
  }
}
```

- **Cập nhật tại Runtime**:
  1. `esp_mn_commands_clear()`: Xóa các từ đánh thức cũ.
  2. `esp_mn_commands_add()`: Đăng ký từ mới như một câu lệnh.
  3. `esp_mn_commands_update()`: Cam kết (commit) các thay đổi vào engine MultiNet.

#### 4. Vòng lặp Nhận diện

1. Dữ liệu microphone được đưa vào `multinet_->detect`.
2. Nếu engine nhận diện được cụm từ mới và hành động là `"wake"`, nó sẽ kích hoạt callback đánh thức.

### Tóm tắt các File Quan trọng

| Đường dẫn File                              | Trách nhiệm                                                              |
| ------------------------------------------- | ------------------------------------------------------------------------ |
| `main/protocols/websocket_protocol.cc`      | Nhận dữ liệu WebSocket thô và nhận diện JSON-RPC.                        |
| `main/application.cc`                       | Định tuyến tin nhắn loại "mcp" tới MCP server và quản lý tải tài nguyên. |
| `main/mcp_server.cc`                        | Thực thi lệnh `self.assets.set_download_url`.                            |
| `main/audio/wake_words/custom_wake_word.cc` | Ánh xạ cấu hình JSON vào engine Espressif MultiNet.                      |
