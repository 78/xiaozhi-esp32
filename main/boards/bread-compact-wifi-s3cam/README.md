# Bread Compact WiFi S3Cam Board Documentation

## Tổng quan

Board **bread-compact-wifi-s3cam** là một board phát triển dựa trên ESP32-S3, tích hợp WiFi, màn hình LCD SPI, camera OV2640, và audio I2S. Board này được thiết kế cho các ứng dụng AI như trợ lý ảo với khả năng hiển thị, chụp ảnh, và xử lý âm thanh.

**Lưu ý:** Camera sử dụng nhiều GPIO, chiếm dụng USB pins 19 và 20 của ESP32-S3.

## Cấu hình chân (Pin Configuration)

### Màn hình LCD (Display Pins)

- **MOSI**: GPIO_NUM_20
- **CLK (SCLK)**: GPIO_NUM_19
- **DC**: GPIO_NUM_47
- **RST**: GPIO_NUM_21
- **CS**: GPIO_NUM_45
- **Backlight**: GPIO_NUM_38

### Camera Pins (OV2640)

- **D0-D7**: GPIO_NUM_11, 9, 8, 10, 12, 18, 17, 16
- **XCLK**: GPIO_NUM_15
- **PCLK**: GPIO_NUM_13
- **VSYNC**: GPIO_NUM_6
- **HREF**: GPIO_NUM_7
- **SIOC (SCL)**: GPIO_NUM_5
- **SIOD (SDA)**: GPIO_NUM_4
- **PWDN**: GPIO_NUM_NC (Không sử dụng)
- **RESET**: GPIO_NUM_NC (Không sử dụng)
- **XCLK Frequency**: 20MHz

### Audio I2S (Simplex Mode - Default)

- **MIC WS**: GPIO_NUM_1
- **MIC SCK**: GPIO_NUM_2
- **MIC DIN**: GPIO_NUM_42
- **SPK DOUT**: GPIO_NUM_39
- **SPK BCLK**: GPIO_NUM_40
- **SPK LRCK**: GPIO_NUM_41

### Audio I2S (Duplex Mode - Alternative)

- **WS**: GPIO_NUM_4
- **BCLK**: GPIO_NUM_5
- **DIN**: GPIO_NUM_6
- **DOUT**: GPIO_NUM_7

### Các chân khác

- **Builtin LED**: GPIO_NUM_48
- **Boot Button**: GPIO_NUM_0
- **Lamp Control**: GPIO_NUM_14

## Cấu hình màn hình (Display Configuration)

Board hỗ trợ nhiều loại màn hình LCD SPI khác nhau, được cấu hình qua Kconfig:

### ST7789 240x320 (IPS)

- Width: 240
- Height: 320
- Mirror X: false
- Mirror Y: false
- Swap XY: false
- Invert Color: true
- RGB Order: RGB
- Offset X: 0
- Offset Y: 0
- Backlight Invert: false
- SPI Mode: 0

### ST7789 240x320 (Non-IPS)

- Width: 240
- Height: 320
- Invert Color: false

### ST7789 170x320

- Width: 170
- Height: 320
- Offset X: 35
- Offset Y: 0

### ST7789 172x320

- Width: 172
- Height: 320
- Offset X: 34
- Offset Y: 0

### ST7789 240x280

- Width: 240
- Height: 280
- Offset X: 0
- Offset Y: 20

### ST7789 240x240

- Width: 240
- Height: 240
- Offset X: 0
- Offset Y: 0

### ST7789 240x135

- Width: 240
- Height: 135
- Mirror X: true
- Mirror Y: false
- Swap XY: true
- Offset X: 40
- Offset Y: 53

### ST7735 128x160

- Width: 128
- Height: 160
- Mirror X: true
- Mirror Y: true
- Swap XY: false
- Invert Color: false

### ST7735 128x128

- Width: 128
- Height: 128
- Mirror X: true
- Mirror Y: true
- Swap XY: false
- Invert Color: false
- RGB Order: BGR
- Offset X: 0
- Offset Y: 32

### ST7796 320x480

- Width: 320
- Height: 480
- Mirror X: true
- Mirror Y: false
- Swap XY: false
- Invert Color: true
- RGB Order: BGR

### ILI9341 240x320

- Width: 240
- Height: 320
- Mirror X: true
- Mirror Y: false
- Swap XY: false
- Invert Color: true
- RGB Order: BGR

### GC9A01 240x240

- Width: 240
- Height: 240
- Mirror X: true
- Mirror Y: false
- Swap XY: false
- Invert Color: true
- RGB Order: BGR

## Workflow khởi tạo (Initialization Workflow)

### 1. Khởi tạo SPI Bus

- Cấu hình SPI bus với MOSI (GPIO 20), SCLK (GPIO 19)
- Sử dụng SPI3_HOST
- Max transfer size: DISPLAY_WIDTH _ DISPLAY_HEIGHT _ 2 bytes

### 2. Khởi tạo LCD Display

- Tạo panel IO với SPI config
- CS (GPIO 45), DC (GPIO 47), SPI mode, PCLK 40MHz
- Khởi tạo panel driver (ST7789, ILI9341, GC9A01)
- Reset panel
- Cấu hình panel: invert color, swap XY, mirror
- Tạo SpiLcdDisplay object

### 3. Khởi tạo Camera

- Cấu hình DVP pins cho camera OV2640
- SCCB I2C config (SCL GPIO 5, SDA GPIO 4)
- Video init config với DVP
- Tạo Esp32Camera object
- Set H-Mirror: false

### 4. Khởi tạo Buttons

- Boot button trên GPIO 0
- OnClick: Toggle chat state hoặc enter WiFi config mode

### 5. Khởi tạo Backlight (nếu có)

- PWM backlight trên GPIO 38
- Restore brightness

## Các Function chính (Main Functions)

### GetAudioCodec()

- Trả về NoAudioCodec (Simplex hoặc Duplex)
- Sample rates: Input 16kHz, Output 24kHz

### GetDisplay()

- Trả về SpiLcdDisplay pointer

### GetBacklight()

- Trả về PwmBacklight pointer (nếu có)

### GetCamera()

- Trả về Esp32Camera pointer

### GetLed()

- Trả về SingleLed trên GPIO 48

## Hướng dẫn biên dịch và cấu hình

**Cấu hình target ESP32-S3:**

```bash
idf.py set-target esp32s3
```

**Mở menuconfig:**

```bash
idf.py menuconfig
```

**Chọn board:**

- Xiaozhi Assistant -> Board Type -> Bread Compact WiFi + LCD + Camera

**Cấu hình camera sensor:**

- Component config -> Espressif Camera Sensors Configurations -> Camera Sensor Configuration -> Select and Set Camera Sensor
- Chọn OV2640
- Enable Auto detect
- Set default output format: YUV422

**Biên dịch và flash:**

```bash
idf.py build flash
```

## Lưu ý

- Board sử dụng ESP32-S3 target
- Hỗ trợ partition table V2
- Font mặc định: puhui_basic_16_4, font_awesome_16_4
- Emoji collection: twemoji_64
- Camera không có H-Mirror
- Camera chiếm dụng USB pins (19, 20)
