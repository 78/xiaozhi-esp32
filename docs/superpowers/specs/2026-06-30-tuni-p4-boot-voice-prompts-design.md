# Tuni P4 — Boot/Connection Voice Prompts (Design)

**Date:** 2026-06-30
**Board:** tuni-p4 (Waveshare ESP32-P4-NANO) — the only board that pins `vi-VN`
**Scope of change:** the **`vi-VN` locale** (see Locale scope below)
**Status:** Design — approved, pending spec review → plan

## Motivation

When a child (5–8) powers on Tuni, the device spends a few seconds finding WiFi,
connecting, checking version, and opening the WebSocket before it can talk. Today it
plays the **stock xiaozhi Vietnamese lines** (generic *"Đang quét Wi-Fi"*, *"Đang kết
nối"*, then a success chime). They work, but they aren't in Tuni's voice or
personality, and the WiFi→ready window is partly silent.

This feature replaces those moments with **custom Tuni-voiced Vietnamese lines** so the
robot cat feels alive from power-on, using the **same Chirp voice clone** the backend
uses for conversation — so the boot voice is identical to the voice the child hears when
talking to Tuni.

## Scope

In scope — six prompts:

| Asset | Phase | Today | Change |
|---|---|---|---|
| `wifi_scanning.ogg` | finding WiFi (every boot, first) | generic | asset swap |
| `wifi_connecting.ogg` | connecting to WiFi | generic | asset swap |
| `ready.ogg` (**new**) | ready to talk | success **chime** | new asset + small code change |
| `wificonfig.ogg` | no WiFi saved / setup (AP mode) | generic | asset swap |
| `disconnected.ogg` | connection dropped mid-session | generic | asset swap |
| `reconnected.ogg` | reconnected | generic | asset swap |

Out of scope: other locales (en-US etc. keep stock), wake-word/activation-code digit
sounds, OTA-upgrade line, any backend/protocol change.

### Locale scope (not strictly board-scoped)

The locale layer is selected **purely by `CONFIG_LANGUAGE_VI_VN`** — `CMakeLists.txt:857`
maps it to `vi-VN`, `:930` globs that locale's `*.ogg`. There is **no board-specific
locale layer**. So replacing `main/assets/locales/vi-VN/*.ogg` (and the
`#ifdef CONFIG_LANGUAGE_VI_VN` ready-line guard) makes **every `vi-VN` build on any board**
play the Tuni-branded prompts — this is a **`vi-VN`-locale change**, not a tuni-p4-only one.
That is acceptable because **tuni-p4 is currently the only board that pins `vi-VN`**, so in
practice the two are identical; the lines literally say "Tuni" and that's the intended
product.

**Alternative considered and rejected (YAGNI):** keep the stock `vi-VN` filenames untouched
and add tuni-prefixed prompts (`tuni_wifi_scanning.ogg`, …) selected behind
`CONFIG_BOARD_TYPE_TUNI_P4 && CONFIG_LANGUAGE_VI_VN`. This would truly isolate tuni from a
hypothetical future non-tuni vi-VN product, but turns five **zero-code asset swaps** into
board-`#ifdef`'d edits at five shared call sites (`application.cc:136,148,347,361`,
`wifi_board.cc:175`) plus new `OGG_TUNI_*` constants — real complexity for theoretical
isolation during single-product bring-up. If a second vi-VN product ever ships, revisit
then (introduce a board prompt layer); the asset filenames make that a clean follow-up.

## Draft script (final wording set during spec review)

Tuni speaks of itself in the third person ("Tuni"), addresses the child as "bạn",
warm and playful. The "ready" line is intentionally **general** (not English-specific)
because the activity isn't always an English lesson.

| Asset | Vietnamese |
|---|---|
| `wifi_scanning.ogg` | "Tuni đang tìm wifi nè, bạn chờ Tuni một chút xíu nha!" |
| `wifi_connecting.ogg` | "Tuni đang kết nối nè, sắp xong rồi, đợi Tuni xíu nha!" |
| `ready.ogg` | "A, Tuni sẵn sàng rồi! Hôm nay mình cùng chơi gì nào?" |
| `wificonfig.ogg` | "Tuni chưa có wifi. Bạn nhờ ba mẹ cài wifi cho Tuni nha!" |
| `disconnected.ogg` | "Ơ, Tuni bị rớt mạng rồi. Tuni thử kết nối lại nha!" |
| `reconnected.ogg` | "A, Tuni kết nối lại được rồi! Mình chơi tiếp nha!" |

Keep `ready.ogg` short (~2–3 s) regardless — see the auto-listen gating note.

## Voice generation

The production voice is **not** a named voice — it's a **Chirp 3 Instant Custom Voice
clone**, keyed by `clone_key_vi.txt` in the `robo-bridge` repo. The named
`vi-VN-Chirp3-HD-Achernar` that the worker sends is overridden by the clone key in prod.
To get the *identical* voice offline we must synthesize with that clone key.

**Pipeline (offline, run on the dev mac):**

```
vi text
  → Google Cloud TTS (voice_clone = clone_key_vi.txt, languageCode=vi-VN)   # raw audio
  → wav (LINEAR16, mono)
  → ffmpeg: loudnorm I=-16 LUFS, libopus, -b:a 16k, mono, -ar 16000, -frame_duration 60
  → <name>.ogg  →  main/assets/locales/vi-VN/
```

**Auth:** Google ADC via the dev's `gcloud` login + a quota project (the same auth
`robo-bridge/genkey.go` uses: `gcloud auth print-access-token` + `GCP_QUOTA_PROJECT`).
No Cloud Run / bridge OIDC token needed — we call Google Cloud TTS directly with the
clone key, bypassing the bridge.

**Implementation risk to resolve first (small spike in the plan):** confirm whether the
clone voice is reachable via the **unary** `text:synthesize` REST call or only via
**streaming** synthesis. `robo-bridge/main.go` uses `StreamingSynthesize` (gRPC). If
unary doesn't accept a `voice_clone` key for Chirp 3 ICV, the generator mirrors the
bridge's streaming call instead (collect raw audio chunks → wav). Either way the voice
and downstream encode are identical.

### Target audio format (match existing embedded prompts exactly)

Existing prompts probe as: `Opus, 48000 Hz (Opus-internal), mono, ~15 kb/s, libopus`,
produced by the xiaozhi converter with `acodec=libopus, audio_bitrate=16k, ac=1,
ar=16000, frame_duration=60` plus optional loudnorm to −16 LUFS. New files use the same
encode — including explicit **`-b:a 16k`** (the converter's `audio_bitrate=16k`; omit it
and libopus picks its own bitrate, diverging from the ~15 kb/s embedded prompts) — so
framing/loudness match what the firmware's `OggDemuxer` + Opus decoder already play
successfully. (Opus always advertises 48 kHz in the Ogg header; `-ar 16000` only sets the
pre-encode resample target.)

### Generation script (reusable)

`scripts/tuni_prompts/` (Python):

- `prompts.vi-VN.json` — manifest: `{ "<output_name>.ogg": "<vietnamese text>" }`.
- `gen_prompts.py` — for each entry: synthesize via Google Cloud TTS with the clone key,
  write a temp wav, run the ffmpeg encode, output `<name>.ogg` into
  `main/assets/locales/vi-VN/`. Idempotent: re-running regenerates all lines after a
  wording edit. Reads the clone key path from an arg/env (default
  `../robo-bridge/clone_key_vi.txt`).
- `README.md` — prerequisites (`gcloud` login, quota project, `ffmpeg`) and usage.

Keep **all six lines short (≤ ~2.5 s)** — defense-in-depth against boot-audio backlog
(see firmware-changes 4) and appropriate for young children's attention.

This keeps the lines reproducible and lets non-engineers (or future-you) tweak wording by
editing the manifest and re-running, rather than hand-converting audio.

## Firmware changes

Five of six prompts already play via existing wiring → **pure asset swap, no code
change**:

- `wifi_scanning.ogg` — `application.cc:136` (NetworkEvent::Scanning)
- `wifi_connecting.ogg` — `application.cc:148` (NetworkEvent::Connecting)
- `disconnected.ogg` — `application.cc:361`
- `reconnected.ogg` — `application.cc:347`
- `wificonfig.ogg` — `wifi_board.cc:175` (config-mode Alert)

The `gen_lang.py` + CMake glob auto-discovers any `*.ogg` in the locale dir and embeds it,
so dropping replacement files in and rebuilding is sufficient for these five.

**Ready line — the one code change** (`application.cc` ~387–396, `HandleActivationDoneEvent`):

1. **Add `ready.ogg` to `vi-VN/` only, and guard the reference.** `gen_lang.py` globs
   `*.ogg` (`gen_lang.py:47-51,109-150`) and emits `OGG_READY` only for locales where
   `ready.ogg` is present (current locale or en-US base). Rather than add an en-US fallback —
   which would make *every* non-vi board play an English spoken ready line instead of the
   stock success chime (an out-of-scope behavior change) — wrap the ready-line code in
   `#ifdef CONFIG_LANGUAGE_VI_VN`. vi-VN builds use `OGG_READY`; all other languages keep
   `OGG_SUCCESS`. `OGG_READY` is then only *referenced* in vi-VN builds, where `ready.ogg`
   (and thus the constant) always exists. tuni-p4 pins `CONFIG_LANGUAGE_VI_VN=y`, so the
   spoken ready line is active there. This keeps the "other locales keep stock" scope true.
2. **Force header regeneration when sounds change.** The CMake rule that runs `gen_lang.py`
   lists `DEPENDS ${LANG_JSON} gen_lang.py` only (`CMakeLists.txt:1030-1032`) — it does
   **not** depend on the `.ogg` files, and the checked-in `lang_config.h` currently has no
   `OGG_READY`. So merely dropping `ready.ogg` in will NOT regenerate the header on an
   incremental build → the `OGG_READY` reference fails to compile. The plan adds the locale +
   common sound files to the custom-command `DEPENDS` (durable fix) and forces a one-time
   header regen (or clean `lang_header` build) so `OGG_READY` exists before compile.
3. Inside the `#ifdef CONFIG_LANGUAGE_VI_VN` guard, play `OGG_READY` instead of `OGG_SUCCESS`
   at line 389 (`#else` keeps `OGG_SUCCESS`). `OGG_SUCCESS` is otherwise used **only** here,
   so non-vi behavior is byte-identical to today; `common/success.ogg` is left intact.
4. **Flush stale boot audio before the ready line (best-effort).** `PlaySound` only *appends*
   to the decode queue (`audio_service.cc:794-814`) with no replace policy, so on a fast
   WiFi/WebSocket path the scanning + connecting lines could still be queued/playing when
   "ready" fires — backlogging audio and delaying auto-listen by the *total* queued duration.
   At the ready step: **flush** pending boot audio (`ResetDecoder()`), **then**
   `PlaySound(OGG_READY)`, **then** wait. Note `ResetDecoder()` is *best-effort*, not a hard
   flush: `OpusCodecTask` decodes outside `audio_queue_mutex_` and could enqueue one
   already-popped packet *after* the clear (`audio_service.cc:399,463,829`), and a chunk
   already inside `codec_->OutputData()` (`:367`) can't be aborted — so a single stale frame
   could leak just before `ready.ogg`. In practice the scanning/connecting lines (triggered
   seconds earlier, during WiFi connect + version checks + the blocking server-hello) finish
   decoding well before the ready step, so the queues are normally already empty and the
   flush is cheap insurance. Acceptable as a brief cosmetic artifact; **if** hardware testing
   shows an objectionable stale frame, escalate to a decode/playback generation token
   (mirroring the upstream `upstream_generation_` pattern) rather than the queue clear.
5. **Precise "playback finished" wait, then auto-listen.** Today the success sound and
   `ToggleChatState()` (under `CONFIG_AUTO_START_LISTENING`) are scheduled back-to-back, and
   entering Listening calls `ResetDecoder()` which clears the queues. `WaitForPlaybackQueueEmpty()`
   is *not* a true finished signal: `AudioOutputTask` pops a chunk (`audio_service.cc:356-357`)
   *before* `codec_->OutputData()` completes (`:367`), so the queue can read empty
   (`:822-826`) while the final chunk is still being written → the tail clips. The plan uses
   an **in-flight-playback completion signal** (flag set before `OutputData`, cleared after;
   wait on it) — or, if proven unnecessary by ear, `WaitForPlaybackQueueEmpty()` + a short
   fixed settle (~150 ms) — gating `ToggleChatState()` until the ready line truly finishes.
   This runs once at boot on the main loop, so a brief block is acceptable.

**Pin the locale** (`main/boards/tuni-p4/config.json`): add `CONFIG_LANGUAGE_VI_VN=y` to
`sdkconfig_append` for **both** `tuni-p4` and `tuni-p4-p4x` builds. Currently the live
`sdkconfig` has it set, but it's not pinned in `config.json` — a `release.py` build
re-applies `config.json` and would otherwise fall back to the Kconfig default
(`LANGUAGE_ZH_CN`), silently reverting all prompts to Chinese.

## Component boundaries

- **Content generation** (`scripts/tuni_prompts/`) — standalone, no firmware dependency.
  Input: text manifest + clone key. Output: `.ogg` files. Testable by ear / `ffprobe`.
- **Asset embedding** (CMake/`gen_lang.py`) — embeds the new `.ogg` via the existing glob,
  but needs a one-line `DEPENDS` addition (locale + common sound files) so the generated
  header rebuilds when sounds change (firmware-changes 2). Otherwise unchanged.
- **Boot wiring** (existing `application.cc` network-event + activation paths) — unchanged
  except the ready-line block, guarded by `#ifdef CONFIG_LANGUAGE_VI_VN` (flush → play
  `OGG_READY` → finished-wait → auto-listen; `#else` keeps `OGG_SUCCESS`).

## Error handling / edge cases

- **Ready-line clipping** — handled by flushing stale boot audio before the ready line and
  a precise playback-finished wait before auto-listen (firmware-changes 4–5).
- **Boot-prompt backlog on a fast path** — flush-before-ready bounds the worst-case
  pre-listen delay to one line; short line durations bound it further.
- **Connecting→ready gap** — the connecting line wording bridges the short server-connect
  window; no separate prompt (avoids two lines stepping on each other). If the gap feels
  long in testing, revisit.
- **`release.py` reverting language** — handled by pinning `CONFIG_LANGUAGE_VI_VN=y`.
- **Non-vi builds** — the ready-line code is `#ifdef CONFIG_LANGUAGE_VI_VN`, so non-vi
  builds keep `OGG_SUCCESS` and are byte-identical to today; `ready.ogg` lives only in vi-VN,
  so no other locale's behavior or build changes.
- **Stale generated header** — adding `.ogg` files alone won't regenerate `lang_config.h`
  (CMake `DEPENDS` omits sound files); the plan fixes the `DEPENDS` and forces a regen.
- **Backpressure / VAD-gated streaming** — boot prompts play before listening starts and
  are independent of the upstream path; no interaction with the pending VAD-gating work.

## Testing / acceptance (hardware)

1. **Cold boot (known WiFi):** hear Tuni — finding-wifi → connecting → ready, in Tuni's
   voice, ready line **not clipped**, then auto-listen starts.
2. **Setup mode:** clear WiFi creds / hold BOOT during Starting → hear the `wificonfig`
   line.
3. **Drop/reconnect:** kill the AP mid-session → hear `disconnected`, restore → hear
   `reconnected`.
4. **Voice match:** boot voice is audibly the same as conversational Tuni.
5. **Build durability:** confirm the locale pin survives by inspecting the **generated
   `build/config/sdkconfig`** for `CONFIG_LANGUAGE_VI_VN=y` after building — not the release
   zip. (`release.py` *skips* a variant whose `releases/v*_tuni-p4.zip` already exists
   (`release.py:344-346`), so a stale zip would false-pass; delete it / clean build first.)

## Key files

- Assets: `main/assets/locales/vi-VN/{wifi_scanning,wifi_connecting,ready,wificonfig,disconnected,reconnected}.ogg` (vi-VN only)
- Wiring: `main/application.cc` (ready line ~389; others unchanged), `main/boards/common/wifi_board.cc:175`
- Audio: `main/audio/audio_service.{h,cc}` (`PlaySound`, `WaitForPlaybackQueueEmpty`/in-flight signal, `IsIdle`, `ResetDecoder`, `AudioOutputTask`)
- Build: `main/boards/tuni-p4/config.json`, `scripts/gen_lang.py`, `main/CMakeLists.txt:1024-1034` (DEPENDS)
- Generator (new): `scripts/tuni_prompts/{gen_prompts.py,prompts.vi-VN.json,README.md}`
- Voice source: `robo-bridge/clone_key_vi.txt`, `robo-bridge/genkey.go` (auth pattern), `robo-bridge/main.go` (streaming synth reference)
