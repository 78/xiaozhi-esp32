# JC8012P4A1C

Board support for the Guition (晶彩) JC8012P4A1C — a 10.1" ESP32-P4 HMI board.

## Hardware

| Item | Value |
|---|---|
| SoC | ESP32-P4, silicon rev **v1.3** (not v3.x) |
| Wi-Fi / BLE | ESP32-C6 co-processor over ESP-Hosted SDIO 4-bit |
| PSRAM | 32MB @ 200MHz |
| Flash | 16MB (Boya) |
| Display | 10.1" MIPI-DSI, **JD9365**, 800×1280 native portrait |
| Touch | **GSL3680** (I2C 0x40) |
| Audio | ES8311 codec + NS4150 amplifier, MSM381A3729H9CP mic |
| Camera | MIPI-CSI 2-lane, 15P FPC connector (no sensor fitted) |
| Storage | microSD (SDMMC 4-bit) |
| RTC | RX8025T-UC with CR1220 backup cell |
| LED | WS2812 |

## Pinout

| Signal | GPIO |
|---|---|
| LCD reset | 27 |
| LCD backlight (`LCD_PWM`) | 23 |
| Amplifier enable (`PA_CTRL`) | 20 |
| Touch reset | 22 |
| Touch interrupt | 21 |
| I2C SDA | 7 |
| I2C SCL | 8 |
| I2S MCLK | 13 |
| I2S BCLK | 12 |
| I2S WS | 10 |
| I2S DOUT | 9 |
| I2S DIN | 11 |

ES8311, GSL3680, RX8025T and the CSI sensor all share the GPIO 7/8 I2C bus.

## Build

```bash
idf.py set-target esp32p4
idf.py menuconfig   # Xiaozhi Assistant -> Board Type -> JC8012P4A1C
idf.py build
idf.py -p /dev/ttyACM0 flash
```

The board exposes both a native USB port (`/dev/ttyACM0`) and a CH340 UART bridge (`/dev/ttyUSB0`).

## Panel notes

The JD9365 initialization sequence in `lcd_init_cmds.h` comes from the vendor SDK
(`esp_lcd_jd9365` v1.0.2). It is not interchangeable with the registry driver's built-in
defaults: those omit `{0x80, 0x01}`, the DSI lane-count select. Without that command the
DBI channel still answers ID reads (`LCD ID: 93 65 04`) but the DPI video path never
produces output, so the panel stays black.

DPI timing also follows the vendor values (60MHz, vsync back porch 8, front porch 20).
The registry macro's 80MHz / 12 / 30 combination does not lock on this panel.

## Backlight

`LCD_PWM` drives the enable pin of an MP3202 LED boost converter. The pin accepts PWM
electrically, but ESP32-P4's LEDC peripheral cannot route to GPIO 23 — it logs
`GPIO 23 is not usable, maybe conflict with others` and silently produces no output.
The board therefore uses `GpioBacklight`, which is on/off only. Dimming would need a
software PWM or a different LEDC routing.

## Camera

The CSI connector is unpopulated. Camera support is behind
`CONFIG_JC8012P4A1C_ENABLE_CAMERA` (default off). Enable it and add the matching
`CONFIG_CAMERA_*` sensor options once a module is attached.

## Touch

The GSL3680 driver lives in a separate component,
[`mangoo1/esp_lcd_touch_gsl3680`](https://components.espressif.com/components/mangoo1/esp_lcd_touch_gsl3680),
rather than in this directory.

It has to stay separate: the driver carries Silead's touch tracking algorithm
from the Linux kernel (`drivers/input/touchscreen/mediatek/gslX680/`), which is
GPL-2.0-or-later. This project is MIT, and vendoring GPL sources into it would
force the whole codebase to GPL and break the commercial-use grant that
downstream vendors rely on. Depending on it as a component keeps the license
boundary intact.

Touch init probes the I2C address first and continues without touch if the
controller does not answer, so a board with an unpopulated touch panel still
boots.

## Status

Verified on hardware (ESP32-P4 rev v1.3, 16MB flash):

- Display brings up cleanly — JD9365 reports `LCD ID: 93 65 04`, LVGL runs
  with no watchdog trips
- GSL3680 firmware uploads and the controller initializes
- ES8311 codec initializes, Wi-Fi associates through the C6

Not systematically tested: touch coordinate reporting, speaker output level,
and camera. The GSL3680 driver logs
`startup_chip failed read 0xb0 = 5a,5a,5a,5a` and continues — firmware upload
succeeds, but the chip does not return a real startup status.

Two warnings appear on every boot and are expected:

- `esp_lcd_panel_swap_xy: swap_xy is not supported by this panel` — JD9365
  has no coordinate swap, harmless unless you want landscape rotation
- `Version mismatch: Host [2.12.0] > Co-proc [2.1.0]` — the ESP-Hosted slave
  firmware on the C6 is older than the host component

## Not yet implemented

- microSD mounting
- RX8025T RTC (the firmware uses SNTP instead)
- WS2812 LED — GPIO not yet confirmed against the schematic
- Backlight dimming (see above)
- Landscape orientation: the panel is driven in its native 800×1280 portrait mode
