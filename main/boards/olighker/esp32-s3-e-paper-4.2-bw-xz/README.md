# OLIGHKER ESP32-S3 E-Paper 4.2 BW XZ

This board targets a 4.2-inch SSD1683 monochrome panel at 400 × 300.
The display, microSD, audio, battery, button, and LED connections match the
[OLIGHKER 1.54-inch E-Paper board](../esp32-s3-e-paper-1.54-bw-xz/README.md).

The background task uses the same policy as the 1.54-inch board. It updates changed areas with
the panel's OTP partial waveform, restores contrast with a full refresh after up to five partial
updates, and cleans any remaining partial updates before putting the panel into deep sleep
after about eight seconds without a visible change.

With ESP-IDF v6.1 active, build the English firmware with the following command.

```powershell
python scripts/build.py olighker/esp32-s3-e-paper-4.2-bw-xz --name esp32-s3-e-paper-4.2-bw-xz --language en-US
```

Flash `build/merged-binary.bin` at `0x0`.
