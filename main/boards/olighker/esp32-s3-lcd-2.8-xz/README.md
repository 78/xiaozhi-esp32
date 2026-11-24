# OLIGHKER ESP32-S3 LCD 2.8 XZ

The 2.8-inch ST7789 uses a 240x320 panel with a 320x240 landscape UI.
LCD SPI: SCK GPIO12, SDI GPIO11, CS GPIO17, DC GPIO21, RST GPIO38, and active-high backlight GPIO47.
microSD has a separate SPI bus: SCK GPIO40, MOSI GPIO9, MISO GPIO13, CS GPIO14.
Audio, battery, buttons, and LEDs match the [OLIGHKER OLED board](../esp32-s3-oled-2.42-xz/README.md).

With ESP-IDF v6.1 active, build the English firmware using:

```powershell
python scripts/build.py olighker/esp32-s3-lcd-2.8-xz --name esp32-s3-lcd-2.8-xz --language en-US
```

The merged image is written to `build/merged-binary.bin` for flashing at `0x0`.
