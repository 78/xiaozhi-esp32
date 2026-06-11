# Custom Board: ESP32-S3-CAM N16R8 + OV5640

## Hardware

| Component | Model | Notes |
|---|---|---|
| MCU | ESP32-S3-WROOM-1 N16R8 | 16MB Flash, 8MB PSRAM |
| Camera | OV5640 | 5MP, integrated on board |
| Display | ST7735 1.8" 160x128 | SPI |
| Amplifier | MAX98357A | I2S, mono |
| Microphone | INMP441 | I2S |

---

## Wiring Summary

### Display (ST7735 1.8")
| Pin | GPIO |
|-----|------|
| SCK | 19 |
| SDA | 20 |
| RST | 21 |
| DC  | 47 |
| CS  | 45 |
| BL  | 48 |
| VCC | 3.3V |
| GND | GND |

### Amplifier (MAX98357A)
| Pin  | GPIO |
|------|------|
| DIN  | 41 |
| BCLK | 14 |
| LRC  | 46 |
| SD   | 3.3V (always on) |
| GAIN | GND (15dB gain) |
| VCC  | 3.3V |
| GND  | GND |

### Microphone (INMP441)
| Pin | GPIO |
|-----|------|
| SD  | 42 |
| SCK | 2 |
| WS  | 1 |
| L/R | GND (left channel) |
| VDD | 3.3V |
| GND | GND |

### Camera (OV5640) — Integrated, no wiring needed
Uses ESP32-S3-EYE standard pinout (internal to board).

---

## Build Instructions

### Prerequisites
- ESP-IDF v5.5+
- Python 3.8+

### Steps

1. Clone Xiaozhi:
   ```bash
   git clone https://github.com/78/xiaozhi-esp32.git
   cd xiaozhi-esp32
   ```

2. Copy this folder into the boards directory:
   ```
   main/boards/s3n16r8-ov5640/
   ```
   Place these files inside:
   - `config.json`
   - `s3n16r8_ov5640_board.cc`
   - `CMakeLists.txt`
   - `README.md` (this file)

3. Run menuconfig to enable OV5640:
   ```bash
   idf.py set-target esp32s3
   idf.py menuconfig
   ```
   Navigate to:
   - `Component config → Camera configuration → Enable OV5640 support` ✓
   - `Component config → ESP PSRAM` → Enable ✓

4. Build:
   ```bash
   python scripts/release.py main/boards/s3n16r8-ov5640
   ```

5. Flash:
   ```bash
   idf.py -p COM3 flash
   # or use ESP Flash Download Tool at address 0x0
   ```

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Camera init failed | Check PSRAM is enabled in menuconfig |
| No audio output | Verify MAX98357A SD pin is connected to 3.3V |
| Display blank | Check BL (GPIO 48) is HIGH; verify SPI pins |
| OV5640 overheating | This is normal — optionally add small heatsink |

---

## Notes
- The OV5640 uses the same DVP pin layout as ESP32-S3-EYE
- PSRAM is required for camera frame buffers
- I2S port 0 is shared between speaker and mic (duplex mode)
