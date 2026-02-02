# Báo Cáo Kỹ Thuật: Hệ Thống FOTA & Assets Update

Tài liệu này tổng hợp toàn bộ kiến thức về hệ thống FOTA của Xiaozhi, từ kiến trúc tổng quan, quy trình vận hành đến các chi tiết triển khai mã nguồn và xử lý lỗi kỹ thuật.

---

## 1. Tổng Quan Kiến Trúc Hệ Thống

Hệ thống được thiết kế để đảm bảo cập nhật an toàn cho cả Firmware (Logic) và Assets (Giao diện).

### 1.1. Thành Phần Chính
*   **Device (Robot):** ESP32-S3, hỗ trợ MQTT (Command) và HTTP Client (Download).
*   **OTA Server:**
    *   **Frontend:** Dashboard quản lý, upload file `.bin`.
    *   **Backend:** API Server điều phối.
    *   **MQTT Broker:** `broker.hivemq.com` (Kênh điều khiển).
*   **Storage:** AWS S3 hoặc Local Storage (lưu file firmware/assets).

### 1.2. Bản Đồ Phân Vùng (Partition Map)
Bộ nhớ Flash 16MB được chia như sau để hỗ trợ cơ chế A/B Update:

| Tên | Offset | Kích thước | Vai trò |
| :--- | :--- | :--- | :--- |
| **Factory** | `0x10000` | 1M | Firmware gốc. |
| **OTA Data** | `0xd000` | 8K | Con trỏ boot (trỏ về OTA_0 hoặc OTA_1). |
| **OTA_0** | `0x20000` | ~3.5M | Slot App A. |
| **OTA_1** | ... | ~3.5M | Slot App B. |
| **Assets** | `0x800000` | 8M | Chứa tài nguyên (Ảnh, Font, Core). **Không có cơ chế A/B.** |

---

## 2. Quy Trình Hoạt Động (Workflow)

### 2.1. Cơ chế kích hoạt an toàn
Để tránh update nhầm, device không tự động check update mà cần người dùng kích hoạt:
1.  Người dùng giữ nút **BOOT** trong **3 giây**.
2.  Device ngắt kết nối server cũ, chuyển sang **FOTA Broker**.
3.  Device vào trạng thái chờ lệnh từ Dashboard.

### 2.2. Luồng Update Firmware (Cơ chế A/B)
1.  Server gửi lệnh `ota_url`.
2.  Device tải file `xiaozhi.bin`.
3.  Ghi vào phân vùng OTA rảnh (ví dụ OTA_1).
4.  Verify checksum -> Cập nhật **OTA Data** -> Reboot.
5.  Nếu boot thất bại, bootloader tự quay về OTA_0.

### 2.3. Luồng Update Assets (Cơ chế Direct Write)
1.  Server gửi lệnh `assets_url`.
2.  Device tải file `generated_assets.bin`.
3.  **Xóa và Ghi đè** trực tiếp phân vùng `assets` (Offset 0x800000).
4.  Reboot để áp dụng.

---

## 3. Chi Tiết Triển Khai (Deep Dive)

Phần này mô tả các thay đổi cụ thể trong mã nguồn để hiện thực hệ thống trên.

### 3.1. Xử Lý Nút Bấm & Chuyển Mode
File: `main/boards/.../iotforce_xiaozhi_iot_vietnam_es3n28p_lcd_2.8.cc`

Logic giữ nút 3 giây được xử lý sự kiện `OnLongPress`:
```cpp
boot_button_.OnLongPress([this]() {
    ESP_LOGI(TAG, "Boot button long pressed (3s)");
    display->ShowNotification("Entering FOTA...");
    
    // Switch MQTT Broker
    auto mqtt = dynamic_cast<MqttProtocol*>(app.GetProtocol());
    if (mqtt) mqtt->SwitchToFota("broker.hivemq.com");
});
```

### 3.2. Xử Lý Giao Thức MQTT
File: `main/protocols/mqtt_protocol.cc`

Đã khắc phục lỗi crash khi parse URL MQTT:
```cpp
// Fix Crash: Thêm try-catch khi parse port từ chuỗi URL
try {
    broker_port = std::stoi(cleaned_endpoint.substr(pos + 1));
} catch (const std::exception& e) {
    broker_port = 8883; // Default port
}
```

### 3.3. Application Task & Bộ Nhớ (Quan Trọng)
File: `main/application.cc`

Đây là thay đổi quan trọng nhất để fix lỗi **Stack Overflow**.
*   **Cũ:** Dùng `std::thread` -> Stack mặc định (~4KB) -> Crash khi chạy HTTPS Handshake.
*   **Mới:** Dùng `xTaskCreate` với Stack **10KB**.

```cpp
// Tạo Task cho Firmware OTA
xTaskCreate([](void* arg) {
    // ... logic OTA ...
}, "ota_task", 10240, args, 5, NULL);

// Tạo Task cho Assets OTA (Tương tự)
xTaskCreate([](void* arg) {
    // ... logic Assets ...
}, "assets_task", 10240, args, 5, NULL);
```

### 3.4. Logic Download & Ghi Assets (Architectural View)
Tham khảo từ `OTA_GUIDE_VN.md`, logic cốt lõi để tải và ghi assets được tổ chức như sau. Trong thực tế, class `Assets` đảm nhiệm việc này:

```cpp
bool Assets::Download(std::string url, std::function<void(int, size_t)> callback) {
    // 1. Khởi tạo HTTP Client
    esp_http_client_config_t config = { .url = url.c_str(), ... };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    // 2. Tìm phân vùng Assets (Offset 0x800000)
    const esp_partition_t* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "assets");
    
    // 3. Xoá & Ghi dữ liệu (Stream)
    while (true) {
        int read_len = esp_http_client_read(client, buffer, BUFFER_SIZE);
        if (read_len <= 0) break;
        
        // Ghi trực tiếp vào Flash
        esp_partition_write(partition, offset, buffer, read_len);
        offset += read_len;
        
        // Callback cập nhật tiến độ
        callback(percent, speed);
    }
}
```

### 3.5. Cấu trúc Bản Tin JSON Mở Rộng
Để hỗ trợ cả 2 loại cập nhật, cấu trúc JSON điều khiển từ Server được thiết kế linh hoạt:

```json
// Update Firmware
{
    "type": "ota_url",
    "url": "http://server/xiaozhi.bin"
}

// Update Assets (Mới)
{
    "type": "assets_url",
    "url": "http://server/generated_assets.bin" // Server tự động tìm file này
}
```

---

## 4. Phân Tích Các Lỗi Đã Gặp & Cách Fix

### Lỗi 1: `ESP_ERR_INVALID_SIZE` (Lỗi 40% dừng)
*   **Triệu chứng:** Quá trình nạp chạy được một lúc rồi báo lỗi Invalid Size.
*   **Nguyên nhân:** Người dùng nạp nhầm file **`merged-binary.bin`** (9MB). Partition OTA chỉ có 3.5MB nên không chứa nổi.
*   **Giải pháp:** BẮT BUỘC dùng file **`xiaozhi.bin`** (chỉ chứa App code, ~2MB).

### Lỗi 2: Stack Overflow (Guru Meditation Error)
*   **Triệu chứng:** Robot reset ngay khi bắt đầu tải ("Starting OTA...").
*   **Nguyên nhân:** Tác vụ HTTPS yêu cầu bộ nhớ Stack lớn cho TLS (SSL), thread mặc định không đủ.
*   **Giải pháp:** Tăng Stack lên `10KB` (10240 bytes) như mục 3.3.

### Lỗi 3: Mất Assets sau khi nạp Firmware
*   **Nguyên nhân:** Nạp file `merged-binary.bin` qua dây (Wired Flash) sẽ xoá sạch toàn bộ Flash, bao gồm cả phân vùng Assets.
*   **Giải pháp:** Sau khi nạp dây, phải nạp lại cả Assets. Khi FOTA, partitions độc lập nên an toàn.

---

## 5. Hướng Dẫn Vận Hành (Dashboard)

1.  **Truy cập:** `http://localhost:4001`.
2.  **Chọn Robot:** Bấm "Update FOTA".
3.  **Tải file Firmware:**
    *   Chọn file `build/xiaozhi.bin`.
    *   Bấm "Upload & Push".
4.  **Tải file Assets:**
    *   Chọn file `build/generated_assets.bin`.
    *   Bấm "Upload & Push".
5.  **Lưu ý:** Có thể nạp cả 2 cùng lúc (Server sẽ gửi 2 lệnh nối tiếp).
