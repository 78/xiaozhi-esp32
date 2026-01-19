# Hướng dẫn Cơ chế OTA và Cập nhật Giao diện (Assets) cho Xiaozhi

Tài liệu này mô tả chi tiết cách thức hoạt động của hệ thống OTA (Over-The-Air) trên thiết bị Xiaozhi, bao gồm cập nhật Firmware và Giao diện (Assets). Đồng thời, tài liệu cũng hướng dẫn cách mở rộng tính năng để hỗ trợ cập nhật Assets qua URL và thêm URL dự phòng.

## 1. Tổng quan về hệ thống OTA hiện tại

Hệ thống OTA của Xiaozhi hiện tại bao gồm hai phần chính:

1.  **OTA Server (Local):** Một web server chạy trên thiết bị, cho phép người dùng tải lên firmware hoặc assets thủ công qua trình duyệt web (trong mạng LAN).
2.  **OTA Client (Remote):** Thiết bị tự động kiểm tra phiên bản mới từ một URL cấu hình trước và tải xuống firmware nếu có bản cập nhật.

### Cấu trúc Partition

Xiaozhi sử dụng một partition riêng biệt tên là `assets` để lưu trữ tài nguyên giao diện (hình ảnh, âm thanh, font chữ). Partition này không sử dụng hệ thống tệp thông thường (như SPIFFS/LittleFS) mà sử dụng cấu trúc dữ liệu tùy chỉnh được map trực tiếp vào bộ nhớ (memory mapped) để truy xuất nhanh.

## 2. Cơ chế cập nhật Firmware (Hiện có)

### Quy trình:

1.  Thiết bị gửi request GET/POST tới `ota_url` (được lưu trong Settings hoặc `CONFIG_OTA_URL`).
2.  Server trả về JSON chứa thông tin phiên bản.
3.  Thiết bị so sánh phiên bản hiện tại với phiên bản trong JSON.
4.  Nếu có phiên bản mới, thiết bị tải file `.bin` từ `url` trong JSON và ghi vào partition `app` tiếp theo.

### Cấu trúc JSON phản hồi:

```json
{
    "firmware": {
        "version": "1.0.1",
        "url": "http://your-server.com/firmware.bin",
        "force": 0
    },
    "activation": { ... },
    "mqtt": { ... }
}
```

## 3. Cơ chế cập nhật Assets (Giao diện)

### Hiện tại (Local Upload):

Hiện tại, việc cập nhật Assets chủ yếu được thực hiện thủ công qua Web Interface:

- Truy cập `http://<IP-Device>/assets`
- Upload file `assets.bin`.
- Hàm `OtaServer::HandleAssetsUpload` sẽ xóa và ghi đè dữ liệu vào partition `assets`.

### Giải pháp cập nhật Assets qua URL (Yêu cầu mới):

Để thiết bị có thể tự động tải và cập nhật Assets từ URL (giống như Firmware), cần thực hiện các thay đổi sau trong mã nguồn:

#### Bước 1: Mở rộng cấu trúc JSON

Thêm trường `assets` vào phản hồi JSON từ server OTA:

```json
{
    "firmware": { ... },
    "assets": {
        "version": "2.0",
        "url": "http://your-server.com/assets.bin"
    }
}
```

#### Bước 2: Cập nhật Logic `Ota::CheckVersion` (file `main/ota.cc`)

Cần sửa đổi hàm này để parse thêm block `assets`:

```cpp
// Trong Ota::CheckVersion
cJSON *assets = cJSON_GetObjectItem(root, "assets");
if (cJSON_IsObject(assets)) {
    // Parse version và url của assets
    // So sánh với version assets hiện tại (cần lưu version assets vào NVS hoặc file)
    // Nếu mới hơn -> Gọi hàm UpgradeAssets
}
```

#### Bước 3: Hiện thực hàm `Ota::UpgradeAssets`

Viết thêm hàm mới trong class `Ota` để tải và ghi assets:

```cpp
bool Ota::UpgradeAssets(const std::string &assets_url) {
    // 1. Khởi tạo HTTP Client tải file từ assets_url
    // 2. Tìm partition "assets" (esp_partition_find_first)
    // 3. Xóa partition (esp_partition_erase_range)
    // 4. Đọc dữ liệu từ HTTP và ghi vào partition (esp_partition_write)
    // 5. Kiểm tra checksum sau khi ghi xong
    // 6. Restart thiết bị
}
```

## 4. Thêm URL OTA phụ (Backup URL)

Để thêm một URL OTA thứ hai (dự phòng hoặc kênh khác), bạn có thể thực hiện theo cách sau:

### Cách 1: Fallback URL (Tự động chuyển đổi)

Sửa đổi logic trong `Ota::CheckVersion`:

1.  Thử kết nối tới `Primary URL`.
2.  Nếu thất bại (timeout, 404, DNS error), tự động thử kết nối tới `Secondary URL`.

**Implementation:**

```cpp
std::vector<std::string> ota_urls = { primary_url, secondary_url };
for (const auto& url : ota_urls) {
    if (CheckVersion(url)) {
        return true; // Thành công
    }
    ESP_LOGW(TAG, "Failed to check version from %s, trying next...", url.c_str());
}
```

### Cách 2: Cấu hình qua Settings

Cho phép người dùng cấu hình nhiều URL trong `Settings` (NVS) và thiết bị sẽ ưu tiên URL nào được chọn.

## 5. Tóm tắt các file cần chỉnh sửa

- `main/ota.h`: Khai báo hàm `UpgradeAssets` và các biến thành viên mới.
- `main/ota.cc`:
  - Cập nhật `CheckVersion` để parse JSON assets.
  - Hiện thực `UpgradeAssets` (tham khảo logic ghi partition từ `main/ota_server.cc` và logic tải HTTP từ `Upgrade`).
  - Cập nhật logic retry/fallback cho URL.
- `main/assets.h` / `main/assets.cc`: Có thể cần thêm hàm `GetVersion()` để lấy phiên bản assets hiện tại (nếu assets có lưu version trong header hoặc metadata).

---

_Tài liệu được tạo bởi GitHub Copilot - Tech Lead Assistant_
