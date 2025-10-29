# Tài liệu giao thức truyền thông WebSocket chi tiết

（Tiếng Việt | [中文](websocket_zh.md)）

Dưới đây là tài liệu giao thức truyền thông WebSocket được tổng hợp dựa trên triển khai mã nguồn, mô tả cách thiết bị và server tương tác thông qua WebSocket.

Tài liệu này chỉ dựa trên suy luận từ mã nguồn được cung cấp, khi triển khai thực tế có thể cần kết hợp với triển khai phía server để xác nhận thêm hoặc bổ sung.

---

## 1. Tổng quan quy trình chung

1. **Khởi tạo phía thiết bị**  
   - Thiết bị được cấp nguồn, khởi tạo `Application`:  
     - Khởi tạo codec âm thanh, màn hình hiển thị, LED, v.v.  
     - Kết nối mạng  
     - Tạo và khởi tạo instance giao thức WebSocket triển khai interface `Protocol` (`WebsocketProtocol`)  
   - Vào vòng lặp chính chờ sự kiện (đầu vào âm thanh, đầu ra âm thanh, tác vụ lên lịch, v.v.).

2. **Thiết lập kết nối WebSocket**  
   - Khi thiết bị cần bắt đầu phiên thoại (ví dụ: người dùng đánh thức, kích hoạt nút thủ công, v.v.), gọi `OpenAudioChannel()`:  
     - Lấy URL WebSocket theo cấu hình
     - Đặt một số header yêu cầu (`Authorization`, `Protocol-Version`, `Device-Id`, `Client-Id`)  
     - Gọi `Connect()` để thiết lập kết nối WebSocket với server  

3. **Thiết bị gửi tin nhắn "hello"**  
   - Sau khi kết nối thành công, thiết bị sẽ gửi một tin nhắn JSON, ví dụ cấu trúc như sau:  
   ```json
   {
     "type": "hello",
     "version": 1,
     "features": {
       "mcp": true
     },
     "transport": "websocket",
     "audio_params": {
       "format": "opus",
       "sample_rate": 16000,
       "channels": 1,
       "frame_duration": 60
     }
   }
   ```
   - Trong đó trường `features` là tùy chọn, nội dung được tạo tự động theo cấu hình biên dịch thiết bị. Ví dụ: `"mcp": true` biểu thị hỗ trợ giao thức MCP.
   - Giá trị `frame_duration` tương ứng với `OPUS_FRAME_DURATION_MS` (ví dụ 60ms).

4. **Server phản hồi "hello"**  
   - Thiết bị chờ server trả về tin nhắn JSON chứa `"type": "hello"`, và kiểm tra `"transport": "websocket"` có khớp không.  
   - Server có thể tùy chọn gửi trường `session_id`, thiết bị nhận được sẽ tự động ghi lại.  
   - Ví dụ:
   ```json
   {
     "type": "hello",
     "transport": "websocket",
     "session_id": "abc123"
   }
   ```

5. **Giao tiếp hai chiều**  
   - Sau khi hoàn thành bắt tay, thiết bị và server có thể:  
     - **Gửi âm thanh**: Thiết bị thu âm từ microphone, mã hóa Opus rồi gửi dưới dạng frame nhị phân cho server.  
     - **Nhận âm thanh**: Server gửi dữ liệu âm thanh TTS đã mã hóa Opus, thiết bị nhận và phát qua loa.  
     - **Trao đổi JSON**: Hai bên trao đổi tin nhắn JSON để thực hiện các chức năng như nhận dạng giọng nói, điều khiển thiết bị, cấu hình, v.v.  

   - Khi nhận tin nhắn từ server qua callback `OnMessage()`:  
     - Khi `binary` là `true`, coi là frame âm thanh; thiết bị sẽ coi như dữ liệu Opus để giải mã.  
     - Khi `binary` là `false`, coi là văn bản JSON, cần phân tích bằng cJSON ở phía thiết bị và xử lý logic nghiệp vụ tương ứng (như chat, TTS, tin nhắn giao thức MCP, v.v.).  

   - Khi server hoặc mạng bị ngắt kết nối, callback `OnDisconnected()` được kích hoạt:  
     - Thiết bị sẽ gọi `on_audio_channel_closed_()`, và cuối cùng trở về trạng thái nhàn rỗi.

6. **Đóng kết nối WebSocket**  
   - Thiết bị khi cần kết thúc phiên thoại, sẽ gọi `CloseAudioChannel()` để chủ động ngắt kết nối và trở về trạng thái nhàn rỗi.  
   - Hoặc nếu server chủ động ngắt kết nối, cũng sẽ kích hoạt cùng quy trình callback.

---

## 2. Header yêu cầu chung

Khi thiết lập kết nối WebSocket, trong ví dụ mã nguồn đã đặt các header yêu cầu sau:

- `Authorization`: Dùng để chứa access token, dạng `"Bearer <token>"`  
- `Protocol-Version`: Số phiên bản giao thức, giữ nhất quán với trường `version` trong nội dung tin nhắn hello  
- `Device-Id`: Địa chỉ MAC card mạng vật lý thiết bị
- `Client-Id`: UUID được tạo bởi phần mềm (sẽ reset khi xóa NVS hoặc nạp lại toàn bộ firmware)

Các header này sẽ được gửi cùng với bắt tay WebSocket đến server, server có thể thực hiện xác thực, authentication, v.v. theo nhu cầu.

---

## 3. Phiên bản giao thức nhị phân

Thiết bị hỗ trợ nhiều phiên bản giao thức nhị phân, được chỉ định thông qua trường `version` trong cấu hình:

### 3.1 Phiên bản 1 (mặc định)
Gửi trực tiếp dữ liệu âm thanh Opus, không có metadata bổ sung. Giao thức Websocket sẽ phân biệt text và binary.

### 3.2 Phiên bản 2
Sử dụng cấu trúc `BinaryProtocol2`:
```c
struct BinaryProtocol2 {
    uint16_t version;        // Phiên bản giao thức
    uint16_t type;           // Loại tin nhắn (0: OPUS, 1: JSON)
    uint32_t reserved;       // Trường dự trữ
    uint32_t timestamp;      // Timestamp (millisecond, dùng cho AEC phía server)
    uint32_t payload_size;   // Kích thước payload (byte)
    uint8_t payload[];       // Dữ liệu payload
} __attribute__((packed));
```

### 3.3 Phiên bản 3
Sử dụng cấu trúc `BinaryProtocol3`:
```c
struct BinaryProtocol3 {
    uint8_t type;            // Loại tin nhắn
    uint8_t reserved;        // Trường dự trữ
    uint16_t payload_size;   // Kích thước payload
    uint8_t payload[];       // Dữ liệu payload
} __attribute__((packed));
```

---

## 4. Cấu trúc tin nhắn JSON

WebSocket text frame truyền tải theo cách JSON, dưới đây là các trường `"type"` thường gặp và logic nghiệp vụ tương ứng. Nếu tin nhắn chứa các trường không được liệt kê, có thể là tùy chọn hoặc chi tiết triển khai cụ thể.

### 4.1 Thiết bị → Server

**Tin nhắn Hello**
```json
{
  "type": "hello",
  "version": 1,
  "features": {
    "mcp": true
  },
  "transport": "websocket",
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1,
    "frame_duration": 60
  }
}
```

**Tin nhắn trạng thái thiết bị**
```json
{
  "type": "device_status",
  "status": "idle", // hoặc "listening", "speaking", "thinking"
  "battery": 85,
  "volume": 50
}
```

**Tin nhắn MCP**
```json
{
  "type": "mcp",
  "session_id": "abc123",
  "payload": {
    "jsonrpc": "2.0",
    "method": "tools/list",
    "params": {},
    "id": 1
  }
}
```

### 4.2 Server → Thiết bị

**Phản hồi Hello**
```json
{
  "type": "hello",
  "transport": "websocket",
  "session_id": "abc123"
}
```

**Tin nhắn TTS**
```json
{
  "type": "tts",
  "text": "Xin chào, tôi có thể giúp gì cho bạn?",
  "voice_id": "vi-VN-female",
  "speaking_rate": 1.0
}
```

**Tin nhắn điều khiển**
```json
{
  "type": "control",
  "command": "set_volume",
  "parameters": {
    "volume": 70
  }
}
```

---

## 5. Mã hóa giải mã âm thanh

Thiết bị sử dụng codec Opus để mã hóa và giải mã âm thanh:
- **Mã hóa**: Dữ liệu âm thanh từ microphone được mã hóa Opus trước khi gửi
- **Giải mã**: Dữ liệu Opus từ server được giải mã trước khi phát qua loa
- **Tham số**: Sample rate 16kHz, 1 channel, frame duration thường là 60ms

---

## 6. Luồng chuyển trạng thái thường gặp

### Chế độ tự động - Luồng trạng thái
```
Nhàn rỗi → Đang nghe → Đang xử lý → Đang nói → Nhàn rỗi
   ↑                                              ↓
   ←――――――――――――――――――――――――――――――――――――――――――――――――――――
```

### Chế độ thủ công - Luồng trạng thái  
```
Nhàn rỗi → [Nút bấm] → Đang nghe → [Nhả nút] → Đang xử lý → Đang nói → Nhàn rỗi
   ↑                                                                      ↓
   ←―――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――――
```

---

## 7. Xử lý lỗi

- **Lỗi kết nối**: Thiết bị tự động thử kết nối lại sau khoảng thời gian định sẵn
- **Lỗi âm thanh**: Log lỗi và tiếp tục hoạt động với frame tiếp theo  
- **Lỗi JSON**: Bỏ qua tin nhắn không hợp lệ và log cảnh báo
- **Timeout**: Thiết lập timeout cho các hoạt động mạng để tránh treo

---

## 8. Lưu ý khác

- **Bảo mật**: Sử dụng WSS (WebSocket Secure) trong môi trường production
- **Hiệu suất**: Tối ưu hóa kích thước buffer âm thanh để giảm độ trễ
- **Tương thích**: Đảm bảo phiên bản giao thức khớp giữa thiết bị và server
- **Debug**: Sử dụng log để theo dõi luồng tin nhắn và trạng thái kết nối

---

## 9. Ví dụ tin nhắn

### Phiên làm việc hoàn chỉnh
```json
// 1. Thiết bị gửi hello
{
  "type": "hello",
  "version": 1,
  "transport": "websocket",
  "audio_params": {"format": "opus", "sample_rate": 16000}
}

// 2. Server phản hồi hello
{
  "type": "hello", 
  "transport": "websocket",
  "session_id": "session_123"
}

// 3. Server gửi TTS
{
  "type": "tts",
  "text": "Xin chào! Tôi có thể giúp gì cho bạn?"
}

// 4. Thiết bị cập nhật trạng thái  
{
  "type": "device_status",
  "status": "listening"
}

// 5. Thiết bị gửi âm thanh (binary frame)
// [Dữ liệu Opus nhị phân]

// 6. Server phản hồi kết quả STT
{
  "type": "stt_result", 
  "text": "Thời tiết hôm nay như thế nào?"
}
```

---

## 10. Tổng kết

Giao thức này thực hiện truyền tải JSON text và binary frame âm thanh qua lớp WebSocket, hoàn thành các chức năng bao gồm upload stream âm thanh, phát âm thanh TTS, nhận dạng giọng nói và quản lý trạng thái, gửi lệnh MCP, v.v. Đặc điểm cốt lõi:

- **Giai đoạn bắt tay**: Gửi `"type":"hello"`, chờ server phản hồi.  
- **Kênh âm thanh**: Sử dụng frame nhị phân mã hóa Opus truyền tải stream giọng nói hai chiều, hỗ trợ nhiều phiên bản giao thức.  
- **Tin nhắn JSON**: Sử dụng `"type"` làm trường cốt lõi để xác định các logic nghiệp vụ khác nhau, bao gồm TTS, STT, MCP, WakeWord, System, Custom, v.v.  
- **Tính mở rộng**: Có thể thêm trường vào tin nhắn JSON theo nhu cầu thực tế, hoặc thực hiện authentication bổ sung trong headers.

Server và thiết bị cần thỏa thuận trước về ý nghĩa trường của các loại tin nhắn, logic thời gian và quy tắc xử lý lỗi để đảm bảo giao tiếp suôn sẻ. Thông tin trên có thể làm tài liệu cơ sở, thuận tiện cho việc kết nối, phát triển hoặc mở rộng sau này.