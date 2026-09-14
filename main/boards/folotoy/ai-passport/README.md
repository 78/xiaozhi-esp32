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

## Power management

The Passport is a battery wearable, so the board gives up power in two steps,
both counted from the last key press:

| Idle | What happens |
| --- | --- |
| 5 min | Backlight off, display power-save mode, both I2S channels stopped, CPU allowed to drop to 40 MHz and enter tickless light sleep |
| 15 min | Deep sleep; any key wakes the device through GPIO0 and restarts the application |

Both steps are gated by `Application::CanEnterSleepMode()`, so an ongoing
conversation or playback pushes the deadline back instead of being interrupted.
Any key press cancels the countdown (`OnPressDown`, so a long press counts too).
`kBacklightOffSeconds`, `kDeepSleepSeconds` and `kPowerSaveCpuMaxFreq` at the top
of `ai_passport_board.cc` tune the policy; the CPU step needs `CONFIG_PM_ENABLE`
and `CONFIG_FREERTOS_USE_TICKLESS_IDLE`, which `config.json` enables.

That gate has one consequence worth knowing about: a board that never reaches
`kDeviceStateIdle` keeps the screen lit indefinitely. The common case is an
unprovisioned board sitting in Wi-Fi config mode, which the state machine only
lets move to activating or audio testing. Sleeping outside the idle state would
be a framework change affecting every board, so it is not done here - a Passport
delivered unconfigured should be configured, not left on a shelf.

Stopping I2S is what makes the CPU step pay off at all. The I2S standard-mode
driver holds an `ESP_PM_APB_FREQ_MAX` lock while a channel is enabled, and that
lock both pins the APB clock at 80 MHz and stops the PM subsystem from entering
automatic light sleep. Codec construction enables both channels once and only
`esp_codec_dev_close()` stops them again, so a board that has not played anything
since boot would never reach light sleep. The wake path restores the clocks both
explicitly and through `esp_codec_dev_open()`, so either route is enough. On a
real device the explicit stop reports `0 changed` - the config-mode alert and the
audio-testing path open and close the PCM path early enough to release the lock
anyway - so it is the fallback for a board that has not played anything yet.

Deep sleep follows the FoloToy AI Passport BSP shutdown contract
(`docs/reference/shinku-chen/deep-sleep-peripheral-power-off` in the BSP repo),
in this order:

1. Arm the GPIO0 low-level wake source with IDF's
   `esp_sleep_enable_gpio_wakeup_on_hp_periph_powerdown()`. ESP32-C3 has no
   EXT0/EXT1, and the function takes a pin *bit mask*, so passing
   `GPIO_NUM_0` instead of `1ULL << GPIO_NUM_0` silently arms nothing. If arming
   fails the board falls back to a timed wake so the device cannot be stranded
   asleep.
2. CW2017 `CONFIG=0xF0`, read back 5 ms later, retried once on mismatch. The
   board init reverses this with the chip's restart-then-active cycle, so a
   gauge that slept before a deep sleep reports again after the wake.
3. ES8311 suspend: `esp_codec_dev` disable/close first, then the BSP's suspend
   register sequence written through the codec control interface, with the key
   registers read back. REG0E is checked as `0x7F`, not the BSP's `0xFF`: bit 7
   of that register does not latch on this part, so the BSP's expectation can
   never be met and its own board logs a verification failure on every sleep.
   The registers that do matter (REG00/01/0D/12/45) are verified as written.
4. MCLK/BCLK/WS/DOUT/DIN released as inputs with no internal pulls.
5. SDA/SCL released the same way (terminal: no I2C transaction is valid after
   this point).
6. Under the LVGL lock: panel display-off and Sleep In, backlight PWM stopped at
   zero, then CS high / SCLK / MOSI / DC / backlight low, each held through deep
   sleep with `gpio_deep_sleep_hold_en()`.

A failed step is logged but never aborts the shutdown, so the device cannot be
left awake with a half-shut-down board. If deep sleep somehow returns, the board
restarts instead of trying to recover the released buses.

Pin holds are latched in the RTC domain and survive the deep-sleep reset, so
`gpio_hold_dis()` has to be called for every held pin on the way back up.
`ReleaseDeepSleepHolds()` does that first thing in the board constructor. On this
board the holds that survive a wake are GPIO1 (LCD CS) in `RTC_CNTL_PAD_HOLD_REG`
and GPIO8/GPIO9 (LCD SCLK/MOSI) in `RTC_CNTL_DIG_PAD_HOLD_REG`; without the
release they stay held and the panel never comes back.

What software cannot fix: the Passport does not wire the amplifier enable to the
MCU (`AUDIO_CODEC_PA_PIN` is `NC`), so amplifier standby current, regulator
quiescent current, the external I2C pull-ups and cell self-discharge all remain.
Isolate those on hardware if standby current still looks high.

Status of hardware verification (ESP32-C3, IDF 6.0.2). Verified on the device:
board init and a stable idle loop, the backlight-off stage, CW2017 sleep with a
matching readback, the ES8311 suspend sequence passing its register readback, and
deep sleep being entered and left again through a wake source. Not verified: the
actual idle and deep-sleep currents; that the panel and backlight visibly come
back after a wake (the holds above were confirmed released through
`RTC_CNTL_PAD/HOLD` register readbacks, but the panel itself was not inspected);
a key press as the deep-sleep wake source; whether the CPU step earns its keep
(`CONFIG_PM_ENABLE` interacts with Wi-Fi and the USB Serial/JTAG console); and
whether disabling `CONFIG_ESP_SLEEP_GPIO_ENABLE_INTERNAL_RESISTORS` saves
anything on top of the external 10 kOhm pull-up. Note that `PowerSaveTimer`
ignores the return value of `esp_pm_configure`, so if the CPU step appears to do
nothing, check whether the PM build options actually took effect.

## Notes / calibration

- Display orientation, color inversion and the backlight PWM polarity were
  taken from the Passport BSP; verify on real hardware and adjust the
  `DISPLAY_*` macros in `config.h` if the image is rotated/inverted or the
  backlight is reversed.
- The ADC button voltage windows assume the Passport's external 10 kOhm
  pull-up. Re-measure with the Button page if thresholds drift.
- CW2017 presence is optional; without the chip the status bar shows no
  battery level and the board keeps working.
