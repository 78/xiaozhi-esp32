# S3CAM Voice Assistant with Telegram & Relay Module

Custom ESP32-S3 CAM board featuring:
- **XiaoZhi Voice Assistant**: Simplex I2S microphone (INMP441) and audio amplifier (MAX98357A).
- **Display**: 0.96" SSD1306 OLED Display (I2C Port 0).
- **Camera**: OV2640 DVP Camera (I2C Port 1 SCCB).
- **Relay Switch**: 1-Channel Active-LOW 5V Relay module on GPIO 14.
- **Telegram Integration**: Remote control `/status`, `/relay`, `/photo`, and live voice chat transcript forwarding to Telegram group/chat.

## Pinout Mapping

| Component | Pin / GPIO | Notes |
| :--- | :--- | :--- |
| **INMP441 Mic** | SCK: GPIO 2, WS: GPIO 1, SD: GPIO 42 | L/R tied to GND |
| **MAX98357A Amp**| BCLK: GPIO 40, LRCK: GPIO 41, DOUT: GPIO 39 | SD to 3.3V |
| **OLED SSD1306**| SDA: GPIO 45, SCL: GPIO 47 | I2C Port 0 |
| **Relay Switch**| IN: GPIO 14 | Active-LOW (0 = ON, 1 = OFF) |
| **OV2640 Camera**| D0-D7: 12,11,9,8,10,18,17,16, XCLK: 15, PCLK: 13, VSYNC: 6, HREF: 7, SIOD: 4, SIOC: 5 | I2C Port 1 |
| **BOOT Button** | GPIO 0 | Pullup, GND on press |
| **Builtin LED** | GPIO 48 | Active-HIGH |

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
