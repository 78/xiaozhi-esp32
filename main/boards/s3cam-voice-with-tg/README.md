# S3CAM Voice Assistant with Telegram & Relay Module

Custom ESP32-S3 CAM board featuring:
- **XiaoZhi Voice Assistant**: Simplex I2S microphone (INMP441) and audio amplifier (MAX98357A).
- **Display**: 1.8" ST7735S TFT Display (SPI 128x160).
- **Camera**: OV2640 DVP Camera (I2C Port 1 SCCB).
- **Relay Switch**: 1-Channel Active-LOW 5V Relay module on GPIO 14.
- **Telegram Integration**: Remote control `/status`, `/relay`, `/photo`, and live voice chat transcript forwarding to Telegram group/chat.

## Pinout Mapping

| Component | Pin / GPIO | Notes |
| :--- | :--- | :--- |
| **INMP441 Mic** | SCK: GPIO 2, WS: GPIO 1, SD: GPIO 42, VDD: 3.3V, L/R: GND, GND: GND | L/R tied to GND |
| **MAX98357A Amp**| BCLK: GPIO 40, LRCK: GPIO 41, DOUT: GPIO 39, GAIN: GND, SD: 3.3V, Vin: 5V, GND: GND | SD to 3.3V (enabled) |
| **ST7735S TFT** | SCK: GPIO 19, SDA: GPIO 20, A0: GPIO 47, RESET: GPIO 21, CS: GPIO 45, LED: GPIO 38, VCC: 3.3V, GND: GND | SPI 128x160 |
| **Relay Switch**| IN: GPIO 14 | Active-LOW (0 = ON, 1 = OFF) |
| **OV2640 Camera**| D0: GPIO 12, D1: GPIO 11, D2: GPIO 9, D3: GPIO 8, D4: GPIO 10, D5: GPIO 18, D6: GPIO 17, D7: GPIO 16, XCLK: GPIO 15, PCLK: GPIO 13, VSYNC: GPIO 6, HREF: GPIO 7, SIOD: GPIO 4, SIOC: GPIO 5 | I2C Port 1 (SCCB) |
| **BOOT Button** | GPIO 0 | Pullup, GND on press |
| **Builtin LED** | GPIO 48 | Active-HIGH |

## Power Distribution

| Rail | Components |
| :--- | :--- |
| **5V (USB)** | MAX98357A Vin, Relay Module VCC |
| **3.3V** | INMP441 VDD, MAX98357A SD, ST7735S VCC, OV2640 (internal) |
| **GND** | Common ground for all modules |

## NVS Configuration for Telegram

Set your Telegram Bot API token and Chat/Group ID via `idf.py nvs-set`:

```bash
idf.py nvs-set telegram token -s "YOUR_BOT_TOKEN"
idf.py nvs-set telegram chat_id -s "YOUR_CHAT_OR_GROUP_ID"
```

## Build Instructions

```bash
python scripts/build.py s3cam-voice-with-tg --name s3cam-voice-with-tg
```