# Seeed Studio reTerminal D1001

XiaoZhi port for the Seeed Studio reTerminal D1001 (ESP32-P4NRW32 +
ESP32-C6 Wi-Fi coprocessor, ES8311 speaker, ES7210 dual-mic, 800x1280 MIPI-DSI
panel, GSL3670 touch).

## Status

Validated on hardware (P4 v1.3, 32 MB Winbond flash ef/4019, 32 MB PSRAM):

- PCA9535 power sequence (power hold, panel rails, PA gating, LCD/touch
  reset). The expander macros are pin *masks*; the BSP bit numbers are
  0/2/7/8/11/12.
- ES8311 playback and ES7210 capture codecs on the separate TX/RX I2S buses
  (16 kHz capture baseline, matching the Seeed factory BSP).
- ESP-Hosted SDIO link to the factory ESP32-C6 firmware (Wi-Fi, config AP and
  the XiaoZhi cloud connection all work; the co-proc runs esp-hosted 2.3.0
  while this image ships host 2.12.x, see "C6 firmware" below).
- GPIO3 button: a short press toggles the chat state, or enters Wi-Fi config
  mode while the device is starting; a 3 s hold powers the board off (see
  "Power off" below).
- 800x1280 JD9365DA-H3 MIPI-DSI panel (2 lanes at 1000 Mbps, LDO3 at 2.5 V,
  60 MHz DPI clock, RGB565 with one framebuffer) using the Seeed init
  sequence in `lcd_init_cmds.h`, plus the GPIO14 PWM backlight
  (`jd9365: LCD ID: 93 65 04`, `Display initialized`).
- GSL3670 capacitive touch on I2C0, polled (no interrupt), registered as an
  LVGL input device (`gsl3670: load fw success`, `Touch panel initialized`).
  The driver directly unpacks hardware coordinate matrix and applies Silead linear
  calibration without GPL tracking algorithms (licensed under Apache-2.0, aligning
  with Seeed Studio's official repository).
- Onboard MIPI CSI-2 camera (see "Camera" below): the sensor answers at SCCB
  `0x36` with PID `0xeb52` and `EspVideo` opens the CSI/ISP device
  (`Camera init success, captured 150 frames in 5013ms`), so
  `self.camera.take_photo` is registered on the MCP server.
- Idle stability: two 30 min runs with the display on, wake word off
  (free SRAM flat at 248371) and wake word on (free SRAM flat at 250531),
  no resets.

Not yet validated: device AEC, 30 min continuous conversation, touch
coordinate orientation on the panel (the BSP values swap_xy=0, mirror_x=1,
mirror_y=1 are used as-is), and the captured image itself (orientation,
colour and a cloud round trip through `Explain()`).

### Camera

The module Seeed calls SC2356 is the same silicon as the SC202CS that
`espressif/esp_cam_sensor` already supports: same SCCB address `0x36`, same
PID `0xeb52`, and the Seeed BSP RAW8 1280x720 30fps register list matches the
registry one on 126 of 139 writes (the rest are AE/AGC start values that the
ISP overrides anyway). The registry driver is therefore enabled as-is,
instead of vendoring the BSP copy of the 1.2.0 `esp_cam_sensor`:

```
CONFIG_CAMERA_SC202CS=y
CONFIG_CAMERA_SC202CS_MIPI_RAW8_1280X720_30FPS=y   # MIPI CSI-2, 1 lane, 24 MHz
```

Wiring notes:

- SCCB shares I2C0 (GPIO37/38) with the GSL3670 touch controller, so the
  camera reuses `touch_i2c_bus_` with `init_sccb = false`. Creating a second
  master on the same pins is not possible.
- Reset, power-down and the rail enable are PCA9535 bits 9, 3 and 1, so
  `reset_pin`/`pwdn_pin` are `GPIO_NUM_NC` and `PowerUpCamera()` replays the
  BSP timing (EN, 50 ms, PWDN+RST released, 10 ms reset pulse, 50 ms).
- `esp_video_init()` aborts the whole init when no sensor answers, so the
  board probes `0x36` first and leaves `GetCamera()` null when the probe
  fails. That keeps a missing module from looking like a driver fault.
- Idle internal SRAM drops from ~248-250 KB to 244007 free / 231683 minimal
  with the camera open. The frame buffers live in PSRAM.

#### ISP pipeline controller is off on this board

`CONFIG_ESP_VIDEO_ENABLE_ISP_PIPELINE_CONTROLLER=n` overrides the P4 default.
With it enabled the firmware panics right after the sensor is detected:

```
I (2869) sc202cs: Detected Camera sensor PID=0xeb52
Guru Meditation Error: Core 0 panic'ed (Illegal instruction).
MEPC : 0x481991a0   MCAUSE : 0x00000002   MTVAL : 0x20fac7b3
```

`0x481991a0` is inside `esp_ipa_pipeline_create()` and `0x20fac7b3` decodes as
`sh2add`, a RISC-V Zba instruction. The prebuilt `libesp_ipa.a` that
`espressif/esp_ipa` 2.3.0 ships for ESP-IDF >= 6.0 is built for
`rv32i..._zba1p0_zbb1p0_zbs1p0`, while the 5.5 and 5.4 blobs in the same
component are not; `esp_ipa/CMakeLists.txt` picks the blob by IDF version
only, with no ESP32-P4 revision check. This board is a P4 v1.3
(`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`), which has no B extension, so every
IPA call traps. Turning the pipeline controller off removes the only caller
of `esp_ipa`.

The cost is that auto exposure, auto white balance and the rest of the IPA
loop do not run, so colour and exposure use the static ISP defaults. Re-enable
the option once `esp_ipa` ships a P4-rev-aware blob (or an IDF 6 blob without
Zba) and re-check the capture.

### Wake word and internal RAM

The AFE wake word (`wn9_nihaoxiaozhi_tts`) runs on the ESP-IDF 6.0 build, but
only with `CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM=y`. Without it the
esp_hosted SDIO mempools (40 + 47 KB, allocated from an ESP_SYSTEM_INIT_FN)
sit in RETENT_RAM, the only internal heap available before the scheduler
starts, and two things go wrong on this board:

- once `.data/.bss` grow (e.g. the touch driver) the main task stack can no
  longer be allocated: `Mem alloc fail. size 0x2a00 caps 0x804` /
  `assert failed: esp_startup_start_app app_startup.c:83 (res == pdTRUE)`;
- with less static data the firmware boots, but the esp-sr prebuilt
  libraries (`esp32p4_less_v3_idf6`) hang the CPU right after the WakeNet
  pipeline is created (HP watchdog reset loop, `rst:0x7`), apparently
  because an internal-RAM allocation inside the closed library fails and the
  library busy-waits instead of reporting it.

Both symptoms disappear when the mempools live in PSRAM (GDMA reaches PSRAM
on the P4). The same sources built with ESP-IDF 5.5.4 need the same option
(there the mempool allocation itself asserts at boot) and, because 5.5
cannot address this flash chip above 16 MB, would also need
`partitions/v2/16m.csv`; only the IDF >= 6.0 entry is kept in `config.json`.

### C6 firmware

The factory C6 image is esp-hosted 2.3.0. Host-driven slave OTA
(`esp_hosted_slave_ota_*`) requires slave >= 2.6.0 and espressif/esp-hosted-mcu
publishes no prebuilt C6 SDIO binaries, so the upgrade needs a self-built
slave image and a physical flashing path; it is tracked separately from the
P4 image and is not part of this port.

The hardware facts come from the
[reTerminal D10xx product documentation](https://wiki.seeedstudio.com/reterminal_d10xx_main_page/)
and the
[official reTerminal-D1001 repository](https://github.com/Seeed-Studio/reTerminal-D1001).
See `docs/reterminal-d1001-port-design.md` for the port design and milestones.

## Build

```sh
python3 scripts/build.py seeed-studio/reterminal-d1001 --name reterminal-d1001
```

Verified with ESP-IDF 6.0.2 (`espressif/idf:v6.0.2`). The factory images are published in the Seeed BSP repository
under `firmware/`.

The ESP32-P4 application and the ESP32-C6 ESP-Hosted slave are separate
firmware images with separate version lifecycles. Building or updating this
P4 image does not build or silently update the C6 firmware; record and
manage both versions independently.

## Power off

Holding the GPIO3 button for 3 s shows `正在关机 ...` on screen, mutes the
power amplifier, waits 500 ms so the speaker does not pop and then drops the
PCA9535 power-hold bit (`ReTerminalD1001Expander::PowerOff()`). The long press
is registered with `Button(gpio, false, 3000)`; the click threshold is
unchanged, and `iot_button` does not emit a single click after a long press
(`iot_button.c`, `PRESS_LONG_PRESS_UP_CHECK`), so the short-press behaviour
stays as it was.

Power-hold only gates the battery path. **Not measured yet:** with USB power
plugged in the rail may be fed from the host, in which case the board keeps
running after the bit drops; the firmware logs
`Still running after PowerOff(); external power?` when that happens. On battery
the board is expected to switch off. Both need a bench check.

## On-screen settings

The status bar carries three targets and there is no separate settings home
page. Tapping the network icon opens the Wi-Fi page directly (the scan starts
with the page); the rotation symbol next to it opens the screen-orientation
page and the volume symbol after it opens the volume page. The network icon
itself belongs to the stock top bar underneath and never
receives taps, so a transparent 48 px tall hit area covering its footprint is
placed over it inside `status_bar_` (the transparent full-width layer stacked
over the top bar, which is what actually receives taps up there). The rotation
and volume symbols are labels in the same font, size and colour as the rest of
the strip, with a 16 px extended click area; `status_bar_` gets a 48 px minimum
height so the whole band is hit-tested. `status_bar_` is therefore taller than
`top_bar_`, so the hit area and the icon row are aligned to the *top* of
`status_bar_`, not to its middle: both bars are screen children at y=0 with the
same `spacing(2)` top padding and the icon row is exactly one icon-font line
high, so a top alignment puts all three glyphs on the network icon's line.
Any target opens a full-screen overlay
(`settings_ui.h/.cc`) that covers the status bar; the main UI keeps running
underneath and is restored when the overlay closes. The back arrow on both
top-level pages closes the overlay. The GPIO3 button behaviour is unchanged.

Every page uses the same shell: an app bar (back arrow, title, optional
refresh) over a card list — 16 px radius, 16 px gaps, 96 px rows, 72 px
buttons, one accent colour (#2F6BFF) for primary actions. Icons come from the
board's existing Material Symbols font (`font_material_symbols_30_4`) through
`lvgl_theme`'s `icon_font()` / `large_icon_font()`; no font file is added. Card
colours are derived from the theme background, so light and dark both work, and
the status-bar rotation icon and every overlay icon are re-pointed at the new font when
the theme is reloaded (the old font is freed at that moment).

- **Wi-Fi 网络**: signal-strength icon per row, lock icon on encrypted
  networks, `手动输入` and `已保存` at the bottom; scans in a background task (`esp_wifi_scan_start`, up to 20
  results), lists SSID/signal/encryption, opens an on-screen keyboard for the
  password and connects. Saved networks can be reconnected or deleted. The new
  credentials are written through `SsidManager` before the attempt and rolled
  back if the connection does not come up within 20 s. Switching networks while
  the device is idle goes through `EnterWifiConfigMode()` first, so the protocol
  is torn down cleanly before the station restarts.
The password / SSID keyboard uses a phone-style layout set with
`lv_keyboard_set_map()`: `qwertyuiop` / `asdfghjkl` (between two hidden
half-width spacers, which is what indents the row) / shift `z…m` backspace /
`1#` `,` space `.` OK. LVGL's built-in keyboard handler dispatches on the exact
button texts `abc`, `ABC` and `1#` (`lv_keyboard.c`,
`lv_keyboard_def_event_cb`), so the mode keys carry those labels — a `⇧` or
`?123` label would be typed into the text area instead of switching the layout.
The special layout holds the digits and `!@#$%^&*()_-+=~/\:;"'<>?[]{}`.

Only two per-key looks exist in an `lv_buttonmatrix` (default and
`LV_BUTTONMATRIX_CTRL_CHECKED`), so the function keys (`abc`/`ABC`, `1#`,
backspace, OK) are the checked ones and get one shade more contrast than the
letters; the accent is no longer used as the checked colour, which is what
painted a block of blue over the left column and the bottom row. Pressing any
key tints it towards the accent. The OK key therefore looks like the other
function keys rather than like a blue primary button; the blue primary action
stays the separate `连接` button. OK itself runs the page's primary action
(`LV_EVENT_READY` is bound to `WifiConnectConfirm` on the password page and to
`WifiManualNext` on the manual-SSID page), so a password can be submitted from
the keyboard.

- **音量**: one card with the current level, a 56 px tall `lv_slider` (0-100)
  and a mute button. Dragging only updates the label; the codec is written once
  on `LV_EVENT_RELEASED` / `LV_EVENT_PRESS_LOST`, because
  `AudioCodec::SetOutputVolume()` commits to NVS on every call
  (`main/audio/audio_codec.cc`). Mute stores the current level and sets 0; the
  same button restores the stored level. No second copy of the volume is kept.
- **屏幕方向**: four large tiles with the current one highlighted, then
  `保存并重启` with a confirmation step. 0/90/180/270, stored in NVS (namespace `reterminal`, key
  `rotation`) and applied at boot; the device reboots 2 s after the choice is
  confirmed. Every non-zero angle is applied with `lv_display_set_rotation()`
  (the MIPI port runs with `sw_rotate`); the panel and the touch controller keep
  their validated 0° flags because LVGL 9 rotates the pointer coordinates
  itself (`lv_indev.c`, `lv_display_rotate_point`).

The rotation itself runs on the ESP32-P4 PPA, not on the CPU. The board build
appends `CONFIG_LVGL_PORT_ENABLE_PPA=y`; with that symbol set and the MIPI port
configured with `flags.sw_rotate = true`, `esp_lvgl_port` creates a PPA SRM
context instead of the extra CPU rotation buffer and every flush of a rotated
frame goes through `lvgl_port_ppa_rotate()`
(`esp_lvgl_port_disp.c:420-444`, `:644-699`).

The board build also appends `CONFIG_LV_USE_KEYBOARD=y`, `CONFIG_LV_USE_LIST=y`,
`CONFIG_LV_USE_TEXTAREA=y`, `CONFIG_LV_USE_SPINNER=y` and
`CONFIG_LV_USE_SLIDER=y`, which the project defaults either leave off or do not
pin.

## Pin facts (from the Seeed BSP)

| Function        | Pins / address                              |
|-----------------|---------------------------------------------|
| Audio I2C (I2C1)| SDA GPIO20, SCL GPIO21                      |
| PCA9535         | 0x20 (power hold bit8, PA bit11, LCD pwr bit0, LCD rst bit2, backlight bit7, touch rst bit12) |
| ES8311 (DAC)    | 0x18, I2S0 TX: MCLK 33, BCLK 32, WS 31, DOUT 30 |
| ES7210 (ADC)    | 0x40, I2S1 RX TDM: MCLK 29, BCLK 28, LRCK 27, DIN 26 |
| C6 SDIO         | CMD 6, D0 7, D1 8, D2 9, D3 10, CLK 11, CHIP_PU 13 |
| Panel           | 800x1280 JD9365DA-H3, MIPI-DSI 2 lane, LDO3 2.5 V, backlight PWM GPIO14 |
| Touch           | GSL3670 on I2C0 (SDA 37, SCL 38), INT GPIO16 (unused, polled) |
| Button          | GPIO3                                       |

## HTTP 推送面板

局域网 HTTP 推送 Markdown 上屏 / 屏上选择接口，见 [PUSH_PANEL.md](PUSH_PANEL.md)。
