# OLIGHKER ESP32-S3 E-Paper 2.9 BW XZ

This board uses a monochrome SSD1680 panel.
The UI is displayed in landscape at 296 × 128. The panel RAM is addressed at 128 × 296.
The background task transfers the previous and current images and uses an SSD1680 differential
waveform. Only changed pixels are driven during a partial refresh.
The task uses the same periodic full refresh and idle sleep policy as the 1.54-inch board.

The display, microSD, audio, battery, button, and LED connections match the
[OLIGHKER 1.54-inch E-Paper board](../esp32-s3-e-paper-1.54-bw-xz/README.md).

With ESP-IDF v6.1 active, build the English firmware with the following command.

```powershell
python scripts/build.py olighker/esp32-s3-e-paper-2.9-bw-xz --name esp32-s3-e-paper-2.9-bw-xz --language en-US
```

Flash `build/merged-binary.bin` at `0x0`.
