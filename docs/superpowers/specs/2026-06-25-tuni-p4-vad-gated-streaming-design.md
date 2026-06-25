# Tuni P4 — VAD-gated audio streaming — Design

**Date:** 2026-06-25
**Status:** Approved approach; spec revised after team review; under review
**Scope:** Firmware-only (`xiaozhi-esp32`). Behavior change is **opt-in via Kconfig, enabled only for `tuni-p4`**. No backend (`robo-worker` / `robo-bridge`) changes.
**Builds on:** [Single Neural VAD design](2026-06-25-tuni-p4-single-neural-vad-design.md).

## Problem

The robot intermittently speaks the tech-error apology *"…gặp trục trặc tí rồi…"* when nobody is
speaking. Root-cause (code + `wrangler tail`): it is **not** the VAD. The apology is the backend
`onSTTError` path (`robo-worker` `tutor.ts:1751`), fired by transient STT-pipeline hiccups —
`bridge_error` (Chirp gRPC `code = Canceled`) and `ws_closed` (`code=1006`). It only fires when the
worker thinks an utterance is in flight (`stt.ts:194`), and `utteranceState` flips to `"speaking"` on the
**first audio frame** (`stt.ts:90`). Because the device streams **every** frame continuously (silence and
noise included), the worker is perpetually mid-utterance, so any hiccup during a quiet period is misread
as a failed turn → apology. Two costs: Chirp STT is billed for transcribing silence; each false apology
is a wasted TTS call.

## Goal

**Stream audio to the backend only while the VAD reports speech** (opt-in, `tuni-p4` only). During
silence the worker stays `idle`, so pipeline hiccups are silently reconnected (no apology), Chirp is not
billed for silence, and no spurious TTS fires. Real utterances are unchanged.

### Non-goals

- No backend / protocol changes. `listen.start`/`listen.stop` semantics are unchanged.
- No change to the VAD model/tuning (`vad_mode=1`, `vad_min_noise_ms≈672`, `vad_min_speech_ms=128`).
- No behavior change for any board other than `tuni-p4`, or for non-AutoStop modes, or for device-AEC
  builds. The device WS/WiFi instability is a separate thread.

## Why no backend change is needed

The worker's STT stream is driven entirely by **audio frames** + **`listen.stop`**: `case "listen"` acts
only on `state === "stop"` (→ `sendSTTEos`, `tutor.ts:2995`); Chirp is fed only when an audio frame
arrives (`tutor.ts:2965,300`); `listen.start` is a no-op for STT. So if the device stops sending audio
during silence, `utteranceState` returns to `idle` on its own.

## Design

Upstream audio is **raw int16 PCM** at `UPSTREAM_SAMPLE_RATE` (24 kHz), 40 ms frames
(`audio_service.cc:466,474` — `kAudioFormatPcm`; **not** Opus). The pipeline is:
AFE output → `audio_encode_queue_` → `OpusCodecTask` (packs PCM) → `audio_send_queue_` → drained by the
main task's `MAIN_EVENT_SEND_AUDIO` handler → `protocol_->SendAudio` (`application.cc:249`).

### 1. Opt-in gate (addresses review #2)

New `CONFIG_VAD_GATED_UPSTREAM` (bool, default `n`, in `main/Kconfig.projbuild`), enabled **only** in
`main/boards/tuni-p4/config.json` `sdkconfig_append`. `AfeAudioProcessor` is shared by all boards, so the
gating code is compiled out elsewhere.

Gating is active **only when all hold**, else the upstream streams continuously as today (fail-safe — we
never silently drop audio):
- `CONFIG_VAD_GATED_UPSTREAM` is set, **and**
- the AFE VAD is actually running. Under `CONFIG_USE_DEVICE_AEC` the AFE sets `vad_init=false`
  (`afe_audio_processor.cc:61`) → `is_speaking_` would never go true. `AfeAudioProcessor` records a
  `vad_enabled_` member (same `#ifdef` as the config) and **refuses to gate** when VAD is disabled, **and**
- the listening mode is **AutoStop**. The application drives a runtime `EnableUpstreamGating(bool)` on the
  processor from `SetListeningMode` (on for AutoStop; off for Manual/Realtime, which stream continuously
  for server-side AEC/VAD). `tuni-p4` is always AutoStop, so it is always on there.

### 2. Gate the upstream on VAD speech, with pre-roll (addresses review #4 terminology)

In `AfeAudioProcessor::AudioProcessorTask` (`afe_audio_processor.cc:135-187`), when gating is active,
invoke `output_callback_` (which feeds the PCM send pipeline) per the VAD state:

| VAD edge / state | Action |
|------------------|--------|
| **silence → speech** | Emit `res->vad_cache` (pre-roll; `vad_cache_size` bytes = `vad_cache_size/2` int16 samples) through the framing path **first**, then the current frame — first word not clipped. If `vad_cache_size == 0`, skip pre-roll. |
| **speaking** (incl. hangover) | Emit PCM frames as today. |
| **speech → silence** | Stop emitting; **enqueue an end-of-utterance marker** (see #3); clear `output_buffer_`. |
| **silent** | Emit nothing. |

The AFE holds `vad_state == SPEECH` through the `vad_min_noise_ms` (~672 ms) hangover, so trailing words
are streamed and emission stops exactly where the turn ends. Input is still `Feed()`-ed continuously so
the VAD keeps running; only the **send** path is gated. The input-side `MIC peak/rms` diagnostic log is
unaffected.

### 3. Serialize `listen.stop` behind the final PCM frame (addresses review #1 — the important one)

**Requirement:** for every utterance the wire order MUST be `audio* → listen.stop`, never
`listen.stop → audio`. Today `SendStopListening()` fires from the `MAIN_EVENT_VAD_CHANGE` handler
(`application.cc:284`) **independently** of the async PCM pipeline, so frames still in
`audio_encode_queue_`/`audio_send_queue_` when the edge fires are transmitted **after** the eos. With
continuous streaming this is harmless (the late frame is just more stream); with gating it strands an
isolated post-eos frame that opens a fresh backend utterance with no following eos — re-creating the
stuck-utterance failure mode.

**Fix:** carry the end-of-utterance as an **in-band marker through the same FIFO as the PCM** so eos is
emitted only after the last frame. Concretely: add an `end_of_utterance` flag to `AudioStreamPacket`
(`protocol.h`). On the speech→silence edge the gating path tags the **last** emitted frame (or, if the
pipeline is empty at the edge, enqueues a zero-payload marker packet). The main task's send-drain
(`application.cc:249-259`), upon sending a packet whose `end_of_utterance` is set, then calls
`SendStopListening()` (moving the AutoStop `listen.stop` trigger off the raw VAD edge and onto the drain).
The `listen_stop_sent_` / `vad_had_speech_in_turn_` guards move with it.

Consequence: **`AudioService`/`Application` are changed** (the earlier "unchanged" claim was wrong) —
`AudioStreamPacket` gains a flag, the gating path tags it, and the send-drain emits `listen.stop` after it.

### 4. Lifecycle reset & callback-independent VAD state (addresses review #3)

- `AfeAudioProcessor::Stop()` (`:113-121`) and `Start()` reset `is_speaking_ = false` and clear
  `output_buffer_`, so a Stop during speech can't leave the next session stuck mid-state (missing its
  onset/pre-roll or its eos).
- `is_speaking_` is updated from `vad_state` **regardless of whether `vad_state_change_callback_` is set**
  (move the state update out of the `if (vad_state_change_callback_)` guard at `:156`; still invoke the
  callback on edges). Gating reads `is_speaking_`, so it must track VAD state unconditionally.

## Files touched

- `main/Kconfig.projbuild` — add `CONFIG_VAD_GATED_UPSTREAM`.
- `main/boards/tuni-p4/config.json` — enable it (both variants).
- `main/protocols/protocol.h` — add `end_of_utterance` to `AudioStreamPacket`.
- `main/audio/processors/afe_audio_processor.{h,cc}` — `vad_enabled_`, `EnableUpstreamGating`, gating +
  pre-roll, marker enqueue, lifecycle reset, callback-independent `is_speaking_`.
- `main/audio/audio_service.{h,cc}` — propagate `end_of_utterance` through encode→send queues.
- `main/application.cc` — `EnableUpstreamGating` per `SetListeningMode`; emit `listen.stop` from the
  send-drain on the marker instead of the raw VAD edge.

## Edge cases

- **VAD false-trigger on a transient** → short utterance → empty Chirp final → backend speaks the soft
  `pickNoSpeech` line, not the tech apology. Rare, acceptable.
- **Silence edge with empty pipeline** → enqueue a zero-payload marker so `listen.stop` still fires.
- **AFE provides no `vad_cache`** → skip pre-roll; verify onset still acceptable.
- **VAD disabled / non-AutoStop / other board** → gating off, continuous streaming (unchanged).

## Testing / acceptance (hardware-in-the-loop; serial + `wrangler tail`)

1. **Idle (no speech) several minutes:** no `sendAudio`/`speech_begin` on the backend (no audio leaves the
   device) and **zero "gặp trục trặc" apologies**.
2. **Ordering:** every utterance shows `audio* → listen.stop` on the wire — **never** `listen.stop → audio`,
   and **no late audio** appears until the next VAD onset.
3. **First word not clipped:** transcript is the full word (e.g. `"apple"`, not `"pple"`); turn ends, reply
   plays, device auto-returns to listening.
4. **Stop/restart while actively speaking** (e.g. a reply interrupts mid-speech): the next listening session
   gets its onset/pre-roll and sends its eos correctly (no stuck state).
5. **Fail-safe:** with gating off (other board / Manual / Realtime / device-AEC), upstream streams
   continuously exactly as before.

## Out of scope

- Backend apology-gating, `robo-bridge` Chirp resiliency, WS/WiFi stability.
- VAD model/tuning changes.
