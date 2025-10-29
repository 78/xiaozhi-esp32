# Hướng dẫn sử dụng điều khiển IoT với giao thức MCP

（Tiếng Việt | [中文](mcp-usage_zh.md)）

> Tài liệu này giới thiệu cách triển khai điều khiển IoT cho thiết bị ESP32 dựa trên giao thức MCP. Để biết chi tiết quy trình giao thức, vui lòng tham khảo [`mcp-protocol.md`](./mcp-protocol.md).

## Giới thiệu

MCP (Model Context Protocol) là giao thức thế hệ mới được khuyến nghị sử dụng cho điều khiển IoT, thông qua định dạng JSON-RPC 2.0 tiêu chuẩn để phát hiện và gọi các "công cụ" (Tool) giữa backend và thiết bị, thực hiện điều khiển thiết bị linh hoạt.

## Quy trình sử dụng điển hình

1. Sau khi thiết bị khởi động, thiết lập kết nối với backend thông qua giao thức cơ bản (như WebSocket/MQTT).
2. Backend khởi tạo phiên làm việc thông qua phương thức `initialize` của giao thức MCP.
3. Backend lấy tất cả công cụ (chức năng) và mô tả tham số mà thiết bị hỗ trợ thông qua `tools/list`.
4. Backend gọi các công cụ cụ thể thông qua `tools/call` để thực hiện điều khiển thiết bị.

Định dạng giao thức chi tiết và tương tác xem trong [`mcp-protocol.md`](./mcp-protocol.md).

## Hướng dẫn đăng ký công cụ phía thiết bị

Thiết bị đăng ký các "công cụ" có thể được gọi bởi backend thông qua phương thức `McpServer::AddTool`. Chữ ký hàm thường dùng như sau:

```cpp
void AddTool(
    const std::string& name,           // Tên công cụ, khuyến nghị duy nhất và có tính phân cấp, như self.dog.forward
    const std::string& description,    // Mô tả công cụ, giải thích ngắn gọn chức năng để mô hình lớn dễ hiểu
    const PropertyList& properties,    // Danh sách tham số đầu vào (có thể rỗng), hỗ trợ các kiểu: bool, int, string
    std::function<ReturnValue(const PropertyList&)> callback // Callback triển khai khi công cụ được gọi
);
```
- name: Định danh duy nhất của công cụ, khuyến nghị sử dụng phong cách đặt tên "module.function".
- description: Mô tả bằng ngôn ngữ tự nhiên, giúp AI/người dùng dễ hiểu.
- properties: Danh sách tham số, hỗ trợ các kiểu bool, int, string, có thể chỉ định phạm vi và giá trị mặc định.
- callback: Logic thực thi thực tế khi nhận được yêu cầu gọi, giá trị trả về có thể là bool/int/string.

## Ví dụ đăng ký điển hình (lấy ESP-Hi làm ví dụ)

```cpp
void InitializeTools() {
    auto& mcp_server = McpServer::GetInstance();
    // Ví dụ 1: Không có tham số, điều khiển robot tiến lên
    mcp_server.AddTool("self.dog.forward", "Robot di chuyển về phía trước", PropertyList(), [this](const PropertyList&) -> ReturnValue {
        servo_dog_ctrl_send(DOG_STATE_FORWARD, NULL);
        return true;
    });
    // Ví dụ 2: Có tham số, đặt màu RGB cho đèn LED
    mcp_server.AddTool("self.light.set_rgb", "Đặt màu RGB", PropertyList({
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

## Ví dụ JSON-RPC gọi công cụ thường gặp

### 1. Lấy danh sách công cụ
```json
{
  "jsonrpc": "2.0",
  "method": "tools/list",
  "params": { "cursor": "" },
  "id": 1
}
```

### 2. Điều khiển khung gầm tiến lên
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

### 3. Chuyển đổi chế độ đèn LED
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

### 4. Lật camera
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

## Ghi chú
- Tên công cụ, tham số và giá trị trả về vui lòng theo đăng ký `AddTool` phía thiết bị.
- Khuyến nghị tất cả dự án mới thống nhất sử dụng giao thức MCP để điều khiển IoT.
- Giao thức chi tiết và cách sử dụng nâng cao vui lòng xem [`mcp-protocol.md`](./mcp-protocol.md).