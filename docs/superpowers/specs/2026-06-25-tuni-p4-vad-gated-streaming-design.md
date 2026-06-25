# Tuni P4 — VAD-gated audio streaming — Design

**Date:** 2026-06-25
**Status:** Approved approach; spec revised after two review rounds; under review
**Scope:** Firmware-only (`xiaozhi-esp32`). Behavior change is **opt-in via Kconfig, enabled only for `tuni-p4`**. No backend (`robo-worker` / `robo-bridge`) changes.
**Builds on:** [Single Neural VAD design](2026-06-25-tuni-p4-single-neural-vad-design.md).

## Problem

The robot intermittently speaks the tech-error apology *"…gặp trục trặc tí rồi…"* when nobody is
speaking. Root-cause (code + `wrangler tail`): **not** the VAD. The apology is the backend `onSTTError`
path (`robo-worker` `tutor.ts:1751`), fired by transient STT-pipeline hiccups — `bridge_error` (Chirp
gRPC `code = Canceled`) and `ws_closed` (`code=1006`). It only fires when the worker thinks an utterance
is in flight (`stt.ts:194`), and `utteranceState` flips to `"speaking"` on the **first audio frame**
(`stt.ts:90`). Because the device streams **every** frame continuously (silence/noise included), the
worker is perpetually mid-utterance, so any hiccup in a quiet period is misread as a failed turn →
apology. Costs: Chirp STT billed for transcribing silence; each false apology is a wasted TTS call.

## Goal

**Stream audio to the backend only while the VAD reports speech** (opt-in, `tuni-p4` only). During silence
the worker stays `idle` → pipeline hiccups reconnect silently (no apology), Chirp isn't billed for
silence, no spurious TTS. Real utterances unchanged; all other boards/modes unchanged.

### Non-goals

No backend/protocol change; no VAD model/tuning change; no behavior change for any board other than
`tuni-p4`, for non-AutoStop modes, or for device-AEC builds. WS/WiFi stability is a separate thread.

## Why no backend change is needed

The worker's STT stream is driven entirely by **audio frames** + **`listen.stop`**: `case "listen"` acts
only on `state === "stop"` (→ `sendSTTEos`, `tutor.ts:2995`); Chirp is fed only when an audio frame
arrives (`tutor.ts:2965,300`); `listen.start` is a no-op for STT. So if the device stops sending audio
during silence, `utteranceState` returns to `idle` on its own.

## Background: the upstream path

Upstream audio is **raw int16 PCM** at `UPSTREAM_SAMPLE_RATE` (24 kHz), 40 ms frames
(`audio_service.cc:466,474` — `kAudioFormatPcm`; **not** Opus). Flow:
`AfeAudioProcessor` (`OnOutput` emits `vector<int16_t>`, `audio_processor.h:20`) →
`AudioService::PushTaskToEncodeQueue(kAudioTaskTypeEncodeToSendQueue, …)` → `audio_encode_queue_` →
`OpusCodecTask` builds an `AudioStreamPacket` → `audio_send_queue_` → drained by the main task's
`MAIN_EVENT_SEND_AUDIO` handler → `protocol_->SendAudio` (`application.cc:249`). `SendStopListening()`
(eos) is sent separately from the `MAIN_EVENT_VAD_CHANGE` handler (`application.cc:284`).

## Design

### 1. Opt-in gate + a "gating active?" query (review #2 prior round; review #4 this round)

- New `CONFIG_VAD_GATED_UPSTREAM` (bool, default `n`, `main/Kconfig.projbuild`), enabled **only** in
  `main/boards/tuni-p4/config.json` `sdkconfig_append` (both variants). `AfeAudioProcessor` is shared, so
  the gating code is `#ifdef`-compiled out on every other board.
- `AfeAudioProcessor` records `vad_enabled_` (set in its constructor by the same condition as
  `vad_init` — i.e. `false` under `CONFIG_USE_DEVICE_AEC`, `afe_audio_processor.cc:59-65`).
- **Gating is active iff `CONFIG_VAD_GATED_UPSTREAM` is set AND `vad_enabled_`.** When VAD is disabled
  (device-AEC), gating is **off** and the upstream streams continuously — fail-safe, never silently drop
  audio. Mode: `tuni-p4` is **always AutoStop** (no wake word; AEC off → `GetDefaultListeningMode()`
  returns AutoStop, `application.cc:1036`; BOOT = WiFi-config only), so no per-mode runtime control is
  needed; the AutoStop assumption is documented here and must be revisited if `tuni-p4` ever adds another
  mode.
- Expose the state so the shared `Application` can branch its EOS logic without reaching into the
  processor: add `virtual bool IsUpstreamGatingActive() const { return false; }` to `AudioProcessor`
  (`audio_processor.h`); `NoAudioProcessor` inherits the `false` default; `AfeAudioProcessor` overrides it
  to return `vad_enabled_` under `#ifdef CONFIG_VAD_GATED_UPSTREAM` (else `false`). Add an
  `AudioService::IsUpstreamGatingActive()` wrapper forwarding to `audio_processor_`.

### 2. Gate the upstream on VAD speech, with pre-roll

In `AfeAudioProcessor::AudioProcessorTask` (`afe_audio_processor.cc:135-187`), when gating is active,
invoke `output_callback_` (which feeds the PCM encode→send pipeline) per VAD state:

| VAD edge / state | Action |
|------------------|--------|
| **silence → speech** | Emit `res->vad_cache` (pre-roll; `vad_cache_size` bytes = `vad_cache_size/2` int16 samples) through the framing path **first**, then the frame — first word not clipped. If `vad_cache_size == 0`, skip pre-roll. |
| **speaking** (incl. hangover) | Emit PCM frames as today. |
| **speech → silence** | Stop emitting; trigger the end-of-utterance marker (see #3). |
| **silent** | Emit nothing. |

The AFE holds `vad_state == SPEECH` through the `vad_min_noise_ms` (~672 ms) hangover, so trailing words
stream and emission stops where the turn ends. Input is still `Feed()`-ed continuously (VAD keeps
running); only the **send** path is gated. The input-side `MIC peak/rms` diagnostic log is unaffected.

### 3. Serialize `listen.stop` after the final PCM frame — via an explicit control item (review #1 both rounds, #2 this round)

**Requirement:** for every gated utterance the wire order MUST be `audio* → listen.stop`, never
`listen.stop → audio` (a stray post-eos frame opens a fresh backend utterance with no eos — the original
failure mode).

The processor emits only `vector<int16_t>`; the `AudioStreamPacket` is built later — so the marker cannot
be a flag tagged onto the "last emitted frame" at the processor. Instead:

1. New task type **`kAudioTaskTypeEndOfUtterance`** (added to the `AudioTaskType` enum, `audio_service.h:97`).
2. On the **speech → silence** edge, `AudioService` (in its `OnVadStateChange` handler, which already runs
   when the processor reports the edge, `audio_service.cc:125`) — **only when gating is active** —
   enqueues a `kAudioTaskTypeEndOfUtterance` task into `audio_encode_queue_`, i.e. **after** the last PCM
   task (the AFE emits the VAD edge and that fetch's gated-off audio in the same iteration; all prior
   speech PCM is already queued). FIFO order is preserved.
3. `OpusCodecTask` (`audio_service.cc:466+`), on `kAudioTaskTypeEndOfUtterance`, pushes a **marker
   `AudioStreamPacket`** (new `bool end_of_utterance` field in `protocol.h`; empty payload) onto
   `audio_send_queue_`, preserving FIFO behind the PCM packets.
4. The main drain (`application.cc:254`), on popping a packet with `end_of_utterance == true`, calls
   `SendStopListening()` (with the existing AutoStop / `listen_stop_sent_` guards) and **discards** it —
   it is **never** passed to `SendAudio()` (no empty binary frame on the wire).

**Preserve raw-edge EOS when gating is inactive (review #1 this round):** the `MAIN_EVENT_VAD_CHANGE`
handler (`application.cc:278-288`) keeps sending `listen.stop` on the raw VAD edge **only when
`!audio_service_.IsUpstreamGatingActive()`** — unchanged behavior for every other board / mode / AEC
build. When gating is active, the raw edge does **not** send eos (the marker path does), so there is no
double-stop.

### 4. Lifecycle reset, race-free + callback-independent VAD state (review #3 both rounds)

`EnableVoiceProcessing(enable)` calls `processor->Start()/Stop()` (`audio_service.cc:654`) from the main
thread, while `is_speaking_` / `output_buffer_` are owned by `AudioProcessorTask` — clearing them directly
races. Therefore:

- `AfeAudioProcessor::Stop()` sets an `std::atomic<bool> reset_pending_` (does **not** touch
  `is_speaking_`/`output_buffer_` directly). `AudioProcessorTask`, on resuming after the
  `PROCESSOR_RUNNING` wait (`afe_audio_processor.cc:142`), consumes the flag and resets `is_speaking_ =
  false` and `output_buffer_.clear()` itself, before processing — so reset is task-owned and race-free.
- `is_speaking_` is updated from `vad_state` **regardless of `vad_state_change_callback_`** (move the
  state update out of the `if (vad_state_change_callback_)` guard at `:156`; still invoke the callback on
  edges). Gating reads `is_speaking_`, so it must track unconditionally.
- **Pending-item clearing on mid-utterance stop:** when voice processing is disabled mid-utterance
  (listening → speaking), `AudioService` clears the **upstream** `audio_encode_queue_` and
  `audio_send_queue_` (including any in-flight `kAudioTaskTypeEndOfUtterance` task / marker packet) so no
  stale PCM or eos leaks into the next listening session.

## Files touched

- `main/Kconfig.projbuild` — `CONFIG_VAD_GATED_UPSTREAM`.
- `main/boards/tuni-p4/config.json` — enable it (both variants).
- `main/protocols/protocol.h` — `AudioStreamPacket.end_of_utterance` (default `false`).
- `main/audio/audio_processor.h` — `virtual bool IsUpstreamGatingActive() const { return false; }`.
- `main/audio/processors/no_audio_processor.{h,cc}` — inherits the `false` default (no override needed;
  listed for completeness).
- `main/audio/processors/afe_audio_processor.{h,cc}` — `vad_enabled_`, `IsUpstreamGatingActive` override,
  gating + `vad_cache` pre-roll, `reset_pending_` lifecycle reset, callback-independent `is_speaking_`.
- `main/audio/audio_service.{h,cc}` — `kAudioTaskTypeEndOfUtterance`; enqueue it on the VAD→silence edge
  when gating active; `OpusCodecTask` emits the marker packet; `IsUpstreamGatingActive()` wrapper; clear
  upstream queues on mid-utterance stop.
- `main/application.cc` — raw-edge `listen.stop` only when `!IsUpstreamGatingActive()`; send-drain emits
  `listen.stop` (and discards) on the `end_of_utterance` marker.

## Edge cases

- **VAD false-trigger on a transient** → short utterance → empty Chirp final → backend speaks the soft
  `pickNoSpeech` line, not the tech apology. Rare, acceptable.
- **Speech→silence edge with empty PCM pipeline** → the `kAudioTaskTypeEndOfUtterance` task is still
  enqueued and produces a marker, so `listen.stop` still fires.
- **AFE provides no `vad_cache`** → skip pre-roll; verify onset still acceptable.
- **Gating inactive** (other board / device-AEC / `vad_enabled_` false) → no markers; raw-edge EOS as
  today; continuous streaming.

## Testing / acceptance (hardware-in-the-loop; serial + `wrangler tail`)

1. **Idle several minutes:** no `sendAudio`/`speech_begin` on the backend; **zero "gặp trục trặc"** apologies.
2. **Ordering:** every utterance shows `audio* → listen.stop` on the wire — never `listen.stop → audio`;
   no late audio until the next VAD onset; the marker is **never** sent as an (empty) audio frame.
3. **First word not clipped:** transcript is the full word (e.g. `"apple"`, not `"pple"`); turn ends, reply
   plays, device auto-returns to listening.
4. **Stop/restart while actively speaking** (reply interrupts mid-speech): next session gets its
   onset/pre-roll and sends its eos; no stuck state, no leaked stale audio/eos.
5. **Fail-safe / fallback:** with gating off (config off, or device-AEC, or a non-AutoStop mode), upstream
   streams continuously **and turns still end via the raw-edge `listen.stop`** exactly as before.

## Out of scope

Backend apology-gating, `robo-bridge` Chirp resiliency, WS/WiFi stability, VAD model/tuning.
