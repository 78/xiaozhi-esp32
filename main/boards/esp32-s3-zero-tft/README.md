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
| External WAKE button | NO pushbutton | GPIO14 to GND (internal pull-up) |

All modules must share GND. INMP441 uses 3.3V power; respect each module's
power/logic specifications. PCM5102A is **line-level**, not a speaker power
amplifier: use its **L** output into an AUX input or an amplifier for this
initial mono-left firmware. The module's SCK/MCLK, XSMT and FMT wiring/jumpers
must suit I2S without MCLK; many breakout boards generate their own system
clock, but not every board does. Never connect raw speaker terminals directly
to PCM5102A.

Do not use GPIO19/20 for external peripherals; they serve native USB.
Avoid GPIO3/45/46 (strapping pins) and GPIO33-37 (not exposed on Zero).
GPIO21 is dedicated to the onboard WS2812. **Both BOOT (GPIO0) and external
WAKE (GPIO14)** start/toggle the conversation when clicked. Holding either
button for 2 seconds enters Wi-Fi provisioning after normal boot.
Keep GPIO0 high during reset for normal boot; holding BOOT through reset
enters the ROM download mode. Never wire the external button to GPIO0.

## Display notes

This preliminary configuration assumes a **four-wire SPI ST7789**, 240x240,
SPI mode 0, panel offsets X=0 and Y=0, inversion on. Some 240x240 ST7789
modules need a nonzero vertical offset (often 80) or different inversion,
RGB ordering, or SPI mode; verify the actual LCD controller/pin labels before
soldering and change `config.h` in this board only if necessary.

## Build

Use **ESP-IDF v6.1** (v6.0.1 minimum in this repository):

```sh
python scripts/build.py esp32-s3-zero-tft --name esp32-s3-zero-tft --language vi-VN
idf.py size
```

The preset selects `partitions/v2/4m.csv`, **Quad PSRAM** instead of the
ESP32-S3 default Octal mode, and the built-in **WakeNet9 Hi Lily** model (`wn9_hilili_tts`),
with speech-processing/AEC and wake-word audio upload off for the
memory-constrained first build. The LVGL pool is reduced to 256KB and its
image cache to 64KB on this board, to leave PSRAM for WakeNet9/AFE.
**The compiled model and firmware are NOT yet confirmed to fit**. The 4MB partition layout is **factory-only**:
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

## Hi Lily wake word + pushbuttons

The default variant explicitly selects `CONFIG_USE_AFE_WAKE_WORD=y`,
`CONFIG_SR_WN_WN9_HILILI_TTS=y`, and disables the old Chinese WakeNet
model. Hi Lily is an actual WakeNet9 model (not merely a changed label).
It requires the ESP-SR 2.4.7 package to provide the symbol and model.
The runtime fetches speech assets from the 4MB layout's 1MB assets partition.
If the assets image or firmware exceeds that partition, the **4MB physical
flash is a blocker**, not an option to declare a bigger flash in sdkconfig.
No successful ESP-IDF build or physical wake test is claimed here.

To switch off wake detection for comparison:
```sh
python scripts/build.py esp32-s3-zero-tft --name esp32-s3-zero-tft --language vi-VN --wake-word disabled
```

## Wi-Fi configuration on phone

The firmware uses `78/esp-wifi-connect` v3.3.1. Hold either button for
about 2 seconds while the device is running (or configure automatically
on first boot). Connect your phone to the `Xiaozhi-XXXX` hotspot.
Open **http://192.168.4.1**. The existing portal automatically fetches
`GET /scan` every 5 seconds, shows selectable detected SSIDs, allows
entering the password, and uses `POST /submit` to connect. Only 2.4GHz
Wi-Fi is supported by ESP32-S3.

The current embedded vendor portal **does not have a dedicated manual
"Quét Wi-Fi" button**; the list already rescans automatically. Changing
the literal portal HTML requires a separately versioned override/fork of
`78/esp-wifi-connect` (rather than editing generated
`managed_components`). This is not falsely claimed as integrated.

## Display goals

See [HILILY_DISPLAY_PLAN.md](HILILY_DISPLAY_PLAN.md) for Worker weather/location,
Vietnamese lunar calendar, mascot emotion assets, music playback lyric/timing,
and battery charging/low-level rules. These features are not yet all wired
end-to-end, especially battery module and synchronized music player.

Enable battery status (only after wiring the specified 1-cell voltage divider
and charger CHRG# pin) with `CONFIG_HILILY_BATTERY_MONITOR=y` in a build option.
The LCD DC pin changes from GPIO10 to GPIO15 under this option.
