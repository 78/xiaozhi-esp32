# Tuni P4 — VAD-gated audio streaming Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On `tuni-p4`, stream audio upstream only while the VAD reports speech (with `vad_cache` pre-roll and a strictly-ordered end-of-utterance marker), so silence/noise never reaches the backend — killing the false "gặp trục trặc" apologies and the wasted Chirp/TTS spend.

**Architecture:** Opt-in behind `CONFIG_VAD_GATED_UPSTREAM` (tuni-p4 only). `AfeAudioProcessor` gates its output on `is_speaking_`; an `end_of_utterance` control item rides the same FIFO as the PCM so `listen.stop` is always sent *after* the last frame; an `upstream_generation_` counter discards stale audio/markers across stop/restart. No backend or protocol changes.

**Tech Stack:** ESP-IDF v5.5.2 (C++), ESP-SR 2.3.1 AFE / VADNet1-medium, ESP32-P4 (`tuni-p4`, rev v1.3), ES8311.

**Spec:** `docs/superpowers/specs/2026-06-25-tuni-p4-vad-gated-streaming-design.md`

## Global Constraints

- **Firmware-only.** No `robo-worker` / `robo-bridge` changes. No protocol changes (`listen.start`/`listen.stop` semantics unchanged).
- **Opt-in:** all gating logic is under `#ifdef CONFIG_VAD_GATED_UPSTREAM`, enabled **only** for `tuni-p4` (both variants). Compiled out on every other board → zero behavior change elsewhere.
- **Gating is active iff** `CONFIG_VAD_GATED_UPSTREAM` **and** `vad_enabled_` (false under `CONFIG_USE_DEVICE_AEC`). No listening-mode check (tuni-p4 is always AutoStop).
- **Upstream is raw int16 PCM** at `UPSTREAM_SAMPLE_RATE` (24 kHz), 40 ms frames (`kAudioFormatPcm`). Not Opus.
- **No host unit tests exist** for the audio path. Per-code-task gate = **build green** for the `tuni-p4` variant. The config is enabled in Task 1, so every later build compiles the gating paths; **intermediate builds are not flashed**. Behavioral acceptance is the final hardware task (Task 6).
- **Build:** Task 1 (changes `config.json`) builds with `python scripts/release.py tuni-p4 --name tuni-p4` (applies `sdkconfig_append`). Later tasks may use incremental `idf.py build`. Source IDF first: `source /Users/tung/esp/esp-idf/export.sh`.
- **Flashing gotcha (Task 6):** esptool `write_flash @flash_args -b 230400` from `build/` (not `idf.py flash`/460800); port `/dev/cu.usbmodem*` (name varies), replug if it drops.
- **Wire-order invariant:** every gated utterance is `audio* → listen.stop`, never `listen.stop → audio`.

---

### Task 1: Kconfig flag + "gating active?" plumbing

**Files:**
- Modify: `main/Kconfig.projbuild`
- Modify: `main/boards/tuni-p4/config.json`
- Modify: `main/audio/audio_processor.h:20-21`
- Modify: `main/audio/processors/afe_audio_processor.h`
- Modify: `main/audio/processors/afe_audio_processor.cc:40-75`
- Modify: `main/audio/audio_service.h`

**Interfaces:**
- Produces: `AudioProcessor::IsUpstreamGatingActive() const → bool` (virtual, default `false`); `AfeAudioProcessor::vad_enabled_` (bool member); `AudioService::IsUpstreamGatingActive() const → bool`.

- [ ] **Step 1: Add the Kconfig option**

In `main/Kconfig.projbuild`, near the other board feature flags (e.g. after `CONFIG_AUTO_START_LISTENING`), add:

```kconfig
config VAD_GATED_UPSTREAM
    bool "Gate upstream audio on VAD speech (only send while speaking)"
    default n
    help
        When enabled, the device only streams audio to the server while the
        AFE VAD reports speech (with vad_cache pre-roll), instead of streaming
        continuously. Intended for always-on open-mic AutoStop boards with
        on-device VAD (no device AEC). Compiled out when unset.
```

- [ ] **Step 2: Enable it for tuni-p4 (both variants)**

In `main/boards/tuni-p4/config.json`, add `"CONFIG_VAD_GATED_UPSTREAM=y"` to the `sdkconfig_append` array of **both** builds (`tuni-p4` and `tuni-p4-p4x`), e.g. after `"CONFIG_AUTO_START_LISTENING=y",`.

- [ ] **Step 3: Add the virtual query to the AudioProcessor interface**

In `main/audio/audio_processor.h`, inside `class AudioProcessor`, after `virtual void EnableDeviceAec(bool enable) = 0;` add:

```cpp
    // True only when this processor is actively gating upstream audio on VAD
    // speech (CONFIG_VAD_GATED_UPSTREAM + VAD enabled). Default false so the
    // shared Application keeps its existing raw-edge listen.stop behavior.
    virtual bool IsUpstreamGatingActive() const { return false; }
```

(`NoAudioProcessor` inherits this default — no edit needed there.)

- [ ] **Step 4: Add `vad_enabled_` + the override to AfeAudioProcessor**

In `main/audio/processors/afe_audio_processor.h`, add a private member `bool vad_enabled_ = false;` and a public override:

```cpp
    bool IsUpstreamGatingActive() const override {
#ifdef CONFIG_VAD_GATED_UPSTREAM
        return vad_enabled_;
#else
        return false;
#endif
    }
```

In `main/audio/processors/afe_audio_processor.cc`, in the constructor where the AEC/VAD branch sets `vad_init` (`:59-65`), record the same condition:

```cpp
#ifdef CONFIG_USE_DEVICE_AEC
    afe_config->aec_init = true;
    afe_config->vad_init = false;
    vad_enabled_ = false;
#else
    afe_config->aec_init = false;
    afe_config->vad_init = true;
    vad_enabled_ = true;
#endif
```

- [ ] **Step 5: Add the AudioService wrapper**

In `main/audio/audio_service.h`, in `class AudioService` public section, add:

```cpp
    bool IsUpstreamGatingActive() const { return audio_processor_->IsUpstreamGatingActive(); }
```

- [ ] **Step 6: Build green**

Run: `source /Users/tung/esp/esp-idf/export.sh && python scripts/release.py tuni-p4 --name tuni-p4`
Expected: `Project build complete`. (Config now compiled in; nothing reads the query yet → no behavior change.)

- [ ] **Step 7: Commit**

```bash
git add main/Kconfig.projbuild main/boards/tuni-p4/config.json main/audio/audio_processor.h main/audio/processors/afe_audio_processor.h main/audio/processors/afe_audio_processor.cc main/audio/audio_service.h
git commit -m "feat(tuni-p4): add CONFIG_VAD_GATED_UPSTREAM + IsUpstreamGatingActive plumbing

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Generation tagging + end-of-utterance marker infrastructure

Build the stale-safe queue plumbing and the marker packet type. No marker is *enqueued* yet (Task 4), and with `upstream_generation_` starting at 0 and only bumping on stop, behavior is unchanged.

**Files:**
- Modify: `main/protocols/protocol.h:15-21`
- Modify: `main/audio/audio_service.h:89-111,180-185`
- Modify: `main/audio/audio_service.cc:108-123,189-202,466-499,554-574` + `PopPacketFromSendQueue`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: `AudioStreamPacket{bool end_of_utterance=false; uint32_t generation=0;}`; `AudioTaskType::kAudioTaskTypeEndOfUtterance`; `AudioTask::generation`; `std::atomic<uint32_t> AudioService::upstream_generation_`; `AudioService::PushTaskToEncodeQueue(AudioTaskType, std::vector<int16_t>&&, uint32_t generation)`; `AudioService::MarkEndOfUtterance(uint32_t generation)`; `AudioService::BumpUpstreamGeneration()`.

- [ ] **Step 1: Add fields to AudioStreamPacket**

In `main/protocols/protocol.h`, in `struct AudioStreamPacket` (`:15-21`), add (internal, not serialized):

```cpp
    bool end_of_utterance = false;   // marker: drain sends listen.stop, never SendAudio
    uint32_t generation = 0;         // upstream session id for stale-drop
```

- [ ] **Step 2: Add the task type + AudioTask.generation + the counter**

In `main/audio/audio_service.h`:
- In `enum AudioTaskType` (`:97-100`) add `kAudioTaskTypeEndOfUtterance,`.
- In `struct AudioTask` add `uint32_t generation = 0;`.
- In `class AudioService` private members add `#include <atomic>` (top) and `std::atomic<uint32_t> upstream_generation_{0};`.
- Change the declaration of `PushTaskToEncodeQueue` to:
  `void PushTaskToEncodeQueue(AudioTaskType type, std::vector<int16_t>&& pcm, uint32_t generation);`
- Add public/private decls:
  `void MarkEndOfUtterance(uint32_t generation);`
  `void BumpUpstreamGeneration();`

- [ ] **Step 3: Generation-validate in PushTaskToEncodeQueue**

In `main/audio/audio_service.cc`, replace `PushTaskToEncodeQueue` (`:554-574`) with:

```cpp
void AudioService::PushTaskToEncodeQueue(AudioTaskType type, std::vector<int16_t>&& pcm, uint32_t generation) {
    auto task = std::make_unique<AudioTask>();
    task->type = type;
    task->pcm = std::move(pcm);
    task->generation = generation;
    std::unique_lock<std::mutex> lock(audio_queue_mutex_);

    // Drop frames produced in a prior upstream session (a Stop bumped the
    // generation while this frame was still resampling). Prevents stale audio
    // from re-entering after a clear.
    if (type == kAudioTaskTypeEncodeToSendQueue &&
        generation != upstream_generation_.load(std::memory_order_relaxed)) {
        return;
    }

    if (type == kAudioTaskTypeEncodeToSendQueue && !timestamp_queue_.empty()) {
        if (timestamp_queue_.size() <= MAX_TIMESTAMPS_IN_QUEUE) {
            task->timestamp = timestamp_queue_.front();
        } else {
            ESP_LOGW(TAG, "Timestamp queue (%u) is full, dropping timestamp", timestamp_queue_.size());
        }
        timestamp_queue_.pop_front();
    }

    audio_queue_cv_.wait(lock, [this]() {
        return audio_encode_queue_.size() < MAX_ENCODE_TASKS_IN_QUEUE; });
    audio_encode_queue_.push_back(std::move(task));
    audio_queue_cv_.notify_all();
}
```

- [ ] **Step 4: Add MarkEndOfUtterance + BumpUpstreamGeneration**

In `main/audio/audio_service.cc`, add:

```cpp
void AudioService::MarkEndOfUtterance(uint32_t generation) {
    auto task = std::make_unique<AudioTask>();
    task->type = kAudioTaskTypeEndOfUtterance;
    task->generation = generation;
    std::unique_lock<std::mutex> lock(audio_queue_mutex_);
    if (generation != upstream_generation_.load(std::memory_order_relaxed)) {
        return;
    }
    audio_queue_cv_.wait(lock, [this]() {
        return audio_encode_queue_.size() < MAX_ENCODE_TASKS_IN_QUEUE; });
    audio_encode_queue_.push_back(std::move(task));
    audio_queue_cv_.notify_all();
}

void AudioService::BumpUpstreamGeneration() {
    // audio_encode_queue_ and audio_send_queue_ are both guarded by
    // audio_queue_mutex_, so one lock covers the whole bump+clear. Dropping the
    // old session here, plus the generation check before each push, means a
    // clear can't be undone by an in-flight encode.
    std::lock_guard<std::mutex> lock(audio_queue_mutex_);
    upstream_generation_.fetch_add(1, std::memory_order_relaxed);
    audio_encode_queue_.clear();
    audio_send_queue_.clear();
    audio_queue_cv_.notify_all();   // wake any producer blocked in the full-queue wait
}
```

- [ ] **Step 5: Update OnOutput to capture generation at entry**

In `main/audio/audio_service.cc`, the `OnOutput` lambda (`:108-123`): snapshot the generation at entry and pass it into both `PushTaskToEncodeQueue` calls:

```cpp
    audio_processor_->OnOutput([this](std::vector<int16_t>&& data) {
        uint32_t gen = upstream_generation_.load(std::memory_order_relaxed);
        if (upstream_resampler_ != nullptr) {
            uint32_t target_size = 0;
            esp_ae_rate_cvt_get_max_out_sample_num(upstream_resampler_, data.size(), &target_size);
            std::vector<int16_t> resampled(target_size);
            uint32_t actual_output = target_size;
            esp_ae_rate_cvt_process(upstream_resampler_, (esp_ae_sample_t)data.data(), data.size(),
                                    (esp_ae_sample_t)resampled.data(), &actual_output);
            resampled.resize(actual_output);
            PushTaskToEncodeQueue(kAudioTaskTypeEncodeToSendQueue, std::move(resampled), gen);
        } else {
            PushTaskToEncodeQueue(kAudioTaskTypeEncodeToSendQueue, std::move(data), gen);
        }
    });
```

Also update the other `PushTaskToEncodeQueue` caller (audio-testing path, `:309`) to pass `upstream_generation_.load(std::memory_order_relaxed)` as the third arg.

- [ ] **Step 6: Carry generation + handle the marker in OpusCodecTask**

In `main/audio/audio_service.cc`, in the send-queue packing block (`:466-499`): the existing code pops the task under the outer `lock`, then calls `lock.unlock()` (`:471`) before building the packet. **After that `lock.unlock()`** (so the `lock2` push below does not self-deadlock on `audio_queue_mutex_`), branch on the marker type first:

```cpp
            if (task->type == kAudioTaskTypeEndOfUtterance) {
                // Re-check generation under lock; only emit a marker for the
                // current session.
                if (task->generation == upstream_generation_.load(std::memory_order_relaxed)) {
                    auto marker = std::make_unique<AudioStreamPacket>();
                    marker->format = kAudioFormatPcm;
                    marker->end_of_utterance = true;
                    marker->generation = task->generation;
                    {
                        std::lock_guard<std::mutex> lock2(audio_queue_mutex_);
                        audio_send_queue_.push_back(std::move(marker));
                    }
                    if (callbacks_.on_send_queue_available) {
                        callbacks_.on_send_queue_available();   // wake the drain — eos must not sit queued
                    }
                }
                lock.lock();
                continue;
            }
```

- For the existing `kAudioTaskTypeEncodeToSendQueue` packet, set `packet->generation = task->generation;` and only push if still current:

```cpp
            packet->generation = task->generation;
            ...
            if (task->type == kAudioTaskTypeEncodeToSendQueue) {
                {
                    std::lock_guard<std::mutex> lock2(audio_queue_mutex_);
                    if (packet->generation == upstream_generation_.load(std::memory_order_relaxed)) {
                        audio_send_queue_.push_back(std::move(packet));
                    } else {
                        packet.reset();
                    }
                }
                if (callbacks_.on_send_queue_available) {
                    callbacks_.on_send_queue_available();
                }
            }
```

(Match the surrounding lock/unlock flow exactly; the generation check sits inside the same `lock2` that guards `audio_send_queue_`.)

- [ ] **Step 7: Stale-drop in PopPacketFromSendQueue**

In `main/audio/audio_service.cc`, in `PopPacketFromSendQueue`, skip stale-generation packets and return the next current one:

```cpp
std::unique_ptr<AudioStreamPacket> AudioService::PopPacketFromSendQueue() {
    std::lock_guard<std::mutex> lock(audio_queue_mutex_);
    while (!audio_send_queue_.empty()) {
        auto packet = std::move(audio_send_queue_.front());
        audio_send_queue_.pop_front();
        if (packet->generation != upstream_generation_.load(std::memory_order_relaxed)) {
            continue;   // drop stale; do not transmit
        }
        audio_queue_cv_.notify_all();
        return packet;
    }
    return nullptr;
}
```

(If the current `PopPacketFromSendQueue` differs, preserve its locking/notify but add the generation skip loop.)

- [ ] **Step 8: Build green**

Run: `source /Users/tung/esp/esp-idf/export.sh && idf.py build`
Expected: `Project build complete`. (Marker never enqueued yet; generation always current → unchanged behavior.)

- [ ] **Step 9: Commit**

```bash
git add main/protocols/protocol.h main/audio/audio_service.h main/audio/audio_service.cc
git commit -m "feat(tuni-p4): upstream generation tagging + end_of_utterance marker plumbing

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Gate the AFE output on VAD speech + vad_cache pre-roll

**Files:**
- Modify: `main/audio/processors/afe_audio_processor.h` (member)
- Modify: `main/audio/processors/afe_audio_processor.cc:135-187`

**Interfaces:**
- Consumes: `IsUpstreamGatingActive()` (Task 1).
- Produces: gated `output_callback_` (emits only during speech, pre-roll on onset); `is_speaking_` tracked unconditionally.

- [ ] **Step 1: Track is_speaking_ unconditionally + gate output**

In `main/audio/processors/afe_audio_processor.cc`, in `AudioProcessorTask`, replace the VAD-state + output block (`:155-186`) with:

```cpp
        // VAD state — update is_speaking_ regardless of callback presence
        // (gating reads it), then notify on edges.
        bool was_speaking = is_speaking_;
        if (res->vad_state == VAD_SPEECH) {
            is_speaking_ = true;
        } else if (res->vad_state == VAD_SILENCE) {
            is_speaking_ = false;
        }
        bool rising  = is_speaking_ && !was_speaking;   // silence -> speech
        bool falling = !is_speaking_ && was_speaking;   // speech  -> silence
        if ((rising || falling) && vad_state_change_callback_) {
            vad_state_change_callback_(is_speaking_);
        }

#ifdef CONFIG_VAD_GATED_UPSTREAM
        const bool gating = IsUpstreamGatingActive();
#else
        const bool gating = false;
#endif

        if (gating && falling) {
            // Discard the partial sub-frame tail so residual samples can't bleed
            // into the next utterance after the eos marker.
            output_buffer_.clear();
        }

        if (output_callback_ && (!gating || is_speaking_)) {
            // On the rising edge, emit the AFE pre-speech cache first so the
            // first word isn't clipped.
            if (gating && rising && res->vad_cache != nullptr && res->vad_cache_size > 0) {
                size_t cache_samples = res->vad_cache_size / sizeof(int16_t);
                output_buffer_.insert(output_buffer_.end(), res->vad_cache,
                                      res->vad_cache + cache_samples);
            }
            size_t samples = res->data_size / sizeof(int16_t);
            output_buffer_.insert(output_buffer_.end(), res->data, res->data + samples);
            while (output_buffer_.size() >= frame_samples_) {
                if (output_buffer_.size() == frame_samples_) {
                    output_callback_(std::move(output_buffer_));
                    output_buffer_.clear();
                    output_buffer_.reserve(frame_samples_);
                } else {
                    output_callback_(std::vector<int16_t>(output_buffer_.begin(),
                                                          output_buffer_.begin() + frame_samples_));
                    output_buffer_.erase(output_buffer_.begin(),
                                         output_buffer_.begin() + frame_samples_);
                }
            }
        }
```

(`is_speaking_` already exists, `:157`. The old block updated it inside `if (vad_state_change_callback_)` and always ran the output framing; this version tracks it unconditionally and gates the output framing on `is_speaking_` when gating.)

- [ ] **Step 2: Build green**

Run: `source /Users/tung/esp/esp-idf/export.sh && idf.py build`
Expected: `Project build complete`. (Gating now active at runtime on tuni-p4; raw-edge `listen.stop` still fires — Task 4 wires the marker eos and suppresses the raw edge. Not flashed.)

- [ ] **Step 3: Commit**

```bash
git add main/audio/processors/afe_audio_processor.h main/audio/processors/afe_audio_processor.cc
git commit -m "feat(tuni-p4): gate AFE upstream output on VAD speech with vad_cache pre-roll

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: End-of-utterance marker enqueue + drain + raw-edge guard

**Files:**
- Modify: `main/audio/audio_service.cc:125-130` (`OnVadStateChange` handler)
- Modify: `main/application.cc:249-260` (send-drain) and `:278-288` (raw VAD edge)

**Interfaces:**
- Consumes: `MarkEndOfUtterance(generation)`, `upstream_generation_`, `IsUpstreamGatingActive()` (Tasks 1–2); `AudioStreamPacket::end_of_utterance` (Task 2).
- Produces: marker-driven `listen.stop`.

- [ ] **Step 1: Enqueue the marker on the gated speech→silence edge**

In `main/audio/audio_service.cc`, in the processor's `OnVadStateChange` handler (`:125-130`), enqueue the marker when gating and speech ended:

```cpp
    audio_processor_->OnVadStateChange([this](bool speaking) {
        voice_detected_ = speaking;
        if (callbacks_.on_vad_change) {
            callbacks_.on_vad_change(speaking);
        }
#ifdef CONFIG_VAD_GATED_UPSTREAM
        if (IsUpstreamGatingActive() && !speaking) {
            // After the last PCM of this utterance; rides the same FIFO.
            MarkEndOfUtterance(upstream_generation_.load(std::memory_order_relaxed));
        }
#endif
    });
```

- [ ] **Step 2: Handle the marker in the send-drain**

In `main/application.cc`, the `MAIN_EVENT_SEND_AUDIO` drain (`:249-260`): branch on the marker before `SendAudio`:

```cpp
        if (bits & MAIN_EVENT_SEND_AUDIO) {
            if (!(audio_batch_mode_ && GetDeviceState() == kDeviceStateListening)) {
                while (auto packet = audio_service_.PopPacketFromSendQueue()) {
                    if (packet->end_of_utterance) {
                        // Marker rides the FIFO behind the PCM — eos is now
                        // strictly after the last frame. Marker proves a
                        // speech->silence transition, so no vad_had_speech_in_turn_.
                        if (listening_mode_ == kListeningModeAutoStop &&
                            GetDeviceState() == kDeviceStateListening &&
                            !listen_stop_sent_ && protocol_) {
                            ESP_LOGI(TAG, "EOS marker, sending listen.stop");
                            protocol_->SendStopListening();
                            listen_stop_sent_ = true;
                        }
                        continue;   // never SendAudio() a marker
                    }
                    if (protocol_ && !protocol_->SendAudio(std::move(packet))) {
                        break;
                    }
                }
            }
        }
```

- [ ] **Step 3: Suppress the raw-edge listen.stop when gating is active**

In `main/application.cc`, the `MAIN_EVENT_VAD_CHANGE` handler (`:278-288`), guard the existing raw-edge eos:

```cpp
                if (listening_mode_ == kListeningModeAutoStop && !listen_stop_sent_ &&
                    !audio_service_.IsUpstreamGatingActive()) {
                    if (vad_speaking_) {
                        vad_had_speech_in_turn_ = true;
                    } else if (vad_had_speech_in_turn_) {
                        if (protocol_) {
                            ESP_LOGI(TAG, "VAD end-of-speech, sending listen.stop");
                            protocol_->SendStopListening();
                            listen_stop_sent_ = true;
                        }
                    }
                }
```

- [ ] **Step 4: Build green**

Run: `source /Users/tung/esp/esp-idf/export.sh && idf.py build`
Expected: `Project build complete`. (Functional path complete: gated audio + marker eos.)

- [ ] **Step 5: Commit**

```bash
git add main/audio/audio_service.cc main/application.cc
git commit -m "feat(tuni-p4): eos marker enqueue + send-drain listen.stop; guard raw edge

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Race-free lifecycle reset + generation bump on stop

**Files:**
- Modify: `main/audio/processors/afe_audio_processor.h` (member)
- Modify: `main/audio/processors/afe_audio_processor.cc:109-125,135-153`
- Modify: `main/audio/audio_service.cc:654-680` (`EnableVoiceProcessing`)

**Interfaces:**
- Consumes: `BumpUpstreamGeneration()` (Task 2).
- Produces: task-owned reset of `is_speaking_`/`output_buffer_`; upstream generation bumped + queues cleared on stop.

- [ ] **Step 1: Add reset_pending_ and set it in Stop()**

In `main/audio/processors/afe_audio_processor.h`, add `#include <atomic>` and member `std::atomic<bool> reset_pending_{false};`.

In `main/audio/processors/afe_audio_processor.cc`, `Stop()` (`:113-121`) sets the flag (does **not** touch task-owned state):

```cpp
void AfeAudioProcessor::Stop() {
    xEventGroupClearBits(event_group_, PROCESSOR_RUNNING);
    reset_pending_.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(input_buffer_mutex_);
    if (afe_data_ != nullptr) {
        afe_iface_->reset_buffer(afe_data_);
    }
    input_buffer_.clear();
}
```

- [ ] **Step 2: Consume reset_pending_ after every fetch**

In `main/audio/processors/afe_audio_processor.cc`, `AudioProcessorTask`, right after `fetch_with_delay` returns and the running-bit check (`:144-147`), consume the reset and discard that result:

```cpp
        auto res = afe_iface_->fetch_with_delay(afe_data_, portMAX_DELAY);
        if ((xEventGroupGetBits(event_group_) & PROCESSOR_RUNNING) == 0) {
            continue;
        }
        if (reset_pending_.exchange(false, std::memory_order_relaxed)) {
            is_speaking_ = false;
            output_buffer_.clear();
            continue;   // discard this fetch; it may belong to the prior session
        }
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            ...
        }
```

- [ ] **Step 3: Bump generation + clear upstream queues on voice-processing stop**

In `main/audio/audio_service.cc`, `EnableVoiceProcessing(false)` branch (`:654-680`), call `BumpUpstreamGeneration()` before/with `audio_processor_->Stop()`:

```cpp
    } else {
        BumpUpstreamGeneration();   // discard any in-flight upstream PCM / eos marker
        audio_processor_->Stop();
        xEventGroupClearBits(event_group_, AS_EVENT_AUDIO_PROCESSOR_RUNNING);
    }
```

- [ ] **Step 4: Build green**

Run: `source /Users/tung/esp/esp-idf/export.sh && idf.py build`
Expected: `Project build complete`.

- [ ] **Step 5: Commit**

```bash
git add main/audio/processors/afe_audio_processor.h main/audio/processors/afe_audio_processor.cc main/audio/audio_service.cc
git commit -m "feat(tuni-p4): race-free reset_pending_ + generation bump on voice-processing stop

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Flash + hardware verification (acceptance)

Hardware-in-the-loop. Deliverable = the spec's acceptance properties confirmed on the device.

**Files:** none unless a defect is found.

- [ ] **Step 1: Confirm serial port**

Run: `ls /dev/cu.usbmodem*` (board on the **P4 "USB & Power"** USB-C port, direct to Mac). Replug if absent.

- [ ] **Step 2: Flash**

From `build/`: `python -m esptool --chip esp32p4 -p <PORT> -b 230400 --before default_reset --after hard_reset write_flash @flash_args`
Expected: `Hash of data verified`, `Hard resetting`.

- [ ] **Step 3: Start serial capture + backend tail**

Serial reader on `<PORT>` @115200 (DTR/RTS low). In `robo-worker`: `pnpm wrangler tail robo-worker --format pretty`.

- [ ] **Step 4: Acceptance checks (spec §Testing)**

1. **Idle 3+ min (no speech):** backend shows **no `sendAudio`/`speech_begin`**; **zero "gặp trục trặc"** apologies.
2. **Ordering:** every utterance is `audio* → listen.stop` (serial `EOS marker, sending listen.stop` after the last frame); no late audio until the next onset; no empty audio frame for the marker.
3. **First word not clipped:** say "apple" → transcript is `"apple"`, not `"pple"`; reply plays; auto-returns to listening.
4. **Stop/restart while speaking** (interrupt with a reply mid-utterance), incl. rapid cycles: next turn works; no stuck state; no stale audio/eos.
5. **Fail-safe sanity:** (code review) gating-off path (other board / device-AEC) still uses raw-edge `listen.stop`.

- [ ] **Step 5: If a defect appears**

Stop. Use `superpowers:systematic-debugging` — root-cause from serial + tail before any change. Fix the specific task, rebuild, re-flash, re-verify.

- [ ] **Step 6: Commit (only if code changed)**

```bash
git add -A
git commit -m "fix(tuni-p4): <defect> found in VAD-gated streaming hardware verification

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Done when

- `tuni-p4` builds green with `CONFIG_VAD_GATED_UPSTREAM=y`.
- On hardware: minutes of silence produce no upstream audio and no apologies; every utterance is `audio* → listen.stop`; first words aren't clipped; stop/restart leaves no stale state; other boards/configs are unchanged (gating compiled out / inactive).
