# ESP-Puppy-S3 (Robot Chó ESP32-S3)

Đây là phiên bản sửa đổi của ESP-Hi dành cho chip ESP32-S3, hỗ trợ robot chó với 5 servo (4 chân + 1 đuôi).

## Thay đổi chính so với ESP-Hi (ESP32-C3)

1.  **Chipset**: Chuyển từ ESP32-C3 sang ESP32-S3 để có hiệu năng cao hơn và nhiều chân GPIO hơn.
2.  **Servo**: Thêm hỗ trợ cho servo thứ 5 (đuôi) sử dụng điều khiển PWM (LEDC).
3.  **Cấu hình**: Cập nhật `sdkconfig` và sơ đồ chân (Pinout) cho phù hợp với ESP32-S3 DevKit.

## Sơ đồ đấu nối (Wiring Diagram)

### 1. Servo (5 cái)
Sử dụng nguồn 5V riêng cho Servo nếu có thể, nối chung GND với ESP32-S3.
**Lưu ý: Chân Servo đã được thay đổi để nhường chỗ cho Audio I2S.**

| Servo | Chức năng | Chân ESP32-S3 (GPIO) | Ghi chú |
|-------|-----------|----------------------|---------|
| FL    | Chân trước trái | **GPIO 17** | PWM |
| FR    | Chân trước phải | **GPIO 18** | PWM |
| BL    | Chân sau trái | **GPIO 19** | PWM |
| BR    | Chân sau phải | **GPIO 20** | PWM |
| **Tail** | **Đuôi** | **GPIO 21** | **PWM** |

### 2. Màn hình (1.54 inch ST7789 SPI)
Màn hình màu 1.54 inch, độ phân giải 240x240 (giống board zhengchen-1.54tft-wifi).

| Chân Màn hình | Chân ESP32-S3 | Ghi chú |
|---------------|---------------|---------|
| SDA (MOSI)    | GPIO 11       | SPI Data |
| SCL (CLK)     | GPIO 12       | SPI Clock |
| DC            | GPIO 13       | Data/Command |
| RES (RST)     | GPIO 14       | Reset |
| CS            | GPIO 10       | Chip Select |
| BLK           | NC / 3.3V     | Đèn nền (hoặc GPIO tùy chọn) |
| VCC           | 3.3V          | Nguồn |
| GND           | GND           | Mass |

### 3. Âm thanh (Audio I2S)
Sử dụng cấu hình I2S giống board `zhengchen-1.54tft-wifi`.

| Thiết bị | Chân (Pin) | Chân ESP32-S3 | Ghi chú |
|----------|------------|---------------|---------|
| **Microphone** | WS (LRCK) | **GPIO 4** | I2S Mic |
| | SCK (BCLK) | **GPIO 5** | I2S Mic |
| | SD (DIN) | **GPIO 6** | I2S Mic |
| **Loa (Speaker)** | DOUT (DIN) | **GPIO 7** | I2S Amp |
| | BCLK | **GPIO 15** | I2S Amp |
| | LRCK | **GPIO 16** | I2S Amp |

### 4. Nút bấm & LED

| Chức năng | Chân ESP32-S3 | Ghi chú |
|-----------|---------------|---------|
| Boot Button | GPIO 0 | Nút Boot có sẵn trên DevKit |
| Wake Button | GPIO 1 | Nút đánh thức chuyển động |
| Audio Button| GPIO 2 | Nút đánh thức âm thanh |
| LED RGB     | GPIO 48 | LED WS2812 trên DevKit S3 |

## Cách biên dịch (Build)

Sử dụng lệnh sau để biên dịch cho board mới:

```bash
idf.py -p PORT flash monitor
```

Hoặc sử dụng script của dự án (nếu có):

```bash
python ./scripts/release.py esp-puppy-s3
```

## Điều khiển Đuôi (Tail Control)

Đã thêm công cụ (tool) mới cho MCP Server để điều khiển đuôi:
- `self.dog.tail_control`: Đặt góc quay cho đuôi (0-180 độ).

Ví dụ:
- Vẫy đuôi: Gọi liên tục `tail_control` với các góc thay đổi (ví dụ: 45 -> 135 -> 45).
