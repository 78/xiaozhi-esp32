[product](https://fr.aliexpress.com/item/1005010207331462.html)

One of the cheapest kits in a watch form factor: ESP32-S3, a 128x64 I2C
SSD1306 OLED, one touch button, and a small speaker/mic. No wake-word
hardware requirement, no screen backlight (OLED), single button for
everything.

## Programming

Before assembling the product, unglue the battery from the board to access
the programming header. You'll need an ESP32 programming dongle with
control of the EN and IO0 pins. The USB connector is not wired to the USB
pins of the ESP32-S3 USB interface and can only be used for charging.

There is no auto-reset circuit, so entering the download/bootloader mode is
always manual: hold **IO0**, tap **EN**, keep holding IO0 for ~2 more
seconds, then release.

## Custom firmware on this board: a battery-first, always-usable watch

This board's firmware was rewritten with one goal: make an always-connected
voice assistant actually work on a wrist-watch-sized battery, without
sacrificing "it just works" reliability. Stock xiaozhi behavior (Wi-Fi +
audio pipeline always running) drains this battery in a few hours. The
changes below get it down to **~10µA at rest** (real ESP32-S3 deep sleep),
while still turning on instantly and never failing to wake up.

### 1. Real deep sleep, not just a dimmed screen

- After 1 minute idle on the clock face, the OLED panel is turned off
  *and* the chip enters `esp_deep_sleep_start()` — CPU, RAM, radio, all
  off. This is fundamentally different from disabling Wi-Fi in software:
  deep sleep means the chip is fully powered down and the only thing left
  running is a small RTC domain.
- **Wake source: the single button, via `ext1`**, level-low
  (`esp_sleep_enable_ext1_wakeup`). Waking up is a full chip reset — there
  is no "reconnect" state machine that can get stuck, which is what makes
  the wake-up 100% reliable: it's just a boot.
- **The two gotchas that will silently ruin this** (found the hard way):
  - The button's GPIO needs its **RTC pull-up explicitly re-enabled**
    (`rtc_gpio_pullup_en`) and the **RTC power domain kept on**
    (`esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON)`)
    before sleeping. Without this, the pin floats low in deep sleep and
    the "wake on low" condition is true immediately — the board looks
    like it never sleeps, waking itself up right after entering sleep.
  - Any amplifier/peripheral enable pin must be **actively held** through
    sleep (`rtc_gpio_hold_en` + `gpio_deep_sleep_hold_en`), or it floats
    and the amp draws idle current the whole time the device is "asleep",
    defeating the entire point.
- See `hu_087_board.cc::EnterDeepSleep()`.

### 2. The clock keeps working with the radio off

The ESP32-S3's RTC timer keeps counting straight through deep sleep. So
once the wall clock has been set once (SNTP, on a real power-on), every
subsequent wake-up can show the correct time **instantly, with Wi-Fi still
off** — reading `time()` costs nothing. Wi-Fi is only brought up again if
the user actually asks to talk to the assistant, exactly like the
factory/original firmware for this SKU behaves. See the
`modo_relogio_offline_` state in `hu_087_board.cc` / `hu087_display.cc`.

Net effect: turn the device on once (its physical switch/first boot) to
sync time over Wi-Fi; from then on, every button-wake shows the clock with
**zero network activity**, and Wi-Fi only comes up on demand.

### 3. One button, three unambiguous gestures

With no wake word and a single physical button, the click has to mean the
right thing depending on what's on screen — this project's `OnClick`
handler in `hu_087_board.cc` implements:

| Screen / state             | Click means                                              |
|-----------------------------|-----------------------------------------------------------|
| Asleep (deep sleep)          | Wake up and show the clock (this click is consumed by hardware, the button handler never even sees it) |
| Clock, offline               | Bring Wi-Fi up and start talking                          |
| "Connecting…" screen         | Enter Wi-Fi provisioning (in case the saved network changed) |
| Assistant is speaking         | Interrupt it (native `AbortSpeaking`); it resumes listening on its own |
| Assistant is listening        | Hang up — screen turns off immediately, deep sleep follows as soon as audio drains |

The first ~1.5s after a wake are deliberately deaf to clicks, so a finger
lingering on the button while it wakes can't accidentally trigger anything.

### 4. A `server_time` timezone bug worth knowing about

`ota.cc`'s activation response can carry a `server_time` block with a
`timestamp` and a `timezone_offset`. The original code added the offset to
the timestamp *before* calling `settimeofday()` — i.e. it stored **local**
time in what every other part of the system (including this board's own
`localtime_r` + `TZ` clock rendering) treats as UTC. Whichever of
SNTP/`server_time` wins the race to set the clock decides whether you see
the correct time or a "double-subtracted" one, off by exactly the
timezone offset. Fixed here by keeping `settimeofday()` on the raw UTC
timestamp only — see `main/ota.cc`.

### Files touched

- `main/boards/hu-087/hu_087_board.cc` — deep sleep, wake source, button
  state machine, offline clock mode, Wi-Fi-on-demand.
- `main/boards/hu-087/hu087_display.{h,cc}` — clock/face/"Conectando…"
  screens, the sleep countdown, and the audio-level-driven mouth
  animation.
- `main/audio/audio_codec.{h,cc}` — a level callback used to drive the
  mouth from the real outgoing audio instead of a fixed timer.
- `main/display/oled_display.h` — exposes real panel power on/off and
  SSD1306 contrast control (wired up as a standard `Backlight`, so the
  assistant's `self.screen.set_brightness` MCP tool works even though
  this OLED has no physical backlight).
- `main/ota.cc` — the timezone double-offset fix described above.
- `main/application.cc` — skips the OTA version check specifically on a
  deep-sleep wake (it would otherwise cost a few extra seconds every time
  the clock is checked).
