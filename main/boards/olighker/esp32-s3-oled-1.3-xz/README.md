# OLIGHKER ESP32-S3 OLED 1.3 XZ

This board uses a 1.3-inch SH1106 128x64 I2C OLED (SDA GPIO41, SCL GPIO42).
The SH1106 uses the project's SH1106 panel driver.
The firmware probes I2C addresses `0x3c` and `0x3d`.
The remaining hardware connections match the [2.42-inch board](../esp32-s3-oled-2.42-xz/README.md).

With ESP-IDF v6.1 active, build this board with:

```powershell
python scripts/build.py olighker/esp32-s3-oled-1.3-xz --name esp32-s3-oled-1.3-xz --language en-US
```

The default firmware locale is English (`en-US`), with the “Hi ESP” wake word.

The merged test image is written to `build/merged-binary.bin`.
