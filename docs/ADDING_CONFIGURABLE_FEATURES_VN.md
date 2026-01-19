# Hướng dẫn: Thêm tính năng Configurable trong Menuconfig (ESP-IDF)

Tài liệu này hướng dẫn chi tiết cách tạo một tùy chọn cấu hình (Configuration Option) để bật/tắt tính năng thông qua `idf.py menuconfig`, và cách viết code sao cho **tất cả các board** đều sử dụng được mà không cần sửa từng file board riêng lẻ.

## 1. Tổng quan luồng dữ liệu (The Flow)

Trước khi bắt đầu, hãy hiểu cách hệ thống hoạt động:

1.  **Kconfig File (`main/Kconfig.projbuild`)**: Bạn định nghĩa menu và biến cấu hình tại đây.
2.  **idf.py menuconfig**: Người dùng chạy lệnh này, giao diện hiện ra, họ chọn Enable/Disable.
3.  **Build System**: Khi biên dịch, ESP-IDF sẽ đọc lựa chọn của người dùng và tạo ra file header `sdkconfig.h`.
4.  **C++ Code**: Code của bạn `#include "sdkconfig.h"` (thường được include tự động) và kiểm tra các macro `CONFIG_...` để quyết định biên dịch đoạn code nào.

**Sơ đồ:**
`Kconfig.projbuild` -> `idf.py menuconfig` -> `sdkconfig` (file lưu) -> `sdkconfig.h` (Auto-gen) -> `Code C++`

---

## 2. Bước 1: Định nghĩa trong Kconfig

File cần chỉnh sửa: `main/Kconfig.projbuild`
Đây là nơi chứa các cấu hình riêng cho dự án Xiaozhi.

**Ví dụ:** Chúng ta sẽ làm tính năng **"Cảnh báo pin yếu bằng giọng nói"** (Voice Low Battery Warning).

Mở `main/Kconfig.projbuild` và thêm đoạn sau (nên thêm vào cuối file hoặc trong một `menu` phù hợp):

```kconfig
menu "Battery Features"  # Tạo một menu con tên là Battery Features

    config ENABLE_LOW_BATTERY_VOICE
        bool "Enable Low Battery Voice Warning"  # Tên hiển thị trong menu
        default y                                # Mặc định là Bật (y=yes, n=no)
        help
            If enabled, the device will speak a warning when battery is low.
            # Dòng giải thích chi tiết tính năng làm gì

    config LOW_BATTERY_THRESHOLD
        int "Low Battery Threshold (%)"          # Cho phép nhập số
        range 5 50                               # Giới hạn từ 5% đến 50%
        default 20                               # Mặc định 20%
        depends on ENABLE_LOW_BATTERY_VOICE      # Chỉ hiện khi option trên được BẬT
        help
            The percentage at which the warning triggers.

endmenu
```

**Giải thích:**

- `config ENABLE_LOW_BATTERY_VOICE`: Đây là tên biến. Trong code C++, nó sẽ tự động thêm tiền tố `CONFIG_` -> `CONFIG_ENABLE_LOW_BATTERY_VOICE`.
- `bool`: Kiểu dữ liệu đúng/sai.
- `depends on`: Rất hay, giúp ẩn hiện menu con dựa trên menu cha.

---

## 3. Bước 2: Triển khai Code (Architecture)

Để code chạy được trên **mọi board**, nguyên tắc vàng là: **Không viết logic vào file board cụ thể (như `wifi_board.cc`)**.

Hãy viết logic vào lớp `Application` (lớp quản lý chung) hoặc một lớp quản lý riêng (Manager). Các lớp này sẽ gọi xuống `Board` thông qua **Interface** (lớp cha `Board`).

### Ví dụ triển khai trong `main/application.cc`

Chúng ta sẽ sửa file `main/application.cc`.

**Bước 2.1: Kiểm tra Config lúc biên dịch (Compile-time check)**

Sử dụng `#ifdef` để trình biên dịch biết có nên đưa đoạn code này vào firmware hay không. Điều này giúp tiết kiệm Flash nếu tính năng bị tắt.

```cpp
// main/application.cc

// ... các include khác ...
#include "board.h"
#include "sdkconfig.h" // Đảm bảo đã include file này

void Application::CheckBatteryStatus() {
    // Lấy instance của Board (Singleton Pattern)
    // Đây là cách code chạy được trên mọi board: Nó gọi hàm ảo GetBatteryLevel
    auto& board = Board::GetInstance();

    int level;
    bool charging, discharging;

    // Gọi hàm chung của Board
    if (board.GetBatteryLevel(level, charging, discharging)) {

        // --- BẮT ĐẦU ĐOẠN CODE CẤU HÌNH ---

#ifdef CONFIG_ENABLE_LOW_BATTERY_VOICE
        // Đoạn code này chỉ được biên dịch nếu bạn chọn "Y" trong menuconfig

        // Lấy ngưỡng từ menuconfig (CONFIG_LOW_BATTERY_THRESHOLD)
        if (discharging && level <= CONFIG_LOW_BATTERY_THRESHOLD) {
            // Giả sử hàm PlaySound có tồn tại
            PlaySound(Lang::Sounds::OGG_LOW_BATTERY);
            ESP_LOGW(TAG, "Low battery warning triggered at %d%%", level);
        }
#endif

        // --- KẾT THÚC ĐOẠN CODE CẤU HÌNH ---
    }
}
```

**Giải thích:**

- `Board::GetInstance()`: Trả về đối tượng board hiện tại (dù là board S3 Box, board Wifi, hay board Custom).
- `#ifdef CONFIG_...`: Nếu người dùng tắt tính năng này, trình biên dịch sẽ vứt bỏ đoạn code bên trong, giúp firmware nhẹ hơn.
- `CONFIG_LOW_BATTERY_THRESHOLD`: Giá trị `int` bạn đã đặt trong Kconfig được thay thế trực tiếp vào đây.

---

## 4. Bước 3: Xử lý Runtime (Nếu cần)

Đôi khi bạn muốn biên dịch code vào nhưng chỉ tắt nó đi dựa trên logic chạy. Nhưng với yêu cầu "Enable trong menuconfig", cách dùng `#ifdef` ở trên là chuẩn nhất.

Tuy nhiên, nếu bạn muốn code sạch hơn (tránh quá nhiều `#ifdef` lồng nhau), bạn có thể dùng `IS_ENABLED` (nếu ESP-IDF hỗ trợ macro này) hoặc viết như sau:

```cpp
// Một cách viết khác hiện đại hơn (C++17 if constexpr - nếu trình biên dịch hỗ trợ)
// Hoặc đơn giản là if thường (trình biên dịch thông minh sẽ tự tối ưu code chết)

void Application::SomeFunction() {
#if CONFIG_ENABLE_LOW_BATTERY_VOICE
    bool voice_enabled = true;
#else
    bool voice_enabled = false;
#endif

    if (voice_enabled) {
        // Logic here
    }
}
```

---

## 5. Tóm tắt các file cần chỉnh sửa

Để thực hiện một tính năng trọn vẹn:

1.  **`main/Kconfig.projbuild`**:

    - _Nhiệm vụ:_ Tạo giao diện bật tắt cho người dùng.
    - _Kết quả:_ Tạo ra các macro `CONFIG_TEN_TINH_NANG`.

2.  **`main/application.h`** (Nếu cần thêm biến/hàm mới):

    - _Nhiệm vụ:_ Khai báo hàm mới.
    - _Lưu ý:_ Cũng nên bọc `#ifdef` quanh khai báo hàm nếu hàm đó chỉ tồn tại khi bật tính năng.

3.  **`main/application.cc`** (Hoặc file logic chính):

    - _Nhiệm vụ:_ Thực thi logic.
    - _Cách gọi:_ Sử dụng `Board::GetInstance()` để tương tác phần cứng, dùng `#ifdef` để kiểm tra cấu hình.

4.  **`main/boards/common/board.h`** (Nếu tính năng cần phần cứng mới):
    - _Nhiệm vụ:_ Định nghĩa Interface chung (ví dụ: `virtual void TurnOnFan() = 0;`).

---

## 6. Ví dụ thực tế: Flow gọi nhau

Giả sử bạn bật tính năng trên. Flow sẽ chạy như sau:

1.  **Khởi động:** `main.cc` gọi `Application::GetInstance().Start()`.
2.  **Vòng lặp:** `Application` có một Timer hoặc Loop kiểm tra trạng thái.
3.  **Kiểm tra:** Trong hàm kiểm tra, nó gặp `#ifdef CONFIG_ENABLE_LOW_BATTERY_VOICE`.
4.  **Tiền xử lý:**
    - Nếu trong menuconfig chọn **Yes**: Trình biên dịch thấy đoạn code bên trong. Nó gọi `Board::GetInstance().GetBatteryLevel()`.
    - Nếu trong menuconfig chọn **No**: Trình biên dịch xóa sạch đoạn code đó. Firmware không hề biết tính năng này tồn tại.
5.  **Đa hình (Polymorphism):** Khi gọi `Board::GetInstance()`, nếu bạn đang nạp code cho board `bread-compact-wifi`, nó sẽ trả về instance của class `CompactWifiBoard`. Nếu nạp cho `esp-box`, nó trả về `EspBoxBoard`. Code logic trong `Application` không thay đổi.

Đây là cách kiến trúc **Platform Agnostic** (Độc lập nền tảng) hoạt động.
