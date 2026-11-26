# OLIGHKER ESP32-S3 E-Paper 1.54 BW XZ

This board targets a 200 × 200 monochrome SSD1681-compatible panel. The background refresh task uses
the SSD1681 partial waveform and one changed window per frame. It performs a full
refresh on initialization and after up to five partial updates to restore contrast. After the image
settles, it cleans any remaining partial updates and puts the panel into deep sleep. Chat and setup
text wraps within the visible area, and long messages advance through static pages.

| E-paper signal | GPIO |
| --- | --- |
| BUSY (active high) | 47 |
| RST | 38 |
| DC | 21 |
| CS | 17 |
| SCK | 12 |
| SDI / MOSI | 11 |

The panel uses SPI3. The microSD card uses SPI2 with SCK GPIO40, MOSI GPIO9, MISO GPIO13,
and CS GPIO14. Audio, battery, buttons, and LEDs follow the
[OLIGHKER OLED board](../esp32-s3-oled-2.42-xz/README.md).

With ESP-IDF v6.1 active, build the English firmware with the following command.

```powershell
python scripts/build.py olighker/esp32-s3-e-paper-1.54-bw-xz --name esp32-s3-e-paper-1.54-bw-xz --language en-US
```

Flash `build/merged-binary.bin` at `0x0`.
