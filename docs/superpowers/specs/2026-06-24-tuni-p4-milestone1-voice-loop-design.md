# Tuni on ESP32-P4 — Milestone 1: Voice Loop on Onboard Audio

**Date:** 2026-06-24
**Status:** Approved (design), revised after code review
**Repos:** `xiaozhi-esp32` (firmware, primary), `robo-worker` (backend, dev-only change)
**Related:** `handoff.md` (Tuni P4 hardware spec), `DSOLUTION_HANDOFF.md` (prior ESP32-S3 bring-up), `robo-worker/CLAUDE.md`, `robo-worker/src/xiaozhi/router.ts`, `robo-worker/src/agents/tutor.ts`

> **Revision note:** §3, §4, §5, §6 were tightened after a review raised six issues:
> build config not applied by `idf.py` + missing silicon-revision variants; incomplete dev
> provisioning; bypass not fail-closed; raw-PCM requires protocol version 1; 24 h token not
> refreshed; and "button-to-talk" mislabeled. All six are addressed below.

---

## 1. Goal & scope

Get a bare **Waveshare ESP32-P4-NANO** running this firmware as a Tuni device that holds a real
**half-duplex voice conversation** with the `robo-worker` backend, using the NANO's **onboard ES8311
audio** (single SMD mic + speaker interface).

This is the fastest path to a "talking robot" and de-risks the toolchain (ESP-IDF + P4 + C6 WiFi),
the WS protocol, and the auth handshake — **before** the Tuni-specific peripherals (480×480 ST7701S
panel, ICS-43434 mics, MAX98357A amps, MG90S servos, WS2812B LED) arrive. Those layer on in later
milestones, in the order given by `handoff.md` §8.

**In scope (Milestone 1):**
- New `tuni-p4` board: builds, boots, brings up C6 WiFi, serial log.
- Full WS voice loop to `robo-worker`: VAD → STT → LLM → TTS, using onboard ES8311 audio.
- **Always-on open-mic VAD** (no wake word, no press-to-talk); VAD detects end-of-utterance.
- Backend dev-bypass auth so OTA returns a usable session token without on-device crypto.

**Out of scope (later milestones — see §8):** the custom display, dual far-field mics + AFE,
stereo amps, servos, chest LED, fuel gauge, and the production on-device ECDSA device-JWT.

## 2. Approach

**Chosen: a new, self-contained `tuni-p4` board, reduced from the existing `esp32-p4-nano` board.**
It reuses the proven P4 + C6-WiFi + ES8311 base and grows into the full Tuni product board across
subsequent milestones (display, mics, servos, LED added as separate, well-bounded units).

Rejected alternatives:
- **Add a "no-display" build variant to the stock `esp32-p4-nano` board.** Pollutes an upstream
  reference board with Tuni-only conditionals; harder to evolve into a distinct product.
- **Throwaway quick bring-up board.** Wasted work — we already know the product board's shape.

## 3. Firmware: `main/boards/tuni-p4/`

Placed directly under `main/boards/` (like `dsolution-ostb`), so no `"manufacturer"` key is required
in `config.json`. Modeled on `main/boards/waveshare/esp32-p4-nano/`, stripped of the MIPI-DSI
display, GT911 touch, camera, and backlight.

### 3.1 `config.h`
Onboard-audio pin map, reused verbatim from the proven `esp32-p4-nano` config (same MCU module):
- I2S: `MCLK=GPIO13`, `WS=GPIO10`, `BCLK=GPIO12`, `DIN=GPIO11`, `DOUT=GPIO9`
- Codec: `PA=GPIO53`, I2C `SDA=GPIO7` / `SCL=GPIO8`, `ES8311_ADDR = ES8311_CODEC_DEFAULT_ADDR`
- `AUDIO_INPUT_SAMPLE_RATE = AUDIO_OUTPUT_SAMPLE_RATE = 24000`
- `BOOT_BUTTON_GPIO = GPIO35`
- No `DISPLAY_*` / camera / backlight defines.

### 3.2 `config.json` — two silicon-revision variants (load-bearing)
The ESP32-P4 silicon revision changes required sdkconfig. `CONFIG_ESP32P4_SELECTS_REV_LESS_V3`
**defaults to `y`** in a normal local build (ESP-IDF `esp_hw_support/port/esp32p4/Kconfig.hw_support`:
`default n if IDF_CI_BUILD` / `default y`) — so a rev≥3 ("P4X") build must set it **explicitly to `n`**
plus `CONFIG_ESP32P4_REV_MIN_300=y` (merely omitting the rev keys still yields a rev<3 build). Picking
the wrong variant can break flash/boot, so it must be resolved against the **physical** chip (see §6).

```json
{
    "target": "esp32p4",
    "builds": [
        {
            "name": "tuni-p4",
            "sdkconfig_append": [
                "CONFIG_SLAVE_IDF_TARGET_ESP32C6=y",
                "CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y",
                "CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y",
                "CONFIG_USE_AUDIO_PROCESSOR=y",
                "CONFIG_SR_VADN_VADNET1_MEDIUM=y",
                "CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y",
                "CONFIG_ESP32P4_REV_MIN_100=y"
            ]
        },
        {
            "name": "tuni-p4-p4x",
            "sdkconfig_append": [
                "CONFIG_SLAVE_IDF_TARGET_ESP32C6=y",
                "CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y",
                "CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y",
                "CONFIG_USE_AUDIO_PROCESSOR=y",
                "CONFIG_SR_VADN_VADNET1_MEDIUM=y",
                "CONFIG_ESP32P4_SELECTS_REV_LESS_V3=n",
                "CONFIG_ESP32P4_REV_MIN_300=y"
            ]
        }
    ]
}
```
**These `sdkconfig_append` entries are applied only by `scripts/release.py` (it resolves the
`CONFIG_BOARD_TYPE_*` symbol and appends the list); a plain `idf.py build` does NOT read
`config.json`.** §6 therefore documents both the `release.py` path and the exact equivalent
`menuconfig` settings for iterative dev. The C6/PSRAM base also comes from `sdkconfig.defaults.esp32p4`.

### 3.3 `tuni_p4.cc`
```cpp
class TuniP4 : public WifiBoard {
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    Display* display_ = nullptr;

    void InitializeCodecI2c();      // same as esp32-p4-nano
    void InitializeButtons();       // BOOT: WiFi-config while kDeviceStateStarting only (no talk toggle)

  public:
    TuniP4() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeButtons();
        display_ = new NoDisplay();          // from main/display/display.h
    }
    AudioCodec* GetAudioCodec() override {   // onboard ES8311
        static Es8311AudioCodec codec(codec_i2c_bus_, I2C_NUM_1,
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &codec;
    }
    Display* GetDisplay() override { return display_; }
};
DECLARE_BOARD(TuniP4);
```
`WifiBoard` already provides the C6 WiFi lifecycle, WiFi-config AP, OTA check, and WS protocol
selection — no overrides needed beyond audio + display.

### 3.4 Board registration
- `main/Kconfig.projbuild`: add `config BOARD_TYPE_TUNI_P4` (bool, `depends on IDF_TARGET_ESP32P4`)
  to the `BOARD_TYPE` choice, near the other P4 boards (~line 374).
- `main/CMakeLists.txt`: add `elseif(CONFIG_BOARD_TYPE_TUNI_P4)` → `set(BOARD_TYPE "tuni-p4")`,
  near the `BOARD_TYPE_WAVESHARE_ESP32_P4_NANO` branch (~line 474).

## 4. Audio & conversation flow (half-duplex)

**No firmware protocol changes.** `main/protocols/websocket_protocol.cc` already opens with a `hello`
advertising `audio_params {format:"pcm", sample_rate:24000, channels:1, frame_duration:40}` — exactly
what the backend's `Tutor.onXiaoziMessage` (`robo-worker/src/agents/tutor.ts`) negotiates and replies
to. The shared STT/LLM/TTS pipeline runs entirely backend-side.

### 4.1 Protocol version MUST be 1 (raw PCM, no binary header)
`websocket_protocol.cc` frames binary audio by `version_`: `version_==2`→`BinaryProtocol2` header
(version+timestamp+payload_size), `version_==3`→`BinaryProtocol3` header, **`version_==1`→raw payload**
(`SendAudio`, lines 28-56). The backend treats every binary frame as raw PCM with **no** header
parsing, so any version ≠ 1 corrupts both directions. The firmware default is `version_ = 1`
(`websocket_protocol.h:27`), but it is overridden by NVS `settings.GetInt("version")` (line 87), so a
**stale NVS `version` is the hazard**. Two mitigations, both required:
- Backend OTA response includes numeric **`"version": 1`** in the `websocket` block. `ota.cc` persists
  numeric items via `settings.SetInt` (lines 177-180), pinning NVS `version=1` over any stale value.
- **`idf.py erase-flash` before initial bring-up** clears stale NVS from earlier firmware.

### 4.2 Turn flow — always-on open-mic VAD (no press-to-talk, no wake word)
Per the product model (`handoff.md` §4), there is **no push-to-talk and no wake word**. The device
auto-starts a continuous listening loop after connecting and uses VAD to detect end-of-utterance. The
firmware already has the pieces: an `on_vad_change` callback drives end-of-speech
(`application.cc:109-114, 310-314`: "VAD end-of-speech, sending listen.stop"), and after TTS `stop`,
any mode except `kListeningModeManualStop` auto-returns to `kDeviceStateListening`
(`application.cc:623-631`). What changes vs. stock is the **trigger**: we remove wake-word/button gating
and auto-enter listening.

**Mode:** `GetDefaultListeningMode()` returns `kListeningModeAutoStop` **only when `aec_mode_ == kAecOff`**
(else `kListeningModeRealtime`/full-duplex). Milestone 1 keeps **server-AEC and device-AEC off** so the
auto-derived mode is `kListeningModeAutoStop` — half-duplex, VAD-bounded turns.

**Configuration:**
- **Disable wake word** (no `Hi Lily`/`Nihaoxiaozhi`); keep the AFE running for **VAD only**
  (`CONFIG_USE_AUDIO_PROCESSOR=y`, voice processing on, wake-word detection off).
- **VAD model = neural VADNet1-medium**, pinned explicitly via `CONFIG_SR_VADN_VADNET1_MEDIUM=y`.
  ESP-SR 2.3.0 offers only WebRTC (the Kconfig default) or VADNet1-medium; the P4 has ample headroom
  for the neural model (`AFE_MODE_HIGH_PERF`, model in PSRAM), and `sdkconfig.defaults.esp32p4` does not
  pin a VAD model, so the explicit select is required or it falls back to WebRTC. `afe_audio_processor.cc`
  auto-selects the flashed VADNet model (`esp_srmodel_filter`, `ESP_VADN_PREFIX`); `vad_mode=VAD_MODE_0`
  and `vad_min_noise_ms=100` are tunables if the onboard mic over/under-triggers. The firmware keeps an
  RMS silence fallback for noisy-baseline mics (`audio_service.cc:240`).
- **Auto-start listening after the audio channel opens** (post WiFi + OTA): programmatically enter the
  autostop listening loop once, instead of waiting on a wake word / BOOT press. Hook to wire during
  implementation — `StartListening()` / `ContinueOpenAudioChannel(kListeningModeAutoStop)`
  (`application.cc:754, 801`) fired off the network-ready / activation-done path.

**Loop (runs continuously):**
1. Onboard mic → AFE **VAD**; binary PCM 24k mono 40 ms frames stream over WS while the child speaks.
2. VAD end-of-speech → firmware sends `{type:"listen", state:"stop"}` → backend flushes STT.
3. Backend: STT → intent → Gemini → mood-tagged TTS chunks → streams `{type:"tts", state:...}` control
   + binary PCM frames back; firmware plays them through ES8311.
4. On TTS `stop` → firmware **auto-returns to Listening** → next open-mic VAD turn. Repeats indefinitely.

Single onboard mic only — **no beamforming/AEC** this milestone; the backend gates device audio while
replying (`isReplying`/`isTtsActive`), so while the robot speaks the child's audio is dropped — i.e.
**no mid-reply barge-in** (consistent with the handoff; the chest button arrives in a later milestone).
BOOT button is **not** used for talk; it stays only for WiFi-config while `kDeviceStateStarting`.

## 5. Auth — backend dev-bypass (fail-closed, mint-only)

Production model: device signs a ≤120 s **ECDSA P-256 device JWT**, `POST /xiaozhi/ota` mints a
session token (`websocket.token`), device connects `/xiaozhi/ws` with `Authorization: Bearer`. The
firmware does not implement device-JWT signing, so Milestone 1 uses a dev-only bypass. Because it
trusts a **spoofable `Device-Id` (MAC)** and mints a bearer token, it is designed fail-closed.

### 5.1 Activation — explicit conjunction
The bypass in `authenticateOta` (`src/xiaozhi/router.ts`) activates **only** when:
```ts
c.env.E2E_ENABLED === "1" && c.env.XIAOZHI_DEV_BYPASS === "1"
```
If either is unset/false, the existing JWT path runs unchanged. (`E2E_ENABLED` already gates the
provisioning routes, so this reuses the established dev posture and cannot be reached in production.)

### 5.2 Bypass behavior — mint-only, never writes device rows
When active, read the `Device-Id` header and:
1. `getDevice(db, deviceId)` — must exist and `status === "active"`, else reject.
2. `getBinding(db, deviceId)` — must exist **and** `binding.child_id === c.env.XIAOZHI_DEV_CHILD_ID`,
   else reject (binding mismatch / unbound → fail closed; never auto-rebind).
3. `mintSessionToken(env, deviceId, XIAOZHI_DEV_CHILD_ID)` with an **extended dev TTL** (see §5.5).

The bypass performs **no** inserts/upserts — provisioning is an explicit operator step (§5.3), so a
spoofed MAC for an unprovisioned device gets nothing even in dev.

### 5.3 One-time provisioning (operator, documented script)
The register API requires a valid EC P-256 JWK (`routes/devices.ts:21`, `public_key` non-nullable),
and `upsertDevice` refuses to overwrite an active device's key (409). So provisioning uses a
**dedicated throwaway dev keypair** (a real, structurally valid P-256 JWK — not a placeholder), via
the existing E2E routes:
1. `POST /api/devices/mock-child` `{ childId: XIAOZHI_DEV_CHILD_ID }` — seed the pinned child.
2. `POST /api/devices/register` `{ deviceId: <P4 MAC>, publicKey: <dev JWK>, status: "active" }`.
3. `POST /api/devices/:deviceId/claim` `{ childId: XIAOZHI_DEV_CHILD_ID }`.

`XIAOZHI_DEV_CHILD_ID` is a fixed UUID config (the single pinned bring-up child), set as a Worker var
alongside `XIAOZHI_DEV_BYPASS`.

### 5.4 OTA response
On success the OTA `websocket` block returns `{ url, token: <session token>, version: 1 }` (see §4.1
for why `version`). Production (flags off) is unchanged.

### 5.5 Token TTL & refresh limitation
`CheckNewVersion()→CheckVersion()` runs once in `ActivationTask` at boot (`application.cc:418`), **not**
on WS reconnect — so an expired session token causes WS 401s until reboot. For bring-up the bypass
mints an **extended dev TTL** (e.g. 30 days) so reconnects within a dev session don't 401.

**API change required:** `mintSessionToken(env, deviceId, childId, nowSec?)` currently hard-codes
`exp = nowSec + DEVICE_SESSION_TTL_SEC` (`auth/device.ts:70`) — there is no TTL parameter. Add an
optional **`ttlSec = DEVICE_SESSION_TTL_SEC`** parameter (or a dedicated `mintDevSessionToken`); the
bypass passes the 30-day value. A unit test asserts the resulting `exp` for both the default and dev
TTL. The reboot-to-refresh limitation is documented; OTA-refresh-on-401 is deferred firmware work
(not in Milestone 1).

### 5.6 Migration to production auth (Milestone 7)
`upsertDevice` replaces a public key **only** when the existing row's `status === "unclaimed"`
(`d1/devices.ts:34`) — both `active` **and** `revoked` rows reject the new key (409,
`DeviceKeyLockedError`). So **do not revoke**. To adopt the device's real on-board-generated key later:
`unclaimDevice(deviceId)` (deletes the binding and resets status to `unclaimed`; `d1/devices.ts:82-92`)
→ then `register`/`upsertDevice` with the real key. (Equivalently: delete both the `device_binding`
and `device` rows outright.)

### 5.7 Firmware: zero new auth code
Already sends `Device-Id: <MAC>` / `Client-Id` on OTA, persists the OTA `websocket.*` (token, version)
to NVS, and sends `token` as `Authorization: Bearer` on WS. `CONFIG_OTA_URL` already targets
`/xiaozhi/ota`.

### 5.8 Required negative tests (backend)
Prove **no token is minted** when: (a) `E2E_ENABLED !== "1"` (production); (b) `XIAOZHI_DEV_BYPASS`
unset; (c) device unbound / no binding; (d) `binding.child_id !== XIAOZHI_DEV_CHILD_ID`; (e) device
`status !== "active"`. Plus a positive test: provisioned+bound dev device mints a token.

## 6. Build / flash / test

### 6.1 Determine the physical P4 silicon revision first
Preferably from the bootloader serial log (`ESP32-P4 … revision vX.Y`), or
`esptool.py --port <port> chip_id` (subcommand is `chip_id`, with an underscore).
Revision `< v3` → use the `tuni-p4` variant (rev flags on); rev≥3 "P4X" silicon → `tuni-p4-p4x`.

### 6.2 Authoritative build (applies `config.json` `sdkconfig_append` + board config + rev flags)
`release.py` requires the board as a positional arg; with **no** arg it only packages the current
build (`scripts/release.py:419`), it does not compile `tuni-p4`.
```bash
. ~/esp/esp-idf/export.sh
cd /Users/tung/xiaozhi-esp32
python scripts/release.py tuni-p4 --name tuni-p4        # rev < v3
# or, for rev≥3 silicon:
python scripts/release.py tuni-p4 --name tuni-p4-p4x
```

### 6.3 Iterative dev build (equivalent manual config — `idf.py` does not read `config.json`)
```bash
idf.py set-target esp32p4
idf.py menuconfig    # set, to match the chosen variant:
#   Board Type → Tuni P4                         (CONFIG_BOARD_TYPE_TUNI_P4)
#   CONFIG_SLAVE_IDF_TARGET_ESP32C6=y
#   CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y
#   CONFIG_ESP_HOSTED_SDIO_4_BIT_BUS=y
#   CONFIG_USE_AUDIO_PROCESSOR=y
#   (rev < v3 only) CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y, CONFIG_ESP32P4_REV_MIN_100=y
idf.py erase-flash   # clear stale NVS (protocol version / token) before first bring-up
idf.py build flash monitor          # port via: ls /dev/cu.usbmodem*
```

### 6.4 Bring-up checks (in order)
1. Boots, serial log clean, C6 WiFi associates (BOOT while starting → config AP if no creds).
2. `POST /xiaozhi/ota` returns `200` with non-empty `websocket.url`, `websocket.token`, `version:1`.
3. WS upgrades; `hello` exchanged (server replies PCM 24k 40 ms); firmware logs `version: 1`.
4. Device auto-enters listening after connect (no press); **speak** → transcript in backend log →
   TTS audio plays back; after it stops, the device auto-returns to listening (continuous open-mic loop).

## 7. Risks & open questions

- **Physical P4 revision (blocking variant choice):** must be read off the actual chip (§6.1) before
  the first flash.
- **C6 WiFi host config:** exact `ESP_HOSTED_*` keys must match the NANO's C6-over-SDIO wiring —
  cross-check against the building `esp32-p4-nano` / `esp-p4-function-ev-board` variants.
- **Onboard ES8311 levels:** mic gain / PA volume may need tuning for usable STT; verify VAD triggers.
- **Token refresh:** expired session token requires reboot until OTA-refresh-on-401 exists (§5.5).
- **No display:** activation-code / state UI is suppressed (`NoDisplay`); rely on serial logs.

## 8. Next milestones (handoff order, each its own spec → plan)

1. **Display:** ST7701S 480×480 via **RGB + SPI init** (not DSI) + GT911 touch (I2C) + LVGL face.
2. **Far-field audio:** dual ICS-43434 I2S mics + **port ESP-SR AFE to P4** (beamforming + AEC).
3. **Stereo out:** dual MAX98357A I2S amps → two ear speakers.
4. **Motion:** 3× MG90S servos + the hard rule *servo still while listening*.
5. **Status LED:** WS2812B chest LED state colors (listen/think/speak/error/charge).
6. **Power:** MAX17043 fuel gauge battery thresholds (20/10/5%).
7. **Production auth:** on-device ECDSA P-256 device-JWT, replacing the dev-bypass (migration in §5.6).
8. **UX:** chest-button barge-in (interrupt mid-reply — requires real device+backend abort) and
   sleep/wake affordances. (Always-on open-mic VAD itself is already in Milestone 1, §4.2.)
