# Guition JC8048W550

5" 800x480 RGB panel board, marked **JC8048W550** (ESP32-S3, 16 MB flash,
8 MB PSRAM, CH340 serial bridge).

| Block | Part | Pins |
|---|---|---|
| Display | 800x480 RGB565, no DISP pin | DE 40, VSYNC 41, HSYNC 39, PCLK 42 (16 MHz, falling edge) |
| Display data | B0-B4 / G0-G5 / R0-R4 | B 8,3,46,9,1  G 5,6,7,15,16,4  R 45,48,47,21,14 |
| Backlight | LEDC PWM | 2 |
| Touch | GT911, I2C0 | SDA 19, SCL 20, RST 38, INT not wired |
| Speaker | I2S amplifier on the speaker connector | BCLK 0, LRC 18, DOUT 17 |
| SD card (unused) | SPI | CS 10, MOSI 11, SCK 12, MISO 13 |

## Where the pins come from

The vendor demo package (Arduino demos for the panel, GT911 and audio) and the
community ESPHome definition of this board. There is no schematic, so **none of
it has been measured on hardware**. Two things to check first:

- Colours: the ESPHome definition inverts colours, the vendor Arduino demo does
  not. `esp_lcd` RGB panels cannot invert, so this build does not.
- PSRAM: the build uses the project default (octal, 80 MHz). If the boot log
  reports a PSRAM failure, the module has a different PSRAM.

## Verified on hardware

Display, backlight and speaker work. Two things were needed to get a picture:

- hsync/vsync idle low (the vendor demo's polarity 0).
- Backlight PWM at 1220 Hz. The stock 25 kHz default left the screen dark.

Touch initialises (GT911 at 0x5D); its coordinates and the INMP441 microphone
path are not checked.

## Display

The 800x480 screen is split in two. The left 240 px column shows the emotion
(an image at most 240 px wide, any height, or a glyph), bottom-aligned. The right 560 px shows the
conversation while talking and the idle dashboard otherwise. The background is
fixed at `#18181C`.

### Idle dashboard

While the device is idle the right side shows:

- a card with the clock, the date, the city, today's weather and the next seven
  days;
- the top five GitHub repositories about AI (tagged `llm`, created in the last
  seven days, most starred first). Tapping a repo opens a detail card with its
  full description, language, stars, forks, topics and last update; tap outside
  the card or the close button to dismiss it.

The data is fetched by the device itself, only while it is idle, the clock is
set and enough internal RAM is free:

| Data | Service | Refresh |
|---|---|---|
| Location (city, latitude, longitude) | `ipwho.is`, looked up from your public IP address | once per boot |
| Weather (today and 7 days) | Open-Meteo, no key | every 30 minutes |
| Repositories | GitHub search API, no token (about 10 requests per minute allowed) | every 60 minutes |

**Privacy:** your public IP address is sent to `ipwho.is`, and the approximate
location it returns is sent to Open-Meteo. Nothing is stored: the data lives in
RAM and is fetched again after a reboot.

If a request fails the last data stays on screen, dimmed with an `offline` tag
(`no data` if nothing was ever fetched; `loading...` until the first attempt
finishes).

Repo names and descriptions and the city are UTF-8 and use the device's text
font (the one in the assets partition), so they show every character that font
has: the default assets carry the Noto `common` set (Latin, Cyrillic, kana,
hangul and about 5,000 Chinese characters). A character the font lacks shows as
a blank or a box. Emoji are removed because that font has no glyph for them.

## Vietnamese fonts

The idle calendar card uses two fonts compiled into the firmware, `font_vi_20.c`
and `font_vi_28.c`, because the assets font lacks some Vietnamese letters
(for example `ỵ` in the year name Tỵ). They are Noto Sans Regular (SIL Open
Font License 1.1) with ASCII, Latin-1 and the Vietnamese letters, generated
files: do not edit them by hand. To regenerate (Node.js required), with
`NotoSans-Regular.ttf` from `https://github.com/notofonts/notofonts.github.io`
(`fonts/NotoSans/full/ttf/`):

```sh
for N in 20 28; do
  npx --yes lv_font_conv@1.5.3 --font NotoSans-Regular.ttf --size $N --bpp 4     --format lvgl --lv-include lvgl.h --lv-font-name font_vi_$N     -r 0x20-0x7E,0xC0-0xFD,0x102-0x103,0x110-0x111,0x128-0x129,0x168-0x169,0x1A0-0x1A1,0x1AF-0x1B0,0x1EA0-0x1EF9,0xB7     -o font_vi_$N.c
done
```

## Build

```sh
python scripts/build.py guition/jc8048w550
```

## Audio

**Microphone: external INMP441.** The vendor pin tables define no microphone,
so one has to be wired to the P2/P4 headers:

| INMP441 | Board |
|---|---|
| WS | P2 IO11 |
| SCK | P2 IO12 |
| SD | P2 IO13 |
| GND and L/R | P4 GND (L/R low = left channel) |
| VDD | P4 3.3V |

IO11-13 are the SD-card lines, so the card slot must stay empty. This wiring is
from a proposed diagram and has not been checked on hardware.

The vendor `Audio_test` demo puts the speaker bit clock on IO19, but IO19 is the
touch SDA line. The demo that runs both together uses IO0, which this board
definition uses. IO0 is also the BOOT switch, so there is no button input.

## Starting a conversation

A tap on the emotion column (the left 240 px) toggles listening. Taps on the
right panel do nothing, except on a repository, which opens its detail card.

## Flashing

The CH340 on this board uses the normal auto-reset: flash over the serial port
(COM port on Windows) with `idf.py -p <port> flash`.
