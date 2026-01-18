# Kincony ESP32-S3 AS

This board is designed for Kincony ESP32-S3 AS with MAX98357A amplifier and INMP441 microphone.

## Features

- Rainbow LED animation on startup (3 seconds)
- WS2812B RGBW LED control
- MAX98357A I2S audio amplifier
- INMP441 I2S microphone

## Pin Configuration

### Audio
- MAX98357A:
  - LRC: GPIO6
  - BCLK: GPIO7
  - DIN: GPIO8
  - SD MODE: GPIO5

- INMP441:
  - WS: GPIO3
  - SCK: GPIO2
  - SD: GPIO4

### LEDs
- WS2812B Bottom (3pcs): GPIO15
- WS2812B Vertical Bar: GPIO16

### SD Card (Reserved)
- MISO: GPIO13
- SCK: GPIO12
- MOSI: GPIO11
- CS: GPIO10

### Buttons
- **BOOT Button** (GPIO0): 
  - Short press: Enter WiFi config mode (during startup) or toggle chat state
  - Long press: Reset WiFi credentials and reboot

## Troubleshooting

If WiFi doesn't work:
1. Long press BOOT button to reset WiFi credentials
2. Device will reboot and enter WiFi config mode
3. Connect to the hotspot and configure WiFi via web interface

Use the release script:
```bash
python scripts/release.py kincony-esp32-s3-as
```

Or manually:
```bash
idf.py set-target esp32s3
idf.py menuconfig  # Select "Kincony ESP32-S3 AS"
idf.py build
idf.py flash
```