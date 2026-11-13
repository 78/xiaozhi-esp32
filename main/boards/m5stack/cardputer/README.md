# M5Stack Cardputer

Port for the original M5Stack Cardputer (StampS3, ESP32-S3FN8). For the
Cardputer Adv, use `m5stack/cardputer-adv` instead.

## Hardware

| Component | Spec |
|-----------|------|
| MCU | ESP32-S3FN8 @ 240MHz (no PSRAM) |
| Flash | 8MB |
| Display | ST7789V2 1.14" 240x135 |
| Microphone | SPM1423 (PDM) |
| Amplifier | NS4168 (I2S) |
| Keyboard | 56 keys, GPIO matrix via 74HC138 |
| Battery | 120mAh + 1400mAh base, ADC on GPIO10 |

## Pins

| Function | GPIO |
|----------|------|
| LCD MOSI / SCLK / CS / DC / RST / BL | 35 / 36 / 37 / 34 / 33 / 38 |
| Speaker BCLK / LRCK / DATA | 41 / 43 / 42 |
| Mic CLK / DATA (PDM) | 43 / 46 |
| Keyboard address (74HC138 A0-A2) | 8, 9, 11 |
| Keyboard inputs | 13, 15, 3, 4, 5, 6, 7 |
| Battery ADC | 10 (1:2 divider) |
| BOOT button | 0 |

## Half-duplex audio

GPIO43 is wired to both the mic clock and the speaker LRCK, so the mic and the
speaker cannot run at the same time. `CardputerAudioCodec` owns I2S0 and
switches it between PDM RX and standard TX on demand: playback always wins, and
while the speaker is active (plus a 300 ms hold) the mic reports silence. This
means:

- Use the default turn-based chat mode. Realtime chat and barge-in (talking
  over the reply) can't work on this hardware, and there is no AEC.
- The wake word doesn't hear anything while the device is speaking.

If the mic level is too low or clips, adjust `AUDIO_MIC_GAIN` in `config.h`.

## Keys

| Key | Action |
|-----|--------|
| BOOT / Enter | Start or stop a chat |
| `;` / `.` (up / down) | Volume |
| `,` / `/` (left / right) | Brightness |
| `W` on the WiFi config screen | Enter WiFi credentials on the keyboard |
| `S` on the WiFi config screen | Pick a saved network |

## Using a self-hosted server

The firmware talks to the server set by `CONFIG_OTA_URL` (upstream default)
unless a custom OTA URL is saved on the device. To set one without
rebuilding:

1. Enter WiFi setup: on first boot it starts automatically; otherwise press
   BOOT while the startup screen is showing.
2. The keyboard WiFi screen and the setup hotspot (`Xiaozhi-XXXX`) are both
   active. From a phone, join the hotspot and open http://192.168.4.1.
3. On the **Advanced** tab, enter the server's OTA URL (for
   xiaozhi-esp32-server: `http(s)://<host>/xiaozhi/ota/`) and save.
4. Enter WiFi on either the keyboard (`W`) or the web page.

## Build

```bash
python scripts/build.py m5stack/cardputer
```

Tested with the tagged ESP-IDF v6.1 image (`espressif/idf:v6.1`). A build
from the `release-v6.1` branch image (`v6.1-829-g9a97f6c54ec`) failed every
HTTPS connection on this board with "PSA signature verification failed" /
`mbedtls_ssl_handshake returned -0x3000`.

```bash
docker run --rm -v "$PWD":/project -w /project espressif/idf:v6.1 \
  python scripts/build.py m5stack/cardputer
```

## Flash

```bash
python -m esptool --chip esp32s3 -b 460800 -p PORT \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xd000 build/ota_data_initial.bin \
  0x20000 build/xiaozhi.bin \
  0x600000 build/generated_assets.bin
```

Replace `PORT` with the serial device (for example `/dev/cu.usbmodem1101`).
This writes the individual images and keeps saved WiFi; writing
`merged-binary.bin` at 0x0 instead erases the NVS partition too.
To enter download mode, hold G0 while plugging in USB.
