# Web Server và OTA trên Xiaozhi Firmware

Tài liệu này mô tả chi tiết cách hệ thống Web Server và OTA hiện tại hoạt động, đồng thời hướng dẫn cách tùy biến (custom) để thêm tính năng cài đặt URL OTA riêng.

---

## 1. Tổng quan hệ thống hiện tại

### 1.1. Web Server hoạt động như thế nào?

Hệ thống sử dụng thư viện `esp_http_server` của ESP-IDF để chạy một web server nhẹ ngay trên chip ESP32.

- **Khởi tạo:** Server được khởi tạo trong `main/ota_server.cc` thông qua hàm `OtaServer::Start(int port)`. Mặc định lắng nghe ở cổng 80.
- **Giao diện (Frontend):** Các file HTML (như `ota_index.html`) không được lưu dưới dạng file trên thẻ nhớ mà được **nhúng trực tiếp vào Firmware** dưới dạng mảng byte (binary) trong quá trình biên dịch (sử dụng tính năng `EMBED_FILES` của CMake).
  - Trong code C++, chúng được truy cập qua biến ngoại lai: `extern const uint8_t ota_index_html_start[]`.
- **Xử lý (Backend):** Các đường dẫn (URI) được đăng ký với các hàm xử lý (Handler).
  - `GET /ota`: Trả về nội dung file HTML.
  - `POST /ota_upload`: Nhận file firmware upload từ trình duyệt.

### 1.2. Cơ chế lưu trữ cấu hình (NVS)

Khi người dùng thay đổi cấu hình trên Web (ví dụ: Wifi, MQTT), dữ liệu không được lưu vào file text mà được ghi vào vùng nhớ **NVS (Non-Volatile Storage)** trong Flash.

**Quy trình lưu dữ liệu:**

1.  **Web (Client):** Gửi HTTP POST request chứa dữ liệu (JSON hoặc Form data).
2.  **RAM (Server):** Chip nhận dữ liệu vào bộ đệm.
3.  **NVS Wrapper:** Class `Settings` (`main/settings.cc`) được dùng để đơn giản hóa việc gọi API của ESP-IDF.
4.  **Flash:** Hàm `nvs_set_str` và `nvs_commit` sẽ ghi dữ liệu vĩnh viễn xuống chip.

### 1.3. Quy trình OTA (Over-The-Air)

Logic OTA nằm trong `main/ota.cc`.

1.  **Check Version:** Thiết bị gửi thông tin (MAC, Version hiện tại) lên Server.
2.  **Response:** Server trả về JSON chứa:
    - Version mới nhất.
    - URL tải firmware.
    - Cấu hình MQTT/WebSocket mới (nếu có).
3.  **Upgrade:** Nếu có version mới, thiết bị tải file `.bin` từ URL và ghi vào phân vùng OTA tiếp theo.

---

## 2. Hướng dẫn Custom: Thêm tính năng cài đặt URL OTA tùy chỉnh (Local OTA Server)

Mục tiêu: Thêm một ô nhập liệu trên trang Web cấu hình để người dùng có thể trỏ thiết bị sang Server OTA riêng của họ.

### Bước 1: Sửa đổi Frontend (HTML/JS)

Bạn cần sửa file HTML gốc (ví dụ `main/web/ota_index.html` hoặc tạo mới nếu dự án đang dùng file binary có sẵn).

Thêm đoạn mã sau vào vị trí mong muốn trong thẻ `<body>`:

```html
<div class="card">
  <h3>Cấu hình OTA Server Riêng</h3>
  <div class="input-group">
    <label>OTA URL:</label>
    <input
      type="text"
      id="custom_ota_url"
      placeholder="https://your-server.com/ota/"
      style="width: 100%;"
    />
  </div>
  <button onclick="saveOtaUrl()" class="btn">Lưu Cấu Hình</button>
</div>

<script>
  function saveOtaUrl() {
    var url = document.getElementById("custom_ota_url").value;
    if (url.length < 10) {
      alert("URL không hợp lệ (quá ngắn)!");
      return;
    }

    // Gửi request xuống chip
    fetch("/api/save_ota_url", {
      method: "POST",
      body: url,
    })
      .then((response) => {
        if (response.ok) {
          alert("Đã lưu thành công! Vui lòng khởi động lại thiết bị.");
        } else {
          alert("Lỗi khi lưu cấu hình!");
        }
      })
      .catch((error) => alert("Lỗi kết nối: " + error));
  }
</script>
```

### Bước 2: Cập nhật Backend Header (`main/ota_server.h`)

Khai báo hàm xử lý mới trong class `OtaServer`.

```cpp
class OtaServer {
public:
    // ... (giữ nguyên code cũ)

private:
    // ... (giữ nguyên code cũ)

    // THÊM DÒNG NÀY
    static esp_err_t HandleSaveOtaUrl(httpd_req_t* req);
};
```

### Bước 3: Cập nhật Backend Implementation (`main/ota_server.cc`)

**1. Đăng ký đường dẫn (URI) mới:**

Trong hàm `OtaServer::Start`:

```cpp
esp_err_t OtaServer::Start(int port) {
    // ... (giữ nguyên code khởi tạo server)

    // Đăng ký URI mới
    httpd_uri_t save_ota_url_uri = {
        .uri = "/api/save_ota_url",   // Phải khớp với fetch() trong JS
        .method = HTTP_POST,
        .handler = HandleSaveOtaUrl,  // Hàm xử lý bên dưới
        .user_ctx = nullptr
    };
    httpd_register_uri_handler(server_handle_, &save_ota_url_uri);

    return ESP_OK;
}
```

**2. Viết hàm xử lý logic:**

Thêm hàm này vào cuối file `ota_server.cc`:

```cpp
#include "settings.h" // Bắt buộc include để dùng NVS

esp_err_t OtaServer::HandleSaveOtaUrl(httpd_req_t* req) {
    char content[256]; // Bộ đệm chứa URL

    // 1. Nhận dữ liệu từ trình duyệt
    // Cắt bớt nếu dữ liệu dài hơn bộ đệm
    size_t recv_size = std::min(req->content_len, sizeof(content) - 1);

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    content[ret] = '\0'; // Kết thúc chuỗi ký tự

    ESP_LOGI(kTag, "Nhận được Custom OTA URL: %s", content);

    // 2. Lưu vào NVS (Flash)
    // "wifi" là namespace (bạn có thể chọn namespace khác nếu muốn)
    // true: cho phép ghi (Read-Write)
    Settings settings("wifi", true);
    settings.SetString("ota_url", std::string(content));

    // 3. Phản hồi OK cho trình duyệt
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
```

### Bước 4: Kiểm tra tích hợp (`main/ota.cc`)

Hệ thống hiện tại đã có sẵn logic để đọc key `ota_url` này. Bạn có thể kiểm tra trong hàm `Ota::GetCheckVersionUrl()`:

```cpp
std::string Ota::GetCheckVersionUrl() {
    Settings settings("wifi", false); // Mở NVS để đọc
    std::string url = settings.GetString("ota_url"); // Đọc key chúng ta vừa lưu

    if (url.empty()) {
        url = CONFIG_OTA_URL; // Nếu không có, dùng mặc định
    }
    return url;
}
```

---

## 3. Phân tích Giao diện Cấu hình Wifi (Captive Portal)

Giao diện cấu hình Wifi đẹp mắt (màu tối, hiệu ứng neon) mà bạn thấy khi kết nối vào Wifi của thiết bị (192.168.4.1) **KHÔNG** nằm trong `main/ota_server.cc`. Nó nằm trong một component riêng biệt.

### 3.1. Vị trí mã nguồn

Giao diện này thuộc về component `TienHuyIoT_esp-wifi-connect`.

- **Frontend (HTML/CSS/JS):** `managed_components/TienHuyIoT_esp-wifi-connect/assets/wifi_configuration.html`
- **Backend (C++):** `managed_components/TienHuyIoT_esp-wifi-connect/wifi_configuration_ap.cc`

### 3.2. Cấu trúc Frontend (`wifi_configuration.html`)

Đây là một file "All-in-One" chứa:

- **HTML:** Cấu trúc trang web, các tab (Wifi Config, Advanced), các form nhập liệu.
- **CSS:** Style giao diện (class `.card`, `.page`, biến màu `--accent`...).
- **JavaScript:**
  - Biến `translations`: Từ điển đa ngôn ngữ (Việt, Anh, Trung...).
  - Hàm `switchTab()`: Chuyển đổi giữa tab Wifi và Advanced.
  - Hàm `submitAdvancedForm()`: Gom dữ liệu từ form Advanced thành JSON và gửi POST lên `/advanced/submit`.

### 3.3. Cấu trúc Backend (`wifi_configuration_ap.cc`)

File này xử lý logic của Web Server khi ở chế độ AP (Access Point).

- **Đăng ký URI:**
  - `/`: Trả về nội dung file `wifi_configuration.html`.
  - `/advanced/config` (GET): Trả về JSON chứa cấu hình hiện tại (để điền vào form khi mới load trang).
  - `/advanced/submit` (POST): Nhận JSON từ form Advanced và lưu vào NVS.

### 3.4. Quy trình phát triển tính năng mới (Dev Flow)

Để thêm một tính năng mới vào trang cấu hình này (ví dụ: thêm ô nhập "API Token"), bạn hãy làm theo các bước sau:

#### Bước 0: Chuẩn bị môi trường (Quan trọng)

Vì code nằm trong `managed_components`, nó sẽ bị ghi đè nếu bạn update component.
**Giải pháp:** Copy thư mục `TienHuyIoT_esp-wifi-connect` từ `managed_components/` ra thư mục `components/` (tạo mới ở root dự án). Khi đó, ESP-IDF sẽ ưu tiên dùng bản local của bạn.

#### Bước 1: Sửa Frontend (`wifi_configuration.html`)

1.  Mở file HTML.
2.  Tìm đến `<div id="advanced-tab">`.
3.  Thêm HTML cho ô nhập liệu mới:
    ```html
    <p>
      <label for="api_token">API Token:</label>
      <input type="text" id="api_token" name="api_token" />
    </p>
    ```
4.  (Tùy chọn) Thêm từ khóa vào biến `translations` trong thẻ `<script>` để hỗ trợ đa ngôn ngữ.

#### Bước 2: Sửa Backend - Lưu dữ liệu (`wifi_configuration_ap.cc`)

1.  Tìm đến handler của URI `/advanced/submit`.
2.  Thêm code đọc JSON và lưu NVS:
    ```cpp
    // Trong handler /advanced/submit
    cJSON *token_item = cJSON_GetObjectItem(json, "api_token");
    if (cJSON_IsString(token_item)) {
        // Lưu vào NVS với key "api_token"
        nvs_set_str(nvs, "api_token", token_item->valuestring);
    }
    ```

#### Bước 3: Sửa Backend - Hiển thị dữ liệu cũ (`wifi_configuration_ap.cc`)

1.  Tìm đến handler của URI `/advanced/config`.
2.  Thêm code đọc từ NVS và gửi xuống Web:
    ```cpp
    // Trong handler /advanced/config
    char token[64] = {0};
    size_t len = sizeof(token);
    if (nvs_get_str(nvs, "api_token", token, &len) == ESP_OK) {
        cJSON_AddStringToObject(json, "api_token", token);
    } else {
        cJSON_AddStringToObject(json, "api_token", "");
    }
    ```

#### Bước 4: Build và Flash

Biên dịch lại dự án và nạp xuống chip. Tính năng mới sẽ xuất hiện trên trang cấu hình 192.168.4.1.
