# Guition JC4827W543

4.3" 480x272 IPS panel board, vendor model **ESP32-4827A043** (schematic v0.2).
ESP32-S3-WROOM-1 **N4R8**: 4 MB flash, 8 MB OPI PSRAM.

| Block | Part | Pins |
|---|---|---|
| Display | NV3041A, QSPI | CS 45, CLK 47, D0 21, D1 48, D2 40, D3 39 |
| Backlight | LEDC PWM | 1 |
| Touch | GT911, I2C1 | SDA 8, SCL 4, INT 3, RST 38 |
| Microphone | INMP441 (external, header P3) | SCK 6, WS 7, SD 15 |
| Speaker | NS4168 (on board, connector P7) | BCLK 42, LRCK 2, DOUT 41 |
| Speaker, `-ext-amp` | I2S amplifier, e.g. MAX98357A (external, headers P2/P3) | BCLK 9, LRC 14, DIN 16 |
| Button | BOOT switch | 0 |
| Battery | divider to ADC1_CH4 | 5 |

## Build

```sh
python scripts/build.py guition/jc4827w543
```

Language and wake word are build parameters, not board settings:

```sh
python scripts/build.py guition/jc4827w543 --language en-US
```

## Two speaker variants

- **`guition-jc4827w543`** (default) plays through the on-board NS4168 and the
  P7 speaker connector - stock hardware, only a speaker to plug in.
- **`guition-jc4827w543-ext-amp`** (`CONFIG_JC4827W543_EXTERNAL_AMP`) drives an
  external I2S amplifier on the expansion headers instead, and parks the
  NS4168's lines low (see below).

```sh
python scripts/build.py guition/jc4827w543 --name guition-jc4827w543-ext-amp
```

## Starting a conversation

A tap anywhere on the screen toggles listening. The board has to provide this:
the stock chat UI installs no touch handler of its own, so without it the only
way in is the BOOT switch, which shares IO0 with the panel's TE line and is not
reachable on a cased device.

The handler cannot hang on a single object either. LVGL delivers a click to the
topmost clickable object under the finger, the chat area fills the screen, and
chat bubbles are created per message on top of that - so the screen, the
container and the chat area all carry it, and new bubbles are flagged to bubble
their events up.

## What this board does not have

**No microphone.** The board has an NS4168 amplifier on the P7 speaker
connector, but nothing that records. This board definition therefore expects an
INMP441 on the "Extended IO" headers - pin headers, no soldering. IO6/IO7/IO15,
and IO9/IO14/IO16 for the `-ext-amp` speaker, are free in the vendor pin table:
they touch none of the display, touch, SD-card or amplifier nets.

**4 MB of flash, so there is no OTA.** `partitions/v2/4m.csv` splits it as
3008KB of app and 1024KB of assets, with no second app slot. A firmware update
means a cable, and the app already uses about 90% of its partition.

A wake word does not fit on top of this. `esp_srmodel_init("model")` looks up a
partition by that name, so it needs one carved out of the 4MB, and the app plus
a 16px font's assets leave no room for it. Turkish has no WakeNet model either,
so it would have to be an English or Chinese phrase.

The text font that lands in the assets partition is the `common` tier - `basic` plus the DeepSeek
tokenizer's character set, so mostly CJK - at 1228KB for 20px against 865KB for
16px, which is why that path uses 16px. `basic` itself, meaning ASCII, Latin-1
and the strings from `main/assets/locales/*/language.json`, is linked into the
app rather than stored in the partition, so Latin text never depends on the
partition copy.

## Hardware notes

**The serial port is the ESP32-S3's own USB Serial/JTAG (IO19/20), not a CH340.**
`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` is in `config.json` for that reason.
Without it the console goes to UART0 (IO43/44), which is a physical pin on the
module but is not brought out to any connector: the ROM bootloader still talks,
and then nothing. On Windows the port shows up under VID `303A`.

**The on-board NS4168 amplifier has no enable pin.** Its CTRL is tied to
VOUT-BAT through 1M, so it is live whenever the board is. When `-ext-amp`
drives an external amplifier, the NS4168's three lines (IO2, IO41, IO42) would
otherwise float next to the QSPI display and the amp would play the noise they
pick up as a steady hiss - loud enough to drag the supply rail down by hundreds
of millivolts. In that variant `SilenceOnboardAmplifier()` parks them low at
boot; do not remove it while the external amp is the output.

**The GT911 picks its I2C address from INT during reset**: INT low gives 0x5D,
INT high gives 0x14. A bring-up that toggles RESET but leaves INT floating
latches a random address, and touch then dies silently on the next reboot while
the display keeps working. `esp_lcd_touch_gt911` drives INT itself, so
`levels.interrupt = 0` pins the address to 0x5D.

**The panel's reset is not wired to the ESP32.** IO38 is the touch reset. The
NV3041A comes up from power-on and the init sequence unlocks it, so
`reset_gpio_num` is `GPIO_NUM_NC`.

**Only 0 and 180 degrees work on this glass.** 90/270 tear and shift, so
`swap_xy` returns `ESP_ERR_NOT_SUPPORTED` rather than painting garbage. The
panel is also IPS, which means colour inversion has to be on.

**The battery divider is not what the schematic draws.** The schematic shows
33K/100K, factor 1.33; the fitted parts are 160K/220K, factor ~1.73, and that
is what `config.h` uses. The chip has no factory ADC calibration, so expect a
few percent of error; adjust the pair if a board reads off. Charge state cannot
be read at all: the charger's status leg is not connected to the ESP32, so
`charging_pin` stays `GPIO_NUM_NC`. Use ADC1; ADC2 is unavailable while WiFi is
up.

The percentage comes from `adc_battery_estimation`'s generic LiPo curve, so
treat it as approximate - optimistic in the flat part of the discharge, and
more so while charging.

**Pulling the USB cable resets the board.** The 3.3 V regulator browns out while
the boost converter takes over. It happens every time and is not a fault.

**There are three switches and only one of them reaches the ESP32.** `RST` is
wired to `EN`, `BOOT` to IO0 (10K pull-up, switch to ground) - that one is
`BOOT_BUTTON_GPIO`. `SW1` is neither: it goes to the `KEY` pin of the charger,
and exists to wake the boost converter after the battery protection has cut
out. Pressing it produces nothing in firmware, which is easy to mistake for a
dead button handler.
