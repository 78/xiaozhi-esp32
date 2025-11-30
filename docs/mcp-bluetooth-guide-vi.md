# Hướng dẫn Tích hợp Điều khiển Bluetooth KCX_BT_EMITTER qua MCP

Tài liệu này hướng dẫn cách tích hợp module phát Bluetooth KCX_BT_EMITTER V1.7 vào hệ thống Xiaozhi ESP32 sử dụng giao thức MCP (Model Context Protocol).

## 1. Kết nối Phần cứng

Bạn cần kết nối module KCX_BT_EMITTER với ESP32 như sau:

| Chân Module Bluetooth   | Chân ESP32 (Mặc định) | Mô tả                            |
| ----------------------- | --------------------- | -------------------------------- |
| **5V**                  | 5V                    | Nguồn điện                       |
| **GND**                 | GND                   | Nối đất                          |
| **CONNECT** (hoặc PAIR) | **GPIO 18**           | Chân điều khiển kết nối (Output) |
| **LINK**                | **GPIO 19**           | Chân trạng thái kết nối (Input)  |
| **AUDIO_L/R**           | DAC/Audio Out         | Tín hiệu âm thanh                |

> **Lưu ý:** Bạn có thể thay đổi chân GPIO trong file code nếu sơ đồ đi dây của bạn khác.

## 2. Triển khai Code MCP

Chúng tôi đã tích hợp trực tiếp các công cụ điều khiển vào `main/mcp_server.cc`.

### Các thay đổi đã thực hiện:

1.  **Khai báo thư viện và chân GPIO:**
    Đã thêm thư viện `driver/gpio.h` và định nghĩa chân kết nối:

    ```cpp
    #include <driver/gpio.h>

    // Cấu hình chân Bluetooth
    #define BLUETOOTH_CONNECT_PIN GPIO_NUM_18
    #define BLUETOOTH_LINK_PIN    GPIO_NUM_19
    ```

2.  **Khởi tạo GPIO:**
    Trong hàm `McpServer::AddCommonTools`, chúng tôi đã thêm đoạn code để cấu hình:

    - `BLUETOOTH_CONNECT_PIN`: Chế độ Output, mặc định mức High (giả sử kích hoạt mức thấp).
    - `BLUETOOTH_LINK_PIN`: Chế độ Input để đọc trạng thái.

3.  **Thêm các công cụ MCP (Tools):**

    - **`self.bluetooth.connect`**:

      - **Chức năng:** Kích hoạt chế độ kết nối hoặc tìm kiếm thiết bị.
      - **Hoạt động:** Tạo một xung thấp (Low pulse) ngắn (100ms) trên chân CONNECT, tương đương với việc nhấn nút PAIR.

    - **`self.bluetooth.disconnect`**:

      - **Chức năng:** Ngắt kết nối hoặc xóa bộ nhớ ghép nối.
      - **Hoạt động:** Giữ chân CONNECT ở mức thấp trong 3 giây (Long press).

    - **`self.bluetooth.get_status`**:
      - **Chức năng:** Kiểm tra xem Bluetooth đã kết nối chưa.
      - **Hoạt động:** Đọc trạng thái chân LINK. (High = Connected, Low = Disconnected).

## 3. Cách sử dụng

Sau khi nạp code và khởi động lại thiết bị, bạn có thể ra lệnh bằng giọng nói hoặc qua giao diện chat:

- **Kết nối:** "Hãy kết nối bluetooth" hoặc "Bật bluetooth".
- **Kiểm tra:** "Kiểm tra trạng thái bluetooth" hoặc "Bluetooth đã kết nối chưa?".
- **Ngắt kết nối:** "Ngắt kết nối bluetooth" hoặc "Tắt bluetooth".

## 4. Tùy chỉnh

Để thay đổi chân GPIO, hãy mở file `main/mcp_server.cc` và sửa các dòng sau ở đầu file:

```cpp
#define BLUETOOTH_CONNECT_PIN GPIO_NUM_18 // Đổi số 18 thành chân bạn dùng
#define BLUETOOTH_LINK_PIN    GPIO_NUM_19 // Đổi số 19 thành chân bạn dùng
```

## 5. Luồng hoạt động của MCP điều khiển phần cứng

Để tự triển khai thêm các điều khiển phần cứng khác, bạn có thể làm theo quy trình sau:

1.  **Xác định phần cứng:** Chọn chân GPIO và cách thức điều khiển (High/Low, PWM, v.v.).
2.  **Viết code điều khiển:** Sử dụng các hàm của ESP-IDF như `gpio_set_level`, `gpio_get_level`.
3.  **Đăng ký Tool trong `McpServer::AddCommonTools`:**
    - Đặt tên tool (ví dụ: `self.relay.turn_on`).
    - Viết mô tả rõ ràng để AI hiểu khi nào cần dùng.
    - Định nghĩa tham số (nếu có).
    - Viết hàm xử lý (lambda function) thực hiện hành động phần cứng.

Ví dụ mẫu:

```cpp
AddTool("self.my_device.action",
        "Mô tả hành động",
        PropertyList(),
        [](const PropertyList &properties) -> ReturnValue {
            // Code điều khiển phần cứng ở đây
            return "Thành công";
        });
```
