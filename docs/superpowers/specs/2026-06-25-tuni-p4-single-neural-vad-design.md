# Tuni P4 — Single Neural VAD (consolidate device-side VAD) — Design

**Date:** 2026-06-25
**Status:** Approved approach; spec under review
**Scope:** Firmware-only (`xiaozhi-esp32`, board `tuni-p4`). No backend changes.
**Related:** [Milestone-1 voice-loop design](2026-06-24-tuni-p4-milestone1-voice-loop-design.md)

## Problem

Turn-ending (end-of-speech → `listen.stop`) on `tuni-p4` is currently driven by **two device-side
detectors running at once**, which race:

1. **Neural VAD (ESP-SR VADNet1-medium)** — `afe_audio_processor.cc:156-164` emits a
   `VAD_SPEECH→VAD_SILENCE` edge → `application.cc:309-318` sends `listen.stop`.
2. **Custom RMS energy detector** — `audio_service.cc:272-287` fires `on_silence_detected` after
   sustained low energy → `application.cc:118-142` flushes Opus and sends `listen.stop`.

Whichever fires first wins via the `listen_stop_sent_` guard. This is fragile, hard to reason about,
and the RMS thresholds (`kRmsSpeechThreshold=8500`, `kRmsSilenceThreshold=7200`,
`audio_service.cc:255-257`) were hand-tuned for a **different board's microphone** (the XH-S3E-AI
purple PCB, baseline ~6000 RMS), **not** the NANO's onboard ES8311. On this board those thresholds are
uncalibrated, so end-of-speech behavior is unpredictable.

There is also a third VAD in the system — Google Chirp's server-side VAD in `robo-bridge` — but it is
deliberately **not used** to end turns (the Worker takes "no action" on `speech_begin/end`, and Chirp's
`VoiceActivityTimeout` auto-endpointing is unarmed). It depends on Google, is hard to tune, and was
turned off by choice. Turn-ending authority therefore **stays on the device**.

## Goal

One device-side VAD, cleanly. Specifically:

- **Single source of truth for end-of-speech: the neural VAD (VADNet1-medium).**
- Remove the RMS energy detector's decision path entirely (no more racing, no mis-calibrated thresholds).
- Tune the neural VAD so end-of-speech is **reliable** and **tolerant of a child's mid-sentence pauses**.

### Non-goals

- No backend (`robo-worker` / `robo-bridge`) changes.
- No new VAD model / no swap to Silero/TEN VAD/esp-dl. (See "Model choice" — there is no better
  drop-in option for this device, and an off-ESP-SR model is a multi-week integration not justified now.)
- No change to the half-duplex / AutoStop protocol, wake-word policy, or auto-start behavior.

## Model choice (why VADNet1-medium, not "something better")

Investigated against the actual component (`espressif__esp-sr`, on 2.3.1; latest 2.4.2):

- **VADNet1-medium is the only neural VAD ESP-SR ships** — no small/large/v2 variant, and **no VAD model
  change between 2.3.1 → 2.4.2**. The only other in-framework option is WebRTC VAD, which is weaker
  (classic signal-based GMM). So VADNet1-medium is already the best in-toolchain choice; upgrading
  ESP-SR buys nothing for VAD.
- VADNet1-medium is a modern neural VAD (released Feb 2025 w/ ESP-SR v2.0, trained on ~15k hours:
  5k zh + 5k en + 5k multilingual), ~288 KB, recommended on ESP32-P4. The P4 already runs it in
  `AFE_MODE_HIGH_PERF` with headroom.
- Genuinely-better models (Silero, TEN VAD) exist but are ONNX/PyTorch-lineage — running one on the P4
  means a separate inference runtime outside the ESP-SR AFE pipeline (losing integrated AEC/NS) plus
  significant tuning. Out of scope; revisit only if VADNet1-medium proves unfixable on this mic.
- The P4 SoC hardware VAD peripheral is a low-power *wake* detector, too coarse for conversational
  turn-taking. Not a fit.

**Conclusion: the fix is configuration, not a new model.** The board currently runs `VAD_MODE_0`
(Normal — the *most permissive*, slowest to declare silence) with a 100 ms silence window — almost
certainly why end-of-speech was unreliable and the RMS hack was added.

## Design / changes

### 1. Remove the RMS energy decision path

- **`main/audio/audio_service.h`** — delete `on_silence_detected` from `AudioServiceCallbacks`
  (lines 94-97). Keep `on_vad_change`, `IsVoiceDetected()`, `voice_detected_` (these are fed by the
  **neural** VAD via `on_vad_change`, `audio_service.cc:125-130`, and consumed by the LED code —
  `single_led.cc:144`, `gpio_led.cc:230`).
- **`main/audio/audio_service.cc`** — remove the silence-trigger block (`audio_service.cc:272-287`:
  the `kRms*` thresholds, `had_loud_in_turn_`, `silent_chunks_`, and the `on_silence_detected()` fire).
  **Keep a lean diagnostic RMS log line** (`MIC peak/rms/chunk_rms`, currently `audio_service.cc:289`)
  — it drives no decision, but it is the instrument we use to verify/tune the neural VAD on serial.
  Explicitly comment that it is diagnostics-only.
- **`main/application.cc`** — delete the `callbacks.on_silence_detected = [...]` lambda
  (lines 118-143). The `MAIN_EVENT_VAD_CHANGE` end-of-speech path (`application.cc:309-318`) and its
  per-turn reset (`application.cc:982-985`) are unchanged and become the **only** turn-ender.

No other consumers of `on_silence_detected` exist (verified by grep).

### 2. Tune the neural VAD (`main/audio/processors/afe_audio_processor.cc:42-43`)

ESP-SR AFE VAD knobs (semantics from `esp_afe_config.h:109-113`, `esp_vad.h:31-36`):

| Knob | Current | New (starting) | Why |
|------|---------|---------------|-----|
| `vad_mode` | `VAD_MODE_0` (Normal) | `VAD_MODE_1` (Aggressive) | MODE_0 is the most permissive / slowest to declare silence. Bump one step so it reliably reports `VAD_SILENCE`; tune up to MODE_2/3 on serial if needed. |
| `vad_min_noise_ms` | `100` | `700` | **End-of-speech hangover.** Min silence before declaring end-of-turn. 100 ms ends a turn on any breath; ~700 ms tolerates a child's mid-sentence pause. The single most important knob. |
| `vad_min_speech_ms` | unset (default 128) | `128` (explicit) | Min sustained speech before declaring onset; suppresses noise-triggered false starts. Set explicitly for clarity; tune up if false-starts appear. |

These are **starting points to be confirmed on hardware**, not final values. All three are AFE-level —
no app-level debounce is added; the hangover lives in `vad_min_noise_ms`.

### 3. Serial verification & tuning (required step, not optional)

Tuning is empirical and needs the serial console (`idf.py monitor`). Prerequisite: a working USB
data connection enumerating `/dev/cu.usbmodem*` (the port that was not appearing during bring-up —
getting serial up is on the critical path for this work).

Procedure:
1. Flash the tuned build; open serial monitor.
2. Speak short kid-like phrases ("Hello", "My name is …") with natural pauses, then go quiet.
3. Observe on serial:
   - `MIC peak=… rms=… chunk_rms=…` — confirms the mic's real speech vs. silence energy on ES8311.
   - `VAD end-of-speech, sending listen.stop` — fires once per turn, ~`vad_min_noise_ms` after you stop.
4. Tune for two acceptance properties:
   - **Reliable:** every spoken turn ends (a `listen.stop` fires); the VAD is never stuck in speech.
   - **Pause-tolerant:** a mid-sentence pause of up to ~`vad_min_noise_ms` does **not** prematurely end
     the turn.
   Adjust `vad_mode` (reliability) and `vad_min_noise_ms` (pause tolerance) and re-flash until both hold.

## Risks & contingency

- **VADNet1-medium stuck in speech on the ES8311 (never reports `VAD_SILENCE`)** — the original failure
  mode on the other board. Escalation order before considering any model change:
  1. Raise `vad_mode` (1 → 2 → 3).
  2. Check/adjust ES8311 mic ADC gain (a hot mic / DC bias can make the noise floor look like speech).
  3. Enable neural noise suppression (NSNet models `nsnet1`/`nsnet2` are bundled) to clean the VAD input.
  - Only if all fail do we revisit a heavier model (separate spec). **RMS-only is *not* re-introduced**
    as a parallel path; if it were ever needed it would be the *sole* detector, recalibrated for ES8311
    — but that is explicitly the non-preferred outcome.
- **No serial access** — we can ship the conservative defaults above blind, but end-of-speech quality is
  unverified. Strongly prefer getting serial up first.
- **Hangover vs. latency tradeoff** — a larger `vad_min_noise_ms` is more pause-tolerant but adds that
  much delay before the robot responds. ~700 ms is the starting balance for ages 5–8; tune to taste.

## Acceptance criteria

- Only one device-side VAD path remains in the code (RMS decision path deleted; no `on_silence_detected`).
- Builds green for the `tuni-p4` variant.
- On hardware: spoken turns reliably end, mid-sentence pauses (≤ hangover) don't cut the child off, and
  the voice loop (speak → transcript → reply → auto-return to listening) runs repeatedly without the two
  detectors fighting.

## Out of scope

- Backend turn-detection / Chirp VAD changes.
- New/alternative VAD models (Silero, TEN VAD, esp-dl, P4 HW VAD).
- Full-duplex / server AEC, wake word, display, or other Milestone-≥2 items.
