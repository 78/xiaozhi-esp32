# Tuni P4 — VAD-gated audio streaming — Design

**Date:** 2026-06-25
**Status:** Approved approach; spec revised after three review rounds; under review
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
silence, no spurious TTS. Real utterances unchanged; all other boards/configs unchanged.

### Non-goals

No backend/protocol change; no VAD model/tuning change; no behavior change for any board other than
`tuni-p4` or for device-AEC builds. WS/WiFi stability is a separate thread.

## Why no backend change is needed

The worker's STT stream is driven entirely by **audio frames** + **`listen.stop`**: `case "listen"` acts
only on `state === "stop"` (→ `sendSTTEos`, `tutor.ts:2995`); Chirp is fed only when an audio frame
arrives (`tutor.ts:2965,300`); `listen.start` is a no-op for STT. So if the device stops sending audio
during silence, `utteranceState` returns to `idle` on its own.

## Background: the upstream path

Upstream is **raw int16 PCM** at `UPSTREAM_SAMPLE_RATE` (24 kHz), 40 ms frames (`audio_service.cc:474` —
`kAudioFormatPcm`; **not** Opus). Flow: `AfeAudioProcessor` (`OnOutput` emits `vector<int16_t>`,
`audio_processor.h:20`) → `AudioService::PushTaskToEncodeQueue` → `audio_encode_queue_` → `OpusCodecTask`
(pops under `audio_queue_mutex_`, **unlocks**, builds an `AudioStreamPacket`, re-locks to push,
`audio_service.cc:467-490`) → `audio_send_queue_` → main task's `MAIN_EVENT_SEND_AUDIO` drain →
`protocol_->SendAudio` (`application.cc:249`). Eos is `SendStopListening()` from the
`MAIN_EVENT_VAD_CHANGE` handler (`application.cc:284`). The `MAIN_EVENT_VAD_CHANGE` bit is an event-group
bit that **coalesces**; `vad_speaking_` holds only the latest state.

## Design

### 1. Opt-in gate + a "gating active?" query

- New `CONFIG_VAD_GATED_UPSTREAM` (bool, default `n`, `main/Kconfig.projbuild`), enabled **only** in
  `main/boards/tuni-p4/config.json` (both variants). `AfeAudioProcessor` is shared, so the gating code is
  `#ifdef`-compiled out on every other board.
- `AfeAudioProcessor` records `vad_enabled_` (set in its constructor by the same condition as `vad_init`
  — `false` under `CONFIG_USE_DEVICE_AEC`, `afe_audio_processor.cc:59-65`).
- **Gating is active iff `CONFIG_VAD_GATED_UPSTREAM` is set AND `vad_enabled_`.** No listening-mode check:
  `tuni-p4` only ever uses **AutoStop** (no wake word; AEC off → `GetDefaultListeningMode()` AutoStop,
  `application.cc:1036`; BOOT = WiFi-config only), so a mode guard is unnecessary. (Documented assumption:
  if a future `tuni-p4` feature introduces Manual/Realtime, a mode guard must be added here.) When VAD is
  disabled (device-AEC) the gate is off and the upstream streams continuously — fail-safe, never silently
  drop audio.
- Expose state so the shared `Application` can branch its eos logic: add
  `virtual bool IsUpstreamGatingActive() const { return false; }` to `AudioProcessor`
  (`audio_processor.h`); `NoAudioProcessor` inherits the `false` default; `AfeAudioProcessor` overrides to
  return `vad_enabled_` under `#ifdef CONFIG_VAD_GATED_UPSTREAM` (else `false`). Add an
  `AudioService::IsUpstreamGatingActive()` wrapper.

### 2. Gate the upstream on VAD speech, with pre-roll

In `AfeAudioProcessor::AudioProcessorTask`, when gating is active, drive `output_callback_` per VAD state:

| VAD edge / state | Action |
|------------------|--------|
| **silence → speech** | Emit `res->vad_cache` (pre-roll; `vad_cache_size` bytes = `vad_cache_size/2` int16 samples) through the framing path **first**, then the frame. If `vad_cache_size == 0`, skip pre-roll. |
| **speaking** (incl. hangover) | Emit PCM frames as today. |
| **speech → silence** | **Clear `output_buffer_` (discard the partial sub-frame tail) BEFORE** signalling the edge / enqueuing eos — otherwise residual samples (AFE fetch size and the 40 ms frame need not align) leak into the next utterance, *after* the marker. Then trigger the eos marker (#3). |
| **silent** | Emit nothing. |

The AFE holds `vad_state == SPEECH` through the `vad_min_noise_ms` (~672 ms) hangover, so trailing words
stream and emission stops where the turn ends. Input is still `Feed()`-ed continuously (VAD keeps
running); only the **send** path is gated. The input-side `MIC peak/rms` log is unaffected.

### 3. Serialize `listen.stop` after the final PCM frame — explicit control item

**Requirement:** every gated utterance is `audio* → listen.stop` on the wire, never `listen.stop → audio`.

1. New task type **`kAudioTaskTypeEndOfUtterance`** (`AudioTaskType` enum, `audio_service.h:97`).
2. On **speech → silence** (in `AudioService`'s `OnVadStateChange` handler, `audio_service.cc:125`), **only
   when gating is active**, enqueue a `kAudioTaskTypeEndOfUtterance` task into `audio_encode_queue_` —
   after the last PCM task (all prior speech PCM is already queued). FIFO preserved.
3. `OpusCodecTask`, on `kAudioTaskTypeEndOfUtterance`, pushes a **marker `AudioStreamPacket`**
   (new `bool end_of_utterance`, empty payload) onto `audio_send_queue_`, behind the PCM.
4. The main drain (`application.cc:254`), on a packet with `end_of_utterance == true`, calls
   `SendStopListening()` and **discards** it — **never** `SendAudio()` (no empty binary frame).
   **Guards for the marker path: AutoStop mode + listening state + `!listen_stop_sent_` only — NOT
   `vad_had_speech_in_turn_`.** That raw-edge guard is unreliable here (the coalesced `MAIN_EVENT_VAD_CHANGE`
   can drop a short true→false cycle's observed `true`), and the marker's existence already proves a
   speech→silence transition produced audio.

**Preserve raw-edge eos when gating is inactive:** the `MAIN_EVENT_VAD_CHANGE` handler
(`application.cc:278-288`) keeps its current `vad_had_speech_in_turn_`-based `listen.stop` **only when
`!audio_service_.IsUpstreamGatingActive()`** — unchanged for every other board / AEC build. When gating
is active the raw edge sends no eos (the marker path does); no double-stop.

### 4. Cross-task safety: generation tagging + race-free reset

Two task boundaries can resurrect stale state across a stop; both are closed with a single
**`upstream_generation_`** counter in `AudioService` (a `uint32_t`, mutated under `audio_queue_mutex_`):

- **Stale encode/send items (review #2).** `EnableVoiceProcessing(false)` / mid-utterance stop increments
  `upstream_generation_` and clears `audio_encode_queue_` + `audio_send_queue_`, all under
  `audio_queue_mutex_`. `AudioTask` and the marker `AudioStreamPacket` each carry the `generation` they
  were created with. `OpusCodecTask`, **under the same lock before pushing**, discards a task whose
  generation ≠ current (so the pop→unlock→build→push window cannot resurrect a stale PCM frame or eos
  marker); `AudioService::PopPacketFromSendQueue` likewise discards stale-generation packets and returns
  only current ones, so `Application` needs no generation awareness.
- **Rapid Stop→Start in the processor (review #4).** `AfeAudioProcessor::Stop()` sets
  `std::atomic<bool> reset_pending_` (it does **not** touch task-owned `is_speaking_`/`output_buffer_`).
  `AudioProcessorTask` checks/consumes `reset_pending_` **after every `fetch_with_delay`** (not only at the
  `PROCESSOR_RUNNING` wait): if set, it resets `is_speaking_ = false`, clears `output_buffer_`, and
  **discards that fetch result** — so a fetch that returns after `Start()` has re-set the running bit still
  honours the reset.
- **Callback-independent VAD state (review #3 prior round).** `is_speaking_` is updated from `vad_state`
  regardless of `vad_state_change_callback_` (moved out of the `if (vad_state_change_callback_)` guard at
  `:156`; the callback still fires on edges).

## Files touched

- `main/Kconfig.projbuild` — `CONFIG_VAD_GATED_UPSTREAM`.
- `main/boards/tuni-p4/config.json` — enable it (both variants).
- `main/protocols/protocol.h` — `AudioStreamPacket`: `bool end_of_utterance = false;` + `uint32_t generation = 0;` (internal, not serialized).
- `main/audio/audio_processor.h` — `virtual bool IsUpstreamGatingActive() const { return false; }`.
- `main/audio/processors/afe_audio_processor.{h,cc}` — `vad_enabled_`, `IsUpstreamGatingActive` override,
  gating + `vad_cache` pre-roll + silence-edge `output_buffer_` clear, `reset_pending_` (checked after
  every fetch), callback-independent `is_speaking_`.
- `main/audio/processors/no_audio_processor.{h,cc}` — inherits the `false` default (listed for completeness).
- `main/audio/audio_service.{h,cc}` — `kAudioTaskTypeEndOfUtterance`; `AudioTask.generation` +
  `upstream_generation_`; enqueue the marker on the gated VAD→silence edge; `OpusCodecTask` generation
  check + marker emission; generation bump + queue clear on stop; `PopPacketFromSendQueue` stale discard;
  `IsUpstreamGatingActive()` wrapper.
- `main/application.cc` — raw-edge `listen.stop` only when `!IsUpstreamGatingActive()`; send-drain emits
  `listen.stop` (+ discard) on the `end_of_utterance` marker with the marker-path guards.

## Edge cases

- **VAD false-trigger on a transient** → short utterance → empty Chirp final → backend speaks the soft
  `pickNoSpeech` line, not the tech apology. Rare, acceptable.
- **Speech→silence with empty PCM pipeline** → the marker task is still enqueued → `listen.stop` still fires.
- **AFE provides no `vad_cache`** → skip pre-roll; verify onset still acceptable.
- **Gating inactive** (other board / device-AEC) → no markers; raw-edge eos as today; continuous streaming.

## Testing / acceptance (hardware-in-the-loop; serial + `wrangler tail`)

1. **Idle several minutes:** no `sendAudio`/`speech_begin` on the backend; **zero "gặp trục trặc"** apologies.
2. **Ordering:** every utterance is `audio* → listen.stop` on the wire — never `listen.stop → audio`; no late
   audio until the next VAD onset; the marker is never sent as an (empty) audio frame; no residual partial
   frame bleeds into the next utterance.
3. **First word not clipped:** transcript is the full word (e.g. `"apple"`, not `"pple"`); turn ends, reply
   plays, device auto-returns to listening.
4. **Stop/restart while actively speaking** (reply interrupts mid-speech), incl. rapid cycles: next session
   gets its onset/pre-roll and sends its eos; no stuck state; no stale audio/eos or partial frame leaks
   (generation discard + reset verified).
5. **Fail-safe:** with gating off (config off, or device-AEC → `vad_enabled_` false), upstream streams
   continuously **and turns still end via the raw-edge `listen.stop`** exactly as before.

## Out of scope

Backend apology-gating, `robo-bridge` Chirp resiliency, WS/WiFi stability, VAD model/tuning.
