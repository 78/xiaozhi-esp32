# ESP32-S31-Function-CoreBoard-1

This board definition targets Espressif's ESP32-S31-Function-CoreBoard-1 and
requires an ESP-IDF master revision with `esp32s31` target support.
Chip-wide flash, PSRAM, CPU, and Wi-Fi defaults live in
`sdkconfig.defaults.esp32s31`; `config.json` contains only this board's release
variant settings.

## Supported on-board hardware

- ESP32-S31-WROOM-3 with 16 MB flash and octal PSRAM
- ES8311 mono audio codec
- On-board microphone
- NS4150B mono speaker amplifier and J9 speaker connector
- One WS2812 RGB status LED on GPIO60
- BOOT button on GPIO61
- Wi-Fi networking

The board has no display, so XiaoZhi uses `NoDisplay`. A display can be added
later through the J2 GPIO header by extending this board implementation. Do not
reuse GPIO50 through GPIO57, GPIO60, or GPIO61 because they are assigned to the
on-board audio, LED, and BOOT button.

The on-board YT8531 Ethernet PHY is not initialized by this board definition
yet; the initial firmware uses Wi-Fi.

ESP-SR AFE, WakeNet, and VAD are supported. The ES8311 path has one microphone
input and no playback reference channel, so device-side AEC is not enabled.

## Closest existing board

`main/boards/esp-spot` is the closest structural match in this repository: it
also has no display and combines an ES8311 codec, a GPIO-controlled speaker
amplifier, buttons, and an addressable RGB status LED. The new implementation
keeps only that reusable structure and uses this board's own schematic pin map.
`main/boards/m5stack-cardputer-adv` documents the same ES8311 + NS4150B chip
pair, but its display, keyboard, and always-on amplifier topology are different.

## Audio pin map

| Signal | GPIO |
| --- | ---: |
| ES8311 I2C SCL | 50 |
| ES8311 I2C SDA | 51 |
| I2S MCLK | 52 |
| I2S BCLK / SCLK | 53 |
| I2S DIN / ES8311 ASDOUT | 54 |
| I2S WS / LRCK | 55 |
| I2S DOUT / ES8311 DSDIN | 56 |
| NS4150B PA control | 57 |

## Build

```sh
source /path/to/esp-idf-master/export.sh
python3 scripts/release.py esp-s31-function-coreboard-1
```

After connecting a speaker to J9, flash the generated firmware through either
USB Type-C programming port. Physical hardware validation should cover
microphone capture, speaker playback, BOOT-button interaction, RGB status
indication, Wi-Fi provisioning, and reconnect behavior.
