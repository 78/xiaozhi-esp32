# OLIGHKER ESP32-S3-OLED-2.42-XZ

Custom ESP32-S3 voice-assistant board using an ESP32-S3-WROOM-1-N16R8 module.

## Hardware

- ESP32-S3-WROOM-1-N16R8: 16 MB flash and 8 MB OPI PSRAM
- ICS-43434 I2S microphone: SCK 5, WS 4, DATA 6
- MAX98357A I2S amplifier: BCLK 15, LRC 16, DATA 7
- SSD1309 128x64 I2C OLED: SDA 41, SCL 42, address 0x3c or 0x3d
- WS2812B status LED: GPIO 48
- BOOT/TALK, VOL+, VOL- buttons: GPIO 0, 10, 39
- BQ24074 status: CHG# GPIO 1, PGOOD# GPIO 8
- Battery ADC: GPIO 2, 100 kOhm / 100 kOhm divider
- microSD SPI: CS 14, SCK 12, MOSI 11, MISO 13, mounted at `/sd`
- Native USB: D- 19, D+ 20

The SSD1309 is initialized through ESP-IDF's SSD1306-compatible panel driver.

## Build

Activate the ESP-IDF v6.1 environment, then run:

```powershell
python scripts\build.py olighker/esp32-s3-oled-2.42-xz --name esp32-s3-oled-2.42-xz --language zh-CN
```

The build script produces `build/merged-binary.bin`. Add `--zip` to create a release package.
