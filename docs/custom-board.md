# Hướng dẫn Bo mạch tùy chỉnh

（Tiếng Việt | [中文](custom-board_zh.md)）

Hướng dẫn này giới thiệu cách tùy chỉnh chương trình khởi tạo bo mạch phát triển mới cho dự án chatbot giọng nói AI Xiaozhi. Xiaozhi AI hỗ trợ hơn 70 loại bo mạch phát triển dòng ESP32, mã khởi tạo của mỗi bo mạch được đặt trong thư mục tương ứng.

## Lưu ý quan trọng

> **Cảnh báo**: Đối với bo mạch tùy chỉnh, khi cấu hình IO khác với bo mạch hiện có, tuyệt đối không được ghi đè trực tiếp cấu hình bo mạch hiện có để biên dịch firmware. Phải tạo loại bo mạch mới, hoặc thông qua cấu hình builds trong file config.json với name và macro sdkconfig khác nhau để phân biệt. Sử dụng `python scripts/release.py [tên thư mục bo mạch]` để biên dịch và đóng gói firmware.
>
> Nếu ghi đè trực tiếp cấu hình hiện có, khi nâng cấp OTA trong tương lai, firmware tùy chỉnh của bạn có thể bị ghi đè bởi firmware tiêu chuẩn của bo mạch gốc, khiến thiết bị của bạn không hoạt động bình thường. Mỗi bo mạch có định danh duy nhất và kênh nâng cấp firmware tương ứng, việc duy trì tính duy nhất của định danh bo mạch rất quan trọng.

## Cấu trúc thư mục

Cấu trúc thư mục của mỗi bo mạch phát triển thường bao gồm các file sau:

- `xxx_board.cc` - Mã khởi tạo cấp bo mạch chính, triển khai các chức năng khởi tạo và tính năng liên quan đến bo mạch
- `config.h` - File cấu hình cấp bo mạch, định nghĩa ánh xạ chân phần cứng và các mục cấu hình khác
- `config.json` - Cấu hình biên dịch, chỉ định chip đích và các tùy chọn biên dịch đặc biệt
- `README.md` - Tài liệu hướng dẫn liên quan đến bo mạch phát triển

## Các bước tùy chỉnh bo mạch phát triển

### 1. Tạo thư mục bo mạch phát triển mới

Đầu tiên tạo một thư mục mới trong thư mục `boards/`, cách đặt tên nên sử dụng dạng `[tên-thương-hiệu]-[loại-bo-mạch]`, ví dụ `m5stack-tab5`:

```bash
mkdir main/boards/my-custom-board
```

### 2. Tạo các file cấu hình

#### config.h

Trong `config.h` định nghĩa tất cả cấu hình phần cứng, bao gồm:

- Tần số lấy mẫu âm thanh và cấu hình chân I2S
- Địa chỉ chip codec âm thanh và cấu hình chân I2C
- Cấu hình chân nút bấm và LED
- Tham số màn hình hiển thị và cấu hình chân

Ví dụ tham khảo (từ lichuang-c3-dev):

```c
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Cấu hình âm thanh
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_10
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_12
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_8
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_7
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_11

#define AUDIO_CODEC_PA_PIN       GPIO_NUM_13
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_0
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_1
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

// Cấu hình nút bấm
#define BOOT_BUTTON_GPIO        GPIO_NUM_9

// Cấu hình màn hình hiển thị
#define DISPLAY_SPI_SCK_PIN     GPIO_NUM_3
#define DISPLAY_SPI_MOSI_PIN    GPIO_NUM_5
#define DISPLAY_DC_PIN          GPIO_NUM_6
#define DISPLAY_SPI_CS_PIN      GPIO_NUM_4

#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY true

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_2
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

#endif // _BOARD_CONFIG_H_
```

#### config.json

Trong `config.json` định nghĩa cấu hình biên dịch, file này được sử dụng cho script `scripts/release.py` để biên dịch tự động:

```json
{
    "target": "esp32s3",  // Mẫu chip đích: esp32, esp32s3, esp32c3, esp32c6, esp32p4, v.v.
    "builds": [
        {
            "name": "my-custom-board",  // Tên bo mạch phát triển, dùng để tạo gói firmware
            "sdkconfig_append": [
                // Cấu hình kích thước Flash đặc biệt
                "CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y",
                // Cấu hình bảng phân vùng đặc biệt
                "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\""
            ]
        }
    ]
}
```

**Giải thích các mục cấu hình:**
- `target`: Mẫu chip đích, phải khớp với phần cứng
- `name`: Tên gói firmware đầu ra biên dịch, khuyến nghị phù hợp với tên thư mục
- `sdkconfig_append`: Mảng các mục cấu hình sdkconfig bổ sung, sẽ được thêm vào cấu hình mặc định

**Cấu hình sdkconfig_append thường dùng:**
```json
// Kích thước Flash
"CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y"   // Flash 4MB
"CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y"   // Flash 8MB
"CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y"  // Flash 16MB

// Bảng phân vùng
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/4m.csv\""  // Bảng phân vùng 4MB
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/8m.csv\""  // Bảng phân vùng 8MB
"CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions/v2/16m.csv\"" // Bảng phân vùng 16MB

// Cấu hình ngôn ngữ
"CONFIG_LANGUAGE_EN_US=y"  // Tiếng Anh
"CONFIG_LANGUAGE_ZH_CN=y"  // Tiếng Trung giản thể

// Cấu hình từ đánh thức
"CONFIG_USE_DEVICE_AEC=y"          // Bật AEC phía thiết bị
"CONFIG_WAKE_WORD_DISABLED=y"      // Tắt từ đánh thức
```

### 3. Viết mã khởi tạo cấp bo mạch

Tạo file `my_custom_board.cc`, triển khai tất cả logic khởi tạo của bo mạch phát triển.

Định nghĩa lớp bo mạch phát triển cơ bản bao gồm các phần sau:

1. **Định nghĩa lớp**: Kế thừa từ `WifiBoard` hoặc `Ml307Board`
2. **Hàm khởi tạo**: Bao gồm khởi tạo các thành phần I2C, màn hình hiển thị, nút bấm, IoT, v.v.
3. **Ghi đè hàm ảo**: Như `GetAudioCodec()`, `GetDisplay()`, `GetBacklight()`, v.v.
4. **Đăng ký bo mạch**: Sử dụng macro `DECLARE_BOARD` để đăng ký bo mạch

```cpp
#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>

#define TAG "MyCustomBoard"

class MyCustomBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    LcdDisplay* display_;

    // Khởi tạo I2C
    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    // Khởi tạo SPI (dùng cho màn hình hiển thị)
    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // Khởi tạo nút bấm
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    // Khởi tạo màn hình hiển thị (ví dụ ST7789)
    void InitializeDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 2;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        
        // Tạo đối tượng màn hình hiển thị
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // Khởi tạo MCP Tools
    void InitializeTools() {
        // Tham khảo tài liệu MCP
    }

public:
    // Hàm khởi tạo
    MyCustomBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeDisplay();
        InitializeButtons();
        InitializeTools();
        GetBacklight()->SetBrightness(100);
    }

    // Lấy codec âm thanh
    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_, 
            I2C_NUM_0, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    // Lấy màn hình hiển thị
    virtual Display* GetDisplay() override {
        return display_;
    }
    
    // Lấy điều khiển đèn nền
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

// Đăng ký bo mạch phát triển
DECLARE_BOARD(MyCustomBoard);
```

### 4. Thêm cấu hình hệ thống xây dựng

#### Thêm tùy chọn bo mạch phát triển trong Kconfig.projbuild

Mở file `main/Kconfig.projbuild`, thêm mục cấu hình bo mạch phát triển mới trong phần `choice BOARD_TYPE`:

```kconfig
choice BOARD_TYPE
    prompt "Board Type"
    default BOARD_TYPE_BREAD_COMPACT_WIFI
    help
        Board type. Loại bo mạch phát triển
    
    # ... Các tùy chọn bo mạch phát triển khác ...
    
    config BOARD_TYPE_MY_CUSTOM_BOARD
        bool "My Custom Board (Bo mạch tùy chỉnh của tôi)"
        depends on IDF_TARGET_ESP32S3  # Sửa theo chip đích của bạn
endchoice
```

**Lưu ý:**
- `BOARD_TYPE_MY_CUSTOM_BOARD` là tên mục cấu hình, cần viết hoa toàn bộ, sử dụng dấu gạch dưới phân tách
- `depends on` chỉ định loại chip đích (như `IDF_TARGET_ESP32S3`, `IDF_TARGET_ESP32C3`, v.v.)
- Văn bản mô tả có thể sử dụng tiếng Việt và tiếng Anh

#### Thêm cấu hình bo mạch phát triển trong CMakeLists.txt

Mở file `main/CMakeLists.txt`, thêm cấu hình mới trong phần phán đoán loại bo mạch phát triển:

```cmake
# Thêm cấu hình bo mạch phát triển của bạn trong chuỗi elseif
elseif(CONFIG_BOARD_TYPE_MY_CUSTOM_BOARD)
    set(BOARD_TYPE "my-custom-board")  # Phù hợp với tên thư mục
    set(BUILTIN_TEXT_FONT font_puhui_basic_20_4)  # Chọn font phù hợp theo kích thước màn hình
    set(BUILTIN_ICON_FONT font_awesome_20_4)
    set(DEFAULT_EMOJI_COLLECTION twemoji_64)  # Tùy chọn, nếu cần hiển thị biểu cảm
endif()
```

**Giải thích cấu hình font và biểu cảm:**

Chọn kích thước font phù hợp theo độ phân giải màn hình:
- Màn hình nhỏ (128x64 OLED): `font_puhui_basic_14_1` / `font_awesome_14_1`
- Màn hình trung bình nhỏ (240x240): `font_puhui_basic_16_4` / `font_awesome_16_4`
- Màn hình trung bình (240x320): `font_puhui_basic_20_4` / `font_awesome_20_4`
- Màn hình lớn (480x320+): `font_puhui_basic_30_4` / `font_awesome_30_4`

Tùy chọn bộ sưu tập biểu cảm:
- `twemoji_32` - Biểu cảm 32x32 pixel (màn hình nhỏ)
- `twemoji_64` - Biểu cảm 64x64 pixel (màn hình lớn)

### 5. Cấu hình và biên dịch

#### Phương pháp 1: Sử dụng idf.py cấu hình thủ công

1. **Đặt chip đích** (khi cấu hình lần đầu hoặc thay đổi chip):
   ```bash
   # Đối với ESP32-S3
   idf.py set-target esp32s3
   
   # Đối với ESP32-C3
   idf.py set-target esp32c3
   
   # Đối với ESP32
   idf.py set-target esp32
   ```

2. **Dọn dẹp cấu hình cũ**:
   ```bash
   idf.py fullclean
   ```

3. **Vào menu cấu hình**:
   ```bash
   idf.py menuconfig
   ```
   
   Trong menu điều hướng đến: `Xiaozhi Assistant` -> `Board Type`, chọn bo mạch phát triển tùy chỉnh của bạn.

4. **Biên dịch và nạp**:
   ```bash
   idf.py build
   idf.py flash monitor
   ```

#### Phương pháp 2: Sử dụng script release.py (khuyên dùng)

Nếu thư mục bo mạch phát triển của bạn có file `config.json`, có thể sử dụng script này để tự động hoàn thành cấu hình và biên dịch:

```bash
python scripts/release.py my-custom-board
```

Script này sẽ tự động:
- Đọc cấu hình `target` trong `config.json` và đặt chip đích
- Áp dụng các tùy chọn biên dịch trong `sdkconfig_append`
- Hoàn thành biên dịch và đóng gói firmware

### 6. Tạo README.md

Trong README.md giải thích các tính năng của bo mạch phát triển, yêu cầu phần cứng, các bước biên dịch và nạp:

## Các thành phần bo mạch phát triển thường gặp

### 1. Màn hình hiển thị

Dự án hỗ trợ nhiều driver màn hình hiển thị, bao gồm:
- ST7789 (SPI)
- ILI9341 (SPI)
- SH8601 (QSPI)
- v.v.

### 2. Codec âm thanh

Các codec được hỗ trợ bao gồm:
- ES8311 (thường dùng)
- ES7210 (mảng microphone)
- AW88298 (bộ khuếch đại)
- v.v.

### 3. Quản lý nguồn

Một số bo mạch phát triển sử dụng chip quản lý nguồn:
- AXP2101
- Các PMIC khả dụng khác

### 4. Điều khiển thiết bị MCP

Có thể thêm các công cụ MCP khác nhau để AI có thể sử dụng:
- Speaker (điều khiển loa)
- Screen (điều chỉnh độ sáng màn hình)
- Battery (đọc mức pin)
- Light (điều khiển đèn LED)
- v.v.

## Mối quan hệ kế thừa lớp bo mạch phát triển

- `Board` - Lớp cơ sở cấp bo mạch
  - `WifiBoard` - Bo mạch phát triển kết nối Wi-Fi
  - `Ml307Board` - Bo mạch phát triển sử dụng module 4G
  - `DualNetworkBoard` - Bo mạch phát triển hỗ trợ chuyển đổi giữa mạng Wi-Fi và 4G

## Kỹ thuật phát triển

1. **Tham khảo bo mạch tương tự**: Nếu bo mạch mới của bạn có điểm tương đồng với bo mạch hiện có, có thể tham khảo triển khai hiện tại
2. **Debug từng bước**: Triển khai chức năng cơ bản trước (như hiển thị), sau đó thêm các chức năng phức tạp hơn (như âm thanh)
3. **Ánh xạ chân**: Đảm bảo cấu hình chính xác tất cả ánh xạ chân trong config.h
4. **Kiểm tra tương thích phần cứng**: Xác nhận tương thích của tất cả chip và driver

## Vấn đề có thể gặp phải

1. **Màn hình hiển thị không bình thường**: Kiểm tra cấu hình SPI, cài đặt mirror và cài đặt đảo màu
2. **Không có âm thanh đầu ra**: Kiểm tra cấu hình I2S, chân enable PA và địa chỉ codec
3. **Không thể kết nối mạng**: Kiểm tra thông tin xác thực Wi-Fi và cấu hình mạng
4. **Không thể giao tiếp với server**: Kiểm tra cấu hình MQTT hoặc WebSocket

## Tài liệu tham khảo

- Tài liệu ESP-IDF: https://docs.espressif.com/projects/esp-idf/
- Tài liệu LVGL: https://docs.lvgl.io/
- Tài liệu ESP-SR: https://github.com/espressif/esp-sr