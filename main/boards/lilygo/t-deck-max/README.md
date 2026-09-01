# LilyGo T-Deck MAX

The T-Deck MAX is an ESP32-S3 XiaoZhi device with a GDEQ031T10 e-paper
display, an ES8311 audio codec, a CST328 touch controller, and a TCA8418
keyboard controller. This variant uses the ESP32-S3 Wi-Fi interface and the
ES8311 codec for standard XiaoZhi audio playback.

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
Xiaozhi Assistant -> Board Type -> LilyGo T-Deck MAX
```

**Configure the PSRAM mode**

```
Component config -> ESP PSRAM -> SPI RAM config -> Mode (QUAD/OCT) -> Quad Mode PSRAM
```

**Build**

```bash
idf.py build
```

## Hardware

- MCU: ESP32-S3
- Network: ESP32-S3 Wi-Fi
- Flash / PSRAM: 16 MB / 8 MB
- E-paper: GDEQ031T10, 320x240
- Audio codec: ES8311, I2C address 0x18
- Touch controller: CST328, I2C address 0x1A
- Keyboard controller: TCA8418, I2C address 0x34
- IO expander: XL9555, I2C address 0x20
- I2C: SDA GPIO13, SCL GPIO14
- E-paper SPI: SCK GPIO36, MOSI GPIO33, DC GPIO35, CS GPIO34

The MAX variant keeps the complete 240x320 LVGL frame, converts RGB565
content to the UC8253 1-bit e-paper buffer, and supports full and partial
refreshes. The bottom rows are part of the panel and are not cropped.
