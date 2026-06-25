# Tuni P4 — Single Neural VAD Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the ESP-SR neural VAD (VADNet1-medium) the single device-side end-of-speech authority on the `tuni-p4` board, removing the racing custom RMS energy detector.

**Architecture:** Two changes to firmware: (1) tune the AFE VAD config so VADNet1-medium reliably reports end-of-speech with a child-pause-tolerant hangover; (2) delete the RMS energy-detector decision path so only the neural VAD drives `listen.stop`. Then flash and tune the VAD parameters empirically on serial.

**Tech Stack:** ESP-IDF (C++), Espressif ESP-SR 2.3.1 AFE / VADNet1-medium, ESP32-P4 (`tuni-p4` variant, rev v1.3), onboard ES8311 codec.

**Spec:** `docs/superpowers/specs/2026-06-25-tuni-p4-single-neural-vad-design.md`

## Global Constraints

- **Firmware-only.** No changes to `robo-worker` or `robo-bridge`.
- **Board/variant:** `tuni-p4` (NOT `tuni-p4-p4x`; device is ESP32-P4 rev v1.3).
- **No host unit tests exist** for the AFE/audio path. The per-code-task test cycle is **`python scripts/release.py tuni-p4 --name tuni-p4` builds green** (or incremental `idf.py build` once the board sdkconfig is in place). Behavioral verification is **on-device over serial** (Task 3).
- **Keep** `on_vad_change`, `IsVoiceDetected()` / `voice_detected_` (fed by the neural VAD, consumed by LED code). **Remove** only `on_silence_detected` and the RMS decision logic.
- **Single tuning surface** = three AFE knobs (`vad_mode`, `vad_min_noise_ms`, `vad_min_speech_ms`). No app-level debounce; the end-of-speech hangover lives in `vad_min_noise_ms`.
- **Flashing gotcha:** native USB-Serial-JTAG drops on reset. Flash with esptool `write_flash @flash_args -b 230400` from `build/`, NOT `idf.py flash`/460800. If the port vanishes, physically replug. Port e.g. `/dev/cu.usbmodem*` (name varies).

---

### Task 1: Tune the neural VAD config

Set the three VAD knobs to their starting values. With this change the neural VAD is tuned but the RMS detector still exists (removed in Task 2); at no intermediate built state is end-of-speech worse than today.

**Files:**
- Modify: `main/audio/processors/afe_audio_processor.cc:42-43`

**Interfaces:**
- Consumes: ESP-SR `afe_config_t` fields `vad_mode` (`vad_mode_t`), `vad_min_speech_ms` (int), `vad_min_noise_ms` (int) — from `esp_afe_config.h:110-113`.
- Produces: a tuned AFE VAD whose `VAD_SPEECH→VAD_SILENCE` edge (already wired at `afe_audio_processor.cc:156-164` → `application.cc:309-318`) fires ~`vad_min_noise_ms` after the user stops talking.

- [ ] **Step 1: Apply the config change**

In `main/audio/processors/afe_audio_processor.cc`, replace:

```cpp
    afe_config->vad_mode = VAD_MODE_0;
    afe_config->vad_min_noise_ms = 100;
```

with:

```cpp
    // Single device-side VAD (vadnet1-medium). These three knobs are the ONLY
    // end-of-speech tuning surface (no app-level debounce):
    //   vad_mode          MODE_0 was too permissive (slow/unreliable to report
    //                     VAD_SILENCE); MODE_1 reliably declares silence.
    //   vad_min_noise_ms  end-of-speech hangover — min silence before ending the
    //                     turn, so a child's mid-sentence pause doesn't cut them off.
    //   vad_min_speech_ms min sustained speech before onset — suppresses noise
    //                     false-starts.
    // Starting points — tune empirically on serial (see plan Task 3).
    afe_config->vad_mode = VAD_MODE_1;
    afe_config->vad_min_speech_ms = 128;
    afe_config->vad_min_noise_ms = 700;
```

- [ ] **Step 2: Build and verify green**

Run: `python scripts/release.py tuni-p4 --name tuni-p4`
Expected: build completes without error and produces the `tuni-p4` artifact. (First build sets the board sdkconfig; later tasks may use incremental `idf.py build`.)

- [ ] **Step 3: Confirm the tuned values compiled in**

Run: `grep -n "vad_mode\|vad_min_speech_ms\|vad_min_noise_ms" main/audio/processors/afe_audio_processor.cc`
Expected: shows `VAD_MODE_1`, `vad_min_speech_ms = 128`, `vad_min_noise_ms = 700`.

- [ ] **Step 4: Commit**

```bash
git add main/audio/processors/afe_audio_processor.cc
git commit -m "feat(tuni-p4): tune neural VAD (mode/hangover) as starting points

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Remove the RMS energy decision path

Delete the second VAD entirely so only the neural VAD ends turns. Keep a diagnostics-only mic level meter (no decision) for serial tuning, and keep `IsVoiceDetected` (neural-VAD-fed, used by LED).

**Files:**
- Modify: `main/audio/audio_service.h:94-97` (remove `on_silence_detected` member + comment)
- Modify: `main/application.cc:113-143` (remove the `on_silence_detected` lambda)
- Modify: `main/audio/audio_service.cc:245-299` (replace RMS decision block with diagnostics-only meter)

**Interfaces:**
- Consumes: nothing new.
- Produces: an `AudioServiceCallbacks` with no `on_silence_detected`; `ReadAudioData` logs `MIC peak=… rms=…` every ~30 input chunks and drives no VAD decision.

- [ ] **Step 1: Remove `on_silence_detected` from the callbacks struct**

In `main/audio/audio_service.h`, delete these four lines (currently 94-97):

```cpp
    // Custom RMS-based silence detector for noisy mics where AFE/VADNet
    // can't reliably detect end-of-speech. Fired once per turn after the mic
    // has been loud then quiet for a sustained period.
    std::function<void(void)> on_silence_detected;
```

(Leave `on_vad_change` and `on_audio_testing_queue_full` above it intact, and the closing `};`.)

- [ ] **Step 2: Remove the `on_silence_detected` lambda in application.cc**

In `main/application.cc`, delete the whole block (currently lines 113-143) that begins with the comment `// RMS-based silence fallback …` and the `callbacks.on_silence_detected = [this]() { … };` lambda, up to and including its closing `};`. The line immediately after, `audio_service_.SetCallbacks(callbacks);`, stays.

- [ ] **Step 3: Replace the RMS decision block with a diagnostics-only meter**

In `main/audio/audio_service.cc`, replace the entire RMS block (the brace block that starts with the comment `// RMS-based mic activity tracker.` near line 239 and ends at the matching `}` near line 299) with:

```cpp
    // Diagnostics-only mic level meter (drives NO decision). End-of-speech is
    // owned solely by the neural VAD (vadnet1-medium); this log is the
    // instrument for verifying/tuning that VAD on serial — see the single-VAD
    // design spec. Logs peak + RMS over ~30 input chunks.
    {
        static uint32_t mic_dbg_calls = 0;
        static int32_t mic_dbg_peak = 0;
        static uint64_t mic_dbg_sqsum = 0;
        static uint32_t mic_dbg_n = 0;
        for (auto s : data) {
            int32_t a = s < 0 ? -static_cast<int32_t>(s) : s;
            if (a > mic_dbg_peak) mic_dbg_peak = a;
            mic_dbg_sqsum += static_cast<uint64_t>(s) * static_cast<uint64_t>(s);
        }
        mic_dbg_n += data.size();
        mic_dbg_calls++;
        if (mic_dbg_calls >= 30) {
            uint32_t rms = mic_dbg_n > 0 ? static_cast<uint32_t>(__builtin_sqrt(mic_dbg_sqsum / mic_dbg_n)) : 0;
            ESP_LOGI(TAG, "MIC peak=%ld rms=%lu", (long)mic_dbg_peak, (unsigned long)rms);
            mic_dbg_calls = 0;
            mic_dbg_peak = 0;
            mic_dbg_sqsum = 0;
            mic_dbg_n = 0;
        }
    }
```

- [ ] **Step 4: Verify no `on_silence_detected` references remain**

Run: `grep -rn "on_silence_detected\|kRmsSpeechThreshold\|had_loud_in_turn_\|silent_chunks_" main`
Expected: no matches.

- [ ] **Step 5: Build and verify green**

Run: `python scripts/release.py tuni-p4 --name tuni-p4` (or `idf.py build`)
Expected: build completes without error.

- [ ] **Step 6: Commit**

```bash
git add main/audio/audio_service.h main/application.cc main/audio/audio_service.cc
git commit -m "refactor(tuni-p4): remove RMS VAD; neural VAD is sole end-of-speech authority

Keep mic level meter as diagnostics-only (no decision). IsVoiceDetected stays
(neural-VAD-fed, used by LED).

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Flash, verify, and tune on hardware

Empirically confirm the single neural VAD ends turns reliably and tolerates a child's pauses, then lock in the final tuned values. This task is hardware-in-the-loop; its deliverable is verified behavior + a commit of the final values.

**Files:**
- Modify (only if tuning requires): `main/audio/processors/afe_audio_processor.cc:42-44`

**Interfaces:**
- Consumes: the build from Task 2.
- Produces: confirmed end-of-speech behavior; final `vad_mode` / `vad_min_noise_ms` / `vad_min_speech_ms` values committed.

- [ ] **Step 1: Ensure a serial port enumerates**

Run: `ls /dev/cu.usbmodem*`
Expected: a port appears (e.g. `/dev/cu.usbmodem*`).
If absent: replug the USB-C cable / try the board's other USB-C port / use a known **data** cable. Serial is required for this task.

- [ ] **Step 2: Flash the Task-2 build**

From `build/`: `esptool.py --chip esp32p4 -p <PORT> -b 230400 write_flash @flash_args`
Expected: write completes (replug and retry if the port drops mid-flash).

- [ ] **Step 3: Open the serial monitor**

Run: `idf.py -p <PORT> monitor` (Ctrl-] to exit)
Expected: boot logs, then the device connects (it's provisioned) and enters the always-on listening loop.

- [ ] **Step 4: Exercise the VAD and observe**

Speak short, kid-like phrases ("Hello", "My name is Tom") with a natural mid-sentence pause, then go quiet. In the serial log, observe:
- `MIC peak=… rms=…` — the real ES8311 speech-vs-silence energy.
- `VAD end-of-speech, sending listen.stop` — fires **once per turn**, ~700 ms after you stop.

Expected (acceptance):
- **Reliable:** every spoken turn produces exactly one `listen.stop`; the VAD is never stuck in speech (no turn that never ends).
- **Pause-tolerant:** a mid-sentence pause up to ~700 ms does NOT prematurely end the turn.

- [ ] **Step 5: Tune if Step 4 fails, then re-flash**

Adjust `main/audio/processors/afe_audio_processor.cc` and repeat Steps 2-4 (`idf.py build` for fast incremental rebuilds):
- Turn never ends / stuck in speech → raise `vad_mode` (`VAD_MODE_1` → `VAD_MODE_2` → `VAD_MODE_3`).
- Cuts off mid-sentence → raise `vad_min_noise_ms` (`700` → `900`).
- Too slow to reply → lower `vad_min_noise_ms` (`700` → `500`).
- False starts on background noise → raise `vad_min_speech_ms` (`128` → `200`).

Contingency — if the turn never ends at ANY `vad_mode` (VADNet1 stuck reporting speech on this mic):
1. Inspect `MIC rms` while silent vs. speaking — a high silent-baseline RMS indicates a hot mic.
2. Lower the ES8311 mic ADC gain in the codec init for this board, re-flash, retest.
3. If still stuck, enable neural noise suppression (NSNet `nsnet1`/`nsnet2` are bundled) to clean the VAD input.
Do NOT re-introduce the RMS detector as a parallel path. (A recalibrated RMS as the *sole* detector is the documented last resort only — see spec Risks.)

- [ ] **Step 6: Commit the final tuned values (if changed from Task 1)**

```bash
git add main/audio/processors/afe_audio_processor.cc
git commit -m "tune(tuni-p4): final VAD params verified on ES8311 hardware

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

(If Step 4 passed with the Task-1 starting values unchanged, skip this commit and note that the defaults verified as-is.)

---

## Done when

- Only one device-side VAD path remains (no `on_silence_detected`, no RMS decision logic).
- `tuni-p4` builds green.
- On hardware: spoken turns reliably end, mid-sentence pauses (≤ hangover) don't cut the child off, and the voice loop (speak → transcript → reply → auto-return to listening) runs repeatedly without two detectors fighting.
