# LC-S3-WIFI-1.54TFT

An ESP32-S3 voice-assistant board with a 1.54-inch 240x240 ST7789 display and an ES8311 audio codec.

## Hardware

- SoC: ESP32-S3
- Display: 1.54-inch 240x240 SPI ST7789 (SPI mode 0, 40 MHz)
- Audio codec: ES8311 over I2S + I2C; speaker PA enable on GPIO 8
- Buttons: BOOT (GPIO 0), volume up (GPIO 40), volume down (GPIO 39)
- LED: GPIO 46
- Battery: ADC1 channel 8 with charging-detect input on GPIO 38

## Build

```sh
python3 scripts/build.py lc-s3-wifi-1.54tft --name lc-s3-wifi-1.54tft
```