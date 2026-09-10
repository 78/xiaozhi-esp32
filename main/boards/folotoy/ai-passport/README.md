# FoloToy AI Passport (ai-passport)

Board definition that lets the XiaoZhi voice assistant run on the
[FoloToy AI Passport](https://github.com/FoloToy/ai-passport) wearable.

## Hardware

- MCU: ESP32-C3, 8 MB flash, **no PSRAM**, USB Serial/JTAG console
- Audio: ES8311 codec (I2C 0x18) on the shared I2C bus, I2S full-duplex
  (amplifier enable not wired, treated as always on)
- Display: ST7789 (ST7789P3) 240x320 portrait, 4-line SPI
- Battery: CW2017 fuel gauge (I2C 0x63, optional)
- Buttons: UP / DOWN / OK share GPIO0 (ADC1_CH0) through a resistor ladder

Pin mapping follows `ai-passport/components/bsp/include/bsp_pins.h`:

| Function | Pin |
| --- | --- |
| LCD MOSI / SCLK / CS / DC | 9 / 8 / 1 / 20 |
| LCD backlight (PWM) | 21 |
| I2S MCLK / BCLK / WS / DOUT / DIN | 6 / 5 / 3 / 2 / 4 |
| I2C SDA / SCL | 10 / 7 |
| Buttons (ADC ladder) | GPIO0 |

## Build

```sh
python scripts/build.py folotoy/ai-passport --name ai-passport
```

Outputs `build/merged-binary.bin` (8 MB flash, USB Serial/JTAG console).

## Controls

The three physical keys map to XiaoZhi's voice-assistant actions:

- **OK** — single click: toggle the chat state (or enter Wi-Fi config mode
  while starting up)
- **UP** — single click: volume +10; long press: max volume
- **DOWN** — single click: volume -10; long press: mute

Because the ladder shares one ADC pin, XiaoZhi reads it as three
independent ADC buttons (the same pattern as the ESP-BOX-Lite).

## Notes / calibration

- Display orientation, color inversion and the backlight PWM polarity were
  taken from the Passport BSP; verify on real hardware and adjust the
  `DISPLAY_*` macros in `config.h` if the image is rotated/inverted or the
  backlight is reversed.
- The ADC button voltage windows assume the Passport's external 10 kOhm
  pull-up. Re-measure with the Button page if thresholds drift.
- CW2017 presence is optional; without the chip the status bar shows no
  battery level and the board keeps working.
