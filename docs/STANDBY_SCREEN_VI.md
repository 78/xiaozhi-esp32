# Tài liệu Triển khai Màn hình Chờ (Standby Screen)

Tài liệu này mô tả chi tiết cách thức triển khai, kiến trúc, và các biện pháp quản lý bộ nhớ cho tính năng màn hình chờ hiển thị thông tin thời tiết, ngày giờ (Dương lịch/Âm lịch) và môi trường trên thiết bị Xiaozhi ESP32.

## 1. Tổng quan Kiến trúc

Tính năng được chia thành 3 thành phần chính để đảm bảo sự tách biệt trách nhiệm (Separation of Concerns):

1.  **`WeatherService` (Backend)**:

    - Chịu trách nhiệm giao tiếp với API OpenWeatherMap.
    - Xử lý dữ liệu thô (JSON) và chuyển đổi sang cấu trúc dữ liệu nội bộ.
    - Quản lý luồng (Thread-safe) để đảm bảo dữ liệu không bị lỗi khi truy cập từ nhiều tác vụ khác nhau.
    - Được thiết kế theo mẫu **Singleton** để chỉ có một thể hiện duy nhất tồn tại trong toàn bộ vòng đời ứng dụng.

2.  **`WeatherUI` (Frontend)**:

    - Sử dụng thư viện đồ họa **LVGL** để vẽ giao diện.
    - Quản lý các đối tượng hiển thị (Label, Container).
    - Áp dụng phong cách thiết kế Neon Purple (Tím Neon) đồng bộ với Web Config.
    - Xử lý việc ẩn/hiện giao diện để tiết kiệm tài nguyên khi không cần thiết.

3.  **`Application` (Controller)**:
    - Điều phối hoạt động: Khi thiết bị ở trạng thái `IDLE` (Rảnh) và không phát nhạc, nó sẽ kích hoạt màn hình chờ.
    - Lên lịch cập nhật dữ liệu định kỳ (mỗi giây cho đồng hồ, mỗi 30 phút cho thời tiết).

## 2. Các Khái niệm & Kỹ thuật Sử dụng

### 2.1. FreeRTOS Tasks & Multithreading

Việc gọi API qua mạng (HTTP Request) là một tác vụ tốn thời gian và có thể chặn (block) luồng chính của hệ thống. Do đó, việc lấy dữ liệu thời tiết được thực hiện trong một **Task riêng biệt** của FreeRTOS.

- **Cú pháp tạo Task**:
  ```cpp
  xTaskCreate([](void *arg) {
      // Code xử lý nặng
      vTaskDelete(NULL); // Tự hủy khi xong
  }, "task_name", 4096, NULL, 5, NULL);
  ```

### 2.2. Thread Safety (An toàn luồng)

Khi một Task đang ghi dữ liệu vào biến `weather_info_` và Task hiển thị màn hình lại đọc biến đó cùng lúc, lỗi sẽ xảy ra.

- **Giải pháp**: Sử dụng `std::mutex` và `std::lock_guard`.
- **Cơ chế**: Khi `lock_guard` được khởi tạo, nó sẽ "khóa" tài nguyên. Khi `lock_guard` ra khỏi phạm vi (hết hàm hoặc hết block `{}`), nó tự động "mở khóa".

### 2.3. Singleton Pattern

Đảm bảo `WeatherService` có thể được truy cập từ bất kỳ đâu trong mã nguồn (Application, UI, v.v.) mà không cần truyền con trỏ đi khắp nơi.

```cpp
static WeatherService& GetInstance() {
    static WeatherService instance;
    return instance;
}
```

### 2.4. JSON Parsing

Sử dụng thư viện `cJSON` để phân tích chuỗi phản hồi từ API.

- **Lưu ý quan trọng**: `cJSON` cấp phát bộ nhớ động (malloc). Bắt buộc phải gọi `cJSON_Delete()` sau khi sử dụng để tránh rò rỉ bộ nhớ.

## 3. Giải thuật & Xử lý Memory Leak (Rò rỉ bộ nhớ)

Đây là phần quan trọng nhất để đảm bảo thiết bị không bị Reset đột ngột sau thời gian dài hoạt động.

### 3.1. Ngăn chặn "Task Pile-up" (Chồng chất tác vụ)

**Vấn đề**: Nếu mạng chậm, tác vụ lấy thời tiết có thể mất 10 giây mới xong. Nếu ta lập lịch gọi mỗi 5 giây, các tác vụ sẽ chồng lên nhau, mỗi tác vụ chiếm 4KB RAM -> Hết RAM -> Crash.

**Giải pháp**: Sử dụng cờ nguyên tử `std::atomic<bool> is_fetching_`.

```cpp
// Trong Application.cc
if (!ws.IsFetching()) { // Chỉ tạo task mới nếu task cũ đã xong
    xTaskCreate(...);
}

// Trong WeatherService.cc
bool FetchWeatherData() {
    if (is_fetching_) return false; // Chặn ngay từ cửa vào
    is_fetching_ = true;
    // ... Xử lý ...
    is_fetching_ = false; // Mở lại khi xong
    return true;
}
```

### 3.2. Quản lý bộ nhớ Heap với cJSON và HTTP Client

Mọi cấp phát động phải có giải phóng tương ứng.

```cpp
// 1. HTTP Client
esp_http_client_handle_t client = esp_http_client_init(&config);
// ... thực hiện request ...
esp_http_client_cleanup(client); // BẮT BUỘC: Giải phóng client

// 2. cJSON
cJSON *json = cJSON_Parse(response.c_str());
if (json) {
    // ... đọc dữ liệu ...
    cJSON_Delete(json); // BẮT BUỘC: Giải phóng toàn bộ cây JSON
}
```

### 3.3. Tránh Race Condition (Điều kiện đua)

Sử dụng Mutex để bảo vệ vùng nhớ chung.

```cpp
// GHI dữ liệu (Background Task)
{
    std::lock_guard<std::mutex> lock(mutex_);
    weather_info_ = new_data;
} // Tự động unlock khi hết block

// ĐỌC dữ liệu (UI Task)
WeatherInfo GetWeatherInfo() {
    std::lock_guard<std::mutex> lock(mutex_);
    return weather_info_; // Trả về bản sao (Copy), an toàn tuyệt đối
}
```

### 3.4. Quản lý đối tượng LVGL

Khi hủy class `WeatherUI`, cần xóa các đối tượng đồ họa đã tạo để trả lại RAM cho hệ thống hiển thị.

```cpp
WeatherUI::~WeatherUI() {
    if (idle_panel_ && lv_obj_is_valid(idle_panel_)) {
        lv_obj_del(idle_panel_); // Xóa container chính, các con sẽ tự động bị xóa
        idle_panel_ = nullptr;
    }
}
```

## 4. Cấu trúc Thư mục & File

- `main/features/weather/`
  - `weather_config.h`: Chứa API Key, URL, cấu hình timeout.
  - `weather_model.h`: Định nghĩa struct `WeatherInfo` và `IdleCardInfo`.
  - `weather_service.h/cc`: Logic tải và xử lý dữ liệu.
  - `weather_ui.h/cc`: Logic hiển thị giao diện LVGL.
  - `lunar_calendar.h/cc`: Thuật toán chuyển đổi Dương lịch -> Âm lịch.

## 5. Cách Bật/Tắt Tính năng

Sử dụng `idf.py menuconfig`:

1.  Vào menu **Standby Screen Configuration**.
2.  Chọn **Enable Standby Screen**.
3.  Nhập **OpenWeatherMap API Key** và **Default City**.
