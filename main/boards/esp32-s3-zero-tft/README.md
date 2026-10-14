# ESP32-S3 Zero N4R2 + TFT 240x240 + PCM5102A + INMP441

Initial **hardware-specific** XiaoZhi board definition for the Waveshare ESP32-S3-Zero
using ESP32-S3FH4R2 (dual-core, **4MB Quad flash / 2MB Quad PSRAM**). This
board has **not yet been hardware-validated**. The LCD controller is assumed to be ST7789.

## Wiring (3.3V GPIO)

| Peripheral | Module pin | ESP32-S3-Zero GPIO |
| --- | --- | --- |
| INMP441 | SCK / BCLK | 2 |
| INMP441 | WS / LRCL | 1 |
| INMP441 | SD / DOUT | 6 |
| INMP441 | L/R | GND (left slot) |
| PCM5102A | BCK | 5 |
| PCM5102A | LCK / LRCK | 4 |
| PCM5102A | DIN | 7 |
| TFT ST7789 | SCL / SCK | 12 |
| TFT ST7789 | SDA / MOSI | 11 |
| TFT ST7789 | DC / A0 | 10 |
| TFT ST7789 | CS | 9 |
| TFT ST7789 | RES / RST | 8 |
| TFT ST7789 | BL / LED | 13 (PWM) |
| Onboard WS2812 RGB LED | Data | 21 (already connected) |
| Onboard BOOT button | Input | 0 (already connected) |

All modules must share GND. INMP441 uses 3.3V power; respect each module's
power/logic specifications. PCM5102A is **line-level**, not a speaker power
amplifier: use its **L** output into an AUX input or an amplifier for this
initial mono-left firmware. The module's SCK/MCLK, XSMT and FMT wiring/jumpers
must suit I2S without MCLK; many breakout boards generate their own system
clock, but not every board does. Never connect raw speaker terminals directly
to PCM5102A.

Do not use GPIO19/20 for external peripherals; they serve native USB.
Avoid GPIO3/45/46 (strapping pins) and GPIO33-37 (not exposed on Zero).
GPIO21 is dedicated to the onboard WS2812. BOOT on GPIO0 is for both USB
download mode and short-click chat toggling after normal boot.

## Display notes

This preliminary configuration assumes a **four-wire SPI ST7789**, 240x240,
SPI mode 0, panel offsets X=0 and Y=0, inversion on. Some 240x240 ST7789
modules need a nonzero vertical offset (often 80) or different inversion,
RGB ordering, or SPI mode; verify the actual LCD controller/pin labels before
soldering and change `config.h` in this board only if necessary.

## Build

Use **ESP-IDF v6.1** (v6.0.1 minimum in this repository):

```sh
python scripts/build.py esp32-s3-zero-tft --name esp32-s3-zero-tft --language vi-VN --wake-word disabled
idf.py size
```

The preset selects `partitions/v2/4m.csv`, **Quad PSRAM** instead of the
ESP32-S3 default Octal mode, and no wake word / AFE for the first
resource-constrained smoke test. The 4MB partition layout is **factory-only**:
it has **no dual OTA app slots**, so firmware OTA A/B is not available.
Verify that the compiled app and assets fit in their partitions. A successful
compile does not imply working mic, playback, LCD, Wi-Fi or wake word.

For native USB flashing, enter download mode with the BOOT button when needed,
then run `idf.py -p <port> flash monitor` using your actual serial port.
After power-up test PSRAM initialization, Wi-Fi provisioning, button input,
LCD orientation/backlight, microphone captured PCM, and DAC audio output.

## Audio limitations

The software uses two independent I2S controllers (I2S0 TX / I2S1 RX).
The current `NoAudioCodecSimplex` uses mono samples in the left I2S
slot: connect the **L analog output** of PCM5102A to the amplifier/AUX.
INMP441 L/R must be held low to match left-channel capture.

Acoustic echo cancellation with a playback reference is **not configured**
for this external mic/DAC pair. Do not assume full-duplex AEC will work.
