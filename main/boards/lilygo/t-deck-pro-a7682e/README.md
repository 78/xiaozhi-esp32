# LilyGo T-Deck Pro A7682E

The T-Deck Pro A7682E is an ESP32-S3 XiaoZhi device with a GDEQ031T10
e-paper display, an A7682E modem, a PDM microphone, and a keyboard. This
variant uses ESP32-S3 Wi-Fi for the XiaoZhi network connection and the A7682E
for local TTS audio output.

Official github: [T-Deck-Pro](https://github.com/Xinyuan-LilyGO/T-Deck-Pro)

## Configuration

**Set the compilation target to ESP32S3**

```bash
idf.py set-target esp32s3
```

**Open menuconfig**

```bash
idf.py menuconfig
```

**Select the board**

```
Xiaozhi Assistant -> Board Type -> LilyGo T-Deck Pro A7682E
```

**Configure the PSRAM mode**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Quad Mode PSRAM
```

**Build**

```bash
idf.py build
```

## Audio limitation

**The Pro version currently cannot modify the voice or timbre.** It uses the
A7682E modem's default local TTS voice through `AT+CTTS`; XiaoZhi server-side
voice selection is not applied by this firmware.

Output volume is a separate setting and does not change the TTS voice.

## Hardware

- MCU: ESP32-S3
- Network: ESP32-S3 Wi-Fi
- Flash / PSRAM: 16 MB / 8 MB
- E-paper: GDEQ031T10, 320x240
- TTS output: A7682E modem local TTS
- Microphone: PDM, DATA GPIO17, CLOCK GPIO18
- Keypad controller: TCA8418, I2C address 0x34
- I2C: SDA GPIO13, SCL GPIO14
- A7682E UART: ESP32 RX GPIO11, ESP32 TX GPIO10
