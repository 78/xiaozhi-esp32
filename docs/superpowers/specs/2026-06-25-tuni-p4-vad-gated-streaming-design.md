# Tuni P4 — VAD-gated audio streaming — Design

**Date:** 2026-06-25
**Status:** Approved approach; spec under review
**Scope:** Firmware-only (`xiaozhi-esp32`, board `tuni-p4`). No backend (`robo-worker` / `robo-bridge`) changes.
**Builds on:** [Single Neural VAD design](2026-06-25-tuni-p4-single-neural-vad-design.md) (the now-single VADNet1 VAD is what we gate on).

## Problem

The robot intermittently speaks the tech-error apology *"…gặp trục trặc tí rồi…"* when nobody is
speaking. Root-cause investigation (code + the `wrangler tail` logs) established it is **not** the VAD:

- The apology text is `pickTechError` → the backend `onSTTError` path (`robo-worker` `tutor.ts:1751`),
  fired by `bridge_error` (Chirp gRPC `code = Canceled`) and `ws_closed` (`code=1006`) — i.e. transient
  STT-pipeline hiccups, normal for a long-lived streaming connection.
- The apology only fires when the worker thinks an utterance is **in flight** (`stt.ts:194`,
  `utteranceState !== "idle"`), and `utteranceState` flips to `"speaking"` on the **first audio frame**
  (`stt.ts:90`), regardless of content.
- The device, by always-on open-mic design, **streams every audio frame continuously** — silence and
  noise included — so the worker is **perpetually "mid-utterance"**, and any pipeline hiccup during a
  quiet period is misread as a failed turn → apology.

Two consequences, both costing money: (1) Google Chirp STT is billed for transcribing **continuous
silence** whenever the device is on; (2) each false apology is a wasted **TTS** call.

## Goal

**Stream audio to the backend only while the VAD reports speech.** Silence/noise never leaves the device.

Result: during silence the worker stays `idle`, so STT-pipeline hiccups are silently reconnected (no
apology), Chirp is not billed for silence, and no spurious TTS fires. Real utterances are unchanged.

### Non-goals

- No backend / protocol changes. `listen.start`/`listen.stop` semantics are unchanged.
- No change to the VAD model or its tuning (`vad_mode=1`, `vad_min_noise_ms≈672`, `vad_min_speech_ms=128`).
- The device WS/WiFi instability (the observed ~534 s WS drop) is a **separate** thread, not addressed
  here (though gating makes a drop during true silence a no-op).

## Why no backend change is needed

The worker's STT stream is driven entirely by **audio frames** and **`listen.stop`**:
`case "listen"` acts only on `state === "stop"` (→ `sendSTTEos`, `tutor.ts:2995`), and Chirp is fed
purely when an audio frame arrives (`sendAudioFrom` → `chirp.sendAudio`, `tutor.ts:2965,300`).
`listen.start` is a **no-op** for STT. So if the device stops sending audio during silence, the worker's
`utteranceState` returns to `idle` on its own — no server code changes.

## Design

**Single change locus: `AfeAudioProcessor::AudioProcessorTask`** (`main/audio/processors/afe_audio_processor.cc:135-187`).

Today the task calls `output_callback_` (which feeds the Opus encoder → send queue) on **every** fetch.
The AFE already tracks speech state in `is_speaking_` (set on the `VAD_SPEECH`/`VAD_SILENCE` edges,
lines 157-163) and provides a pre-speech cache in the fetch result
(`afe_fetch_result_t.vad_cache` / `vad_cache_size`, `esp_afe_sr_iface.h:33-35` — "used to complete the
audio that was truncated").

New behavior, gated on `is_speaking_`:

| VAD edge / state | Action |
|------------------|--------|
| **silence → speech** | Emit `res->vad_cache` (pre-roll, `vad_cache_size` bytes = `vad_cache_size/2` int16 samples) through the output path **first**, then the current frame — so the first word is not clipped. |
| **speaking** (incl. hangover) | Emit frames via `output_callback_` as today. |
| **speech → silence** | Stop emitting. Clear `output_buffer_`. (`application.cc:284` already sends `listen.stop` on this same edge.) |
| **silent** | Emit nothing. |

**Why timing is automatically correct:** the AFE holds `vad_state == SPEECH` through the
`vad_min_noise_ms` (~672 ms) hangover, so trailing words are still streamed and emission stops at the
**exact edge** where `listen.stop` fires. No new timers or buffers are introduced beyond the existing
`output_buffer_` framing.

**Input path unchanged:** the AFE is still `Feed()`-ed continuously so the VAD keeps running; only the
**output/send** path is gated. The input-side `MIC peak/rms` diagnostic log (`audio_service.cc`) is
unaffected.

**Pre-roll handling:** `vad_cache` is raw `int16` PCM in the same format as `res->data`. It is emitted
through the same framing logic (`output_buffer_` → `output_callback_` in `frame_samples_` chunks) so the
encoder/send path needs no special case. If `vad_cache_size == 0` at the transition (AFE provided no
cache), skip the pre-roll — onset may be marginally tighter; this is a verification point, not a failure.

## Components & boundaries

- **`AfeAudioProcessor`** — owns the gating decision. Input: AFE fetch results (`vad_state`, `vad_cache`,
  `data`). Output: `output_callback_` invoked only during speech (+ pre-roll at onset). It already owns
  `is_speaking_`; this change keeps the speech-state logic in one place.
- **`AudioService` / `Application`** — unchanged. They consume `output_callback_` (encode→send) and the
  `on_vad_change` edge (→ `listen.stop`) exactly as before; they simply receive frames only during speech.

## Edge cases

- **VAD false-trigger on a transient (e.g. a bang):** a short utterance streams → Chirp returns empty →
  backend speaks the soft `pickNoSpeech` line ("chưa nghe rõ"), **not** the tech apology. Rare, acceptable.
- **Opus encoder across gaps:** each utterance is a fresh backend STT stream (bridge lazy-opens per
  utterance), so a brief encoder discontinuity at onset is harmless. Verify the first word transcribes
  intact.
- **AFE provides no `vad_cache`:** skip pre-roll (see above); verify onset is still acceptable.

## Testing / acceptance (hardware-in-the-loop)

Verify on the device with serial + `wrangler tail` running concurrently:

1. **Idle (no speech) for several minutes:** the backend tail shows **no `sendAudio` / no `speech_begin`**
   (no audio leaving the device) and **zero "gặp trục trặc" apologies**.
2. **Speaking:** the first word is **not clipped** — transcript is the full word (e.g. `"apple"`, not
   `"pple"`); the turn ends, the reply plays, and the device auto-returns to listening.
3. **Repeat** several speak/idle cycles to confirm stability (no false apologies between turns).

## Out of scope

- Backend apology-gating, Chirp stream resiliency (`robo-bridge`), and WS/WiFi connection stability.
- Any change to VAD model/tuning or the single-VAD consolidation (already done).
