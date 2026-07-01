# Tuni P4 Boot/Connection Voice Prompts — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the generic Vietnamese boot/connection prompts with custom Tuni-voiced
lines (same Chirp voice clone as the backend), generated offline and embedded in the
firmware; add a spoken "ready" line that isn't clipped by auto-start-listening.

**Architecture:** A standalone Python generator synthesizes each line via Google Cloud TTS
with the production voice-clone key, encodes to the exact embedded-prompt OGG/Opus format,
and writes into `main/assets/locales/vi-VN/`. Five prompts reuse existing filenames →
zero firmware code (existing wiring plays them). The new `ready.ogg` needs a small,
`#ifdef CONFIG_LANGUAGE_VI_VN`-guarded change at the activation-done path plus a build-system
fix so the generated `OGG_READY` constant exists.

**Tech Stack:** Python 3 + `ffmpeg`/`ffprobe`, Google Cloud Text-to-Speech (v1beta1, voice
cloning), ESP-IDF (C++), CMake, `gen_lang.py`.

**Spec:** `docs/superpowers/specs/2026-06-30-tuni-p4-boot-voice-prompts-design.md`

## Global Constraints

- **Locale scope:** changes the **`vi-VN` locale**; tuni-p4 is the only board that pins it. No board-specific prompt layer (rejected as YAGNI). Lines literally say "Tuni".
- **Ready-line guard:** the activation-done code change MUST be wrapped in `#ifdef CONFIG_LANGUAGE_VI_VN` / `#else` (keep `OGG_SUCCESS`) so non-vi builds are byte-identical to today.
- **Voice:** Google Cloud TTS with `voice_clone.voice_cloning_key` = contents of `/Users/tung/robo-bridge/clone_key_vi.txt`, `language_code = vi-VN`. NOT the named `vi-VN-Chirp3-HD-Achernar`.
- **Audio format (match embedded prompts exactly):** Opus in Ogg, mono, `-ar 16000`, `-ac 1`, `-b:a 16k`, `-frame_duration 60`, leading/trailing silence trimmed, loudnorm `I=-16` LUFS. (ffprobe will report `48000 Hz` — Opus header quirk — that's correct.)
- **Pacing / wording (user decision, Task 1 + on-device tuning):** `speakingRate = 1.1` (slightly faster than native; tuned on hardware from 0.9→1.0→1.1). Keep the full drafted wording — the prior "≤ ~2.5 s" cap is **dropped**; lines run ~4–5 s and that's accepted. flush-before-ready (Task 5) bounds the only duration-sensitive case (auto-listen waits on the ready line alone; poll ceiling 10 s covers the longest take).
- **Build env:** `source /Users/tung/esp/esp-idf/export.sh` before any `idf.py`/esptool.
- **Flash (from `build/`):** `python -m esptool --chip esp32p4 -p <PORT> -b 230400 --before default_reset --after hard_reset write_flash @flash_args`. NOT `idf.py flash`/460800. Serial port `/dev/cu.usbmodem*` (re-enumerates; replug if it vanishes; never run a serial reader during flash).
- **macOS has no `timeout`.**

---

## Task 1: Spike — confirm offline clone-voice synthesis

De-risk the one real unknown before building anything: can we synthesize the production
clone voice offline, and via the **unary** REST call or only **streaming**? Produces one
verified sample and the confirmed method, which Task 2 hard-codes.

**Files:**
- Create (throwaway, not committed): `/tmp/tuni_tts_spike/` (scratch)

**Interfaces:**
- Produces (for Task 2): the working synthesis recipe — endpoint, request body shape, and which path (unary vs streaming) succeeds.

- [ ] **Step 1: Set up auth + inputs**

```bash
source /Users/tung/esp/esp-idf/export.sh 2>/dev/null || true   # not needed here, harmless
mkdir -p /tmp/tuni_tts_spike && cd /tmp/tuni_tts_spike
export QUOTA_PROJECT="$(gcloud config get-value project 2>/dev/null)"
export ACCESS_TOKEN="$(gcloud auth print-access-token)"
test -s /Users/tung/robo-bridge/clone_key_vi.txt && echo "clone key present" || echo "MISSING clone key"
echo "quota project: $QUOTA_PROJECT"
```
Expected: "clone key present" and a non-empty project. If the token/project is missing, the user must `gcloud auth login` / `gcloud config set project <id>` first.

- [ ] **Step 2: Try the UNARY REST synth with the clone key**

```bash
cd /tmp/tuni_tts_spike
python3 - <<'PY'
import json, base64, os, subprocess, urllib.request
key = open("/Users/tung/robo-bridge/clone_key_vi.txt").read().strip()
body = {
  "input": {"text": "Tuni đang tìm wifi nè, bạn chờ Tuni một chút xíu nha!"},
  "voice": {"languageCode": "vi-VN", "voiceClone": {"voiceCloningKey": key}},
  "audioConfig": {"audioEncoding": "LINEAR16", "sampleRateHertz": 24000},
}
req = urllib.request.Request(
  "https://texttospeech.googleapis.com/v1beta1/text:synthesize",
  data=json.dumps(body).encode(),
  headers={
    "Authorization": "Bearer " + os.environ["ACCESS_TOKEN"],
    "x-goog-user-project": os.environ["QUOTA_PROJECT"],
    "Content-Type": "application/json",
  })
try:
    resp = json.load(urllib.request.urlopen(req))
    open("spike.wav","wb").write(base64.b64decode(resp["audioContent"]))
    print("UNARY OK -> spike.wav")
except urllib.error.HTTPError as e:
    print("UNARY FAILED", e.code); print(e.read().decode()[:800])
PY
```
Expected: either `UNARY OK -> spike.wav`, or a clear error (e.g. voice cloning unsupported in unary → use streaming in the next step).

- [ ] **Step 3: If unary failed, try the streaming Python client**

Only if Step 2 failed. Install the client into a scratch venv and stream-synthesize:

```bash
cd /tmp/tuni_tts_spike
python3 -m venv venv && ./venv/bin/pip -q install google-cloud-texttospeech
GOOGLE_CLOUD_PROJECT="$QUOTA_PROJECT" ./venv/bin/python - <<'PY'
from google.cloud import texttospeech_v1beta1 as tts
key = open("/Users/tung/robo-bridge/clone_key_vi.txt").read().strip()
client = tts.TextToSpeechClient()
voice = tts.VoiceSelectionParams(language_code="vi-VN",
        voice_clone=tts.VoiceCloneParams(voice_cloning_key=key))
sconf = tts.StreamingSynthesizeConfig(voice=voice,
        streaming_audio_config=tts.StreamingAudioConfig(
            audio_encoding=tts.AudioEncoding.PCM, sample_rate_hertz=24000))
def reqs():
    yield tts.StreamingSynthesizeRequest(streaming_config=sconf)
    yield tts.StreamingSynthesizeRequest(input=tts.StreamingSynthesisInput(
        text="Tuni đang tìm wifi nè, bạn chờ Tuni một chút xíu nha!"))
pcm = b"".join(r.audio_content for r in client.streaming_synthesize(reqs()))
# wrap raw PCM16 mono 24k in a wav header via ffmpeg
open("spike.pcm","wb").write(pcm)
print("STREAMING OK -> spike.pcm", len(pcm), "bytes")
PY
ffmpeg -hide_banner -loglevel error -f s16le -ar 24000 -ac 1 -i spike.pcm spike.wav && echo "wrapped -> spike.wav"
```
Expected: `STREAMING OK` then `wrapped -> spike.wav`.

- [ ] **Step 4: Listen + verify it's the Tuni voice**

```bash
cd /tmp/tuni_tts_spike && ffprobe -hide_banner spike.wav 2>&1 | grep -E "Duration|Audio"
afplay spike.wav   # macOS playback
```
Expected: audible Vietnamese in Tuni's voice (compare to a known conversation clip if unsure). This is the gate — if the voice is wrong, STOP and re-check the clone key / language code before proceeding.

- [ ] **Step 5: Record the confirmed method**

No commit (throwaway). Note in the Task 2 work which path won (unary vs streaming) and the exact encoding requested — Task 2 implements that path.

---

## Task 2: Reusable prompt generator (`scripts/tuni_prompts/`)

Build the committed generator: a manifest of lines + a script that synthesizes each (the
Task-1-confirmed path) and encodes to the exact embedded-prompt format. TDD the **encode**
function via `ffprobe`; synthesis is integration-verified by Task 1 + Task 3.

**Files:**
- Create: `scripts/tuni_prompts/prompts.vi-VN.json`
- Create: `scripts/tuni_prompts/gen_prompts.py`
- Create: `scripts/tuni_prompts/README.md`
- Create: `scripts/tuni_prompts/test_encode.py`

**Interfaces:**
- Produces (for Task 3): `gen_prompts.py` writes `<name>.ogg` for every manifest entry into `main/assets/locales/vi-VN/`. Functions: `synthesize(text:str) -> bytes` (wav bytes), `encode_to_ogg(wav_path:str, out_path:str) -> None`.
- Consumes: clone key path (default `/Users/tung/robo-bridge/clone_key_vi.txt`), quota project (default `gcloud config get-value project`).

- [ ] **Step 1: Write the manifest**

`scripts/tuni_prompts/prompts.vi-VN.json`:
```json
{
  "wifi_scanning.ogg":   "Tuni đang tìm wifi nè, bạn chờ Tuni một chút xíu nha!",
  "wifi_connecting.ogg": "Tuni đang kết nối nè, sắp xong rồi, đợi Tuni xíu nha!",
  "ready.ogg":           "A, Tuni sẵn sàng rồi! Hôm nay mình cùng chơi gì nào?",
  "wificonfig.ogg":      "Tuni chưa có wifi. Bạn nhờ ba mẹ cài wifi cho Tuni nha!",
  "disconnected.ogg":    "Ơ, Tuni bị rớt mạng rồi. Tuni thử kết nối lại nha!",
  "reconnected.ogg":     "A, Tuni kết nối lại được rồi! Mình chơi tiếp nha!"
}
```

- [ ] **Step 2: Write the failing encode test**

`scripts/tuni_prompts/test_encode.py`:
```python
import subprocess, json, os, tempfile
from gen_prompts import encode_to_ogg

def _make_sine_wav(path):
    subprocess.run(["ffmpeg","-hide_banner","-loglevel","error","-f","lavfi",
        "-i","sine=frequency=440:duration=1","-ar","24000","-ac","1",path,"-y"], check=True)

def test_encode_to_ogg_matches_embedded_format():
    with tempfile.TemporaryDirectory() as d:
        wav, ogg = os.path.join(d,"in.wav"), os.path.join(d,"out.ogg")
        _make_sine_wav(wav)
        encode_to_ogg(wav, ogg)
        out = subprocess.run(["ffprobe","-hide_banner","-v","error","-show_streams",
            "-of","json", ogg], capture_output=True, text=True).stdout
        st = json.loads(out)["streams"][0]
        assert st["codec_name"] == "opus"
        assert st["channels"] == 1
        assert os.path.getsize(ogg) > 0
```

- [ ] **Step 3: Run the test, verify it fails**

Run: `cd scripts/tuni_prompts && python3 -m pytest test_encode.py -v`
Expected: FAIL — `ImportError`/`encode_to_ogg` not defined.

- [ ] **Step 4: Implement `gen_prompts.py`**

Implement the Task-1-confirmed `synthesize()` (use the unary branch if Step 2 of Task 1 won, else the streaming branch — keep the losing branch out), plus the encode and a `main()` that loops the manifest.
```python
#!/usr/bin/env python3
"""Generate Tuni-voiced vi-VN boot prompts. See README.md."""
import argparse, base64, json, os, subprocess, tempfile, urllib.request, urllib.error

CLONE_KEY_DEFAULT = "/Users/tung/robo-bridge/clone_key_vi.txt"
LOCALE_DIR_DEFAULT = os.path.join(os.path.dirname(__file__), "..", "..",
                                  "main", "assets", "locales", "vi-VN")

def _quota_project():
    return os.environ.get("GCP_QUOTA_PROJECT") or subprocess.run(
        ["gcloud","config","get-value","project"], capture_output=True, text=True).stdout.strip()

def _access_token():
    return subprocess.run(["gcloud","auth","print-access-token"],
                          capture_output=True, text=True).stdout.strip()

def synthesize(text, clone_key_path=CLONE_KEY_DEFAULT):
    """Return WAV (LINEAR16 mono 24k) bytes for `text` in the Tuni clone voice."""
    key = open(clone_key_path).read().strip()
    body = {
        "input": {"text": text},
        "voice": {"languageCode": "vi-VN", "voiceClone": {"voiceCloningKey": key}},
        "audioConfig": {"audioEncoding": "LINEAR16", "sampleRateHertz": 24000},
    }
    req = urllib.request.Request(
        "https://texttospeech.googleapis.com/v1beta1/text:synthesize",
        data=json.dumps(body).encode(),
        headers={"Authorization": "Bearer " + _access_token(),
                 "x-goog-user-project": _quota_project(),
                 "Content-Type": "application/json"})
    resp = json.load(urllib.request.urlopen(req))
    return base64.b64decode(resp["audioContent"])

def encode_to_ogg(wav_path, out_path):
    """Encode a wav to the exact embedded-prompt Opus/Ogg format."""
    subprocess.run(["ffmpeg","-hide_banner","-loglevel","error","-i",wav_path,
        "-af","loudnorm=I=-16:TP=-1.5:LRA=11",
        "-c:a","libopus","-b:a","16k","-ac","1","-ar","16000",
        "-frame_duration","60", out_path, "-y"], check=True)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", default=os.path.join(os.path.dirname(__file__), "prompts.vi-VN.json"))
    ap.add_argument("--locale-dir", default=LOCALE_DIR_DEFAULT)
    ap.add_argument("--clone-key", default=CLONE_KEY_DEFAULT)
    ap.add_argument("--only", help="generate only this output filename")
    args = ap.parse_args()
    manifest = json.load(open(args.manifest))
    os.makedirs(args.locale_dir, exist_ok=True)
    for name, text in manifest.items():
        if args.only and name != args.only:
            continue
        with tempfile.TemporaryDirectory() as d:
            wav = os.path.join(d, "s.wav")
            open(wav, "wb").write(synthesize(text, args.clone_key))
            out = os.path.join(args.locale_dir, name)
            encode_to_ogg(wav, out)
            dur = subprocess.run(["ffprobe","-v","error","-show_entries","format=duration",
                "-of","default=nk=1:nw=1", out], capture_output=True, text=True).stdout.strip()
            print(f"{name}: {text!r} -> {out} ({dur}s)")

if __name__ == "__main__":
    main()
```
(If Task 1 found unary unsupported, replace `synthesize()`'s body with the streaming client from Task 1 Step 3, returning wav bytes.)

- [ ] **Step 5: Run the encode test, verify it passes**

Run: `cd scripts/tuni_prompts && python3 -m pytest test_encode.py -v`
Expected: PASS.

- [ ] **Step 6: Write the README**

`scripts/tuni_prompts/README.md`: prerequisites (`gcloud auth login`, a quota project with the Cloud TTS API enabled, `ffmpeg`), the clone-key location, usage (`python3 gen_prompts.py`, `--only ready.ogg`), the format rationale (must match embedded prompts), and a note that lines must stay ≤ ~2.5 s.

- [ ] **Step 7: Commit**

```bash
git add scripts/tuni_prompts/
git commit -m "feat(tuni-p4): offline generator for Tuni-voiced vi-VN boot prompts

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: Generate + place the six prompt assets

Run the generator for real, audition every line, and place them in the locale dir. After
this, the five reused filenames are live with zero firmware change.

**Files:**
- Modify (overwrite): `main/assets/locales/vi-VN/{wifi_scanning,wifi_connecting,wificonfig,disconnected,reconnected}.ogg`
- Create: `main/assets/locales/vi-VN/ready.ogg`

**Interfaces:**
- Produces (for Task 4): `ready.ogg` present in `vi-VN/` so `gen_lang.py` will emit `OGG_READY`.

- [ ] **Step 1: Generate all six lines**

Run: `cd scripts/tuni_prompts && python3 gen_prompts.py`
Expected: six `name: '...' -> .../vi-VN/name.ogg (<dur>s)` lines, each duration ≤ ~2.5 s. If any exceeds ~2.5 s, shorten its manifest text and re-run with `--only <name>.ogg`.

- [ ] **Step 2: Verify format on every file**

Run:
```bash
cd /Users/tung/xiaozhi-esp32
for f in wifi_scanning wifi_connecting ready wificonfig disconnected reconnected; do
  echo "== $f =="; ffprobe -hide_banner main/assets/locales/vi-VN/$f.ogg 2>&1 | grep -E "Duration|Audio"
done
```
Expected: each `Audio: opus, 48000 Hz, mono`, Duration ≤ ~2.5 s.

- [ ] **Step 3: Audition each line**

Run: `for f in wifi_scanning wifi_connecting ready wificonfig disconnected reconnected; do echo $f; afplay main/assets/locales/vi-VN/$f.ogg; done`
Expected: correct Tuni voice, intelligible, no clipping/garble. Re-generate any bad line before committing.

- [ ] **Step 4: Commit**

```bash
git add main/assets/locales/vi-VN/*.ogg
git commit -m "feat(tuni-p4): Tuni-voiced vi-VN boot prompt audio (incl. new ready.ogg)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 4: Build-system — pin locale, fix header regen, materialize `OGG_READY`

Make `OGG_READY` exist in the generated header (the CMake rule doesn't depend on `.ogg`
files, so a new file alone won't regenerate it) and pin `vi-VN` so `release.py` can't revert
it.

**Files:**
- Modify: `main/boards/tuni-p4/config.json` (both builds)
- Modify: `main/CMakeLists.txt:1024-1034` (`add_custom_command` DEPENDS)

**Interfaces:**
- Produces (for Task 5): `Lang::Sounds::OGG_READY` present in `main/assets/lang_config.h`; `CONFIG_LANGUAGE_VI_VN=y` pinned for tuni-p4 builds.

- [ ] **Step 1: Pin the locale in `config.json`**

Add `"CONFIG_LANGUAGE_VI_VN=y"` to the `sdkconfig_append` array of **both** the `tuni-p4` and `tuni-p4-p4x` builds in `main/boards/tuni-p4/config.json`.

- [ ] **Step 2: Add sound files to the header's CMake DEPENDS**

In `main/CMakeLists.txt`, change the `add_custom_command(OUTPUT ${LANG_HEADER} ...)` `DEPENDS` block (currently `${LANG_JSON}` + `gen_lang.py`) to also depend on the locale + common sound files, so adding/removing an `.ogg` regenerates the header:
```cmake
    DEPENDS
        ${LANG_JSON}
        ${PROJECT_DIR}/scripts/gen_lang.py
        ${LANG_SOUNDS}
        ${COMMON_SOUNDS}
```
(`LANG_SOUNDS`/`COMMON_SOUNDS` are already defined earlier in this file for `EMBED_FILES`.)

- [ ] **Step 3: Force a one-time header regen + confirm `OGG_READY`**

```bash
cd /Users/tung/xiaozhi-esp32
python scripts/gen_lang.py --language vi-VN --output main/assets/lang_config.h
grep -n "OGG_READY" main/assets/lang_config.h
```
Expected: at least one `OGG_READY` line (the generated constant). If absent, `ready.ogg` is missing from `vi-VN/` (re-do Task 3).

- [ ] **Step 4: Clean-build to prove the language + header are correct**

```bash
source /Users/tung/esp/esp-idf/export.sh
cd /Users/tung/xiaozhi-esp32
rm -f releases/v*_tuni-p4.zip 2>/dev/null || true
idf.py build
grep -n "CONFIG_LANGUAGE_VI_VN" build/config/sdkconfig | head
```
Expected: build succeeds; `CONFIG_LANGUAGE_VI_VN=y` present in `build/config/sdkconfig`. (Verifying the generated sdkconfig, not the release zip — `release.py` skips an existing zip.)

- [ ] **Step 5: Commit**

```bash
git add main/boards/tuni-p4/config.json main/CMakeLists.txt main/assets/lang_config.h
git commit -m "build(tuni-p4): pin vi-VN locale, regen lang header w/ OGG_READY, DEPENDS on sounds

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 5: Firmware — spoken ready line, guarded + un-clipped

Swap the activation-done chime for the spoken `OGG_READY` on vi-VN builds, flushing stale
boot audio first and waiting for playback to finish before auto-start-listening so it isn't
clipped by `ResetDecoder()`.

**Files:**
- Modify: `main/application.cc` (~387–396, `HandleActivationDoneEvent`)

**Interfaces:**
- Consumes: `Lang::Sounds::OGG_READY` (Task 4), `AudioService::{ResetDecoder, PlaySound, IsIdle}`.

- [ ] **Step 1: Replace the success-sound schedule with the guarded ready block**

In `main/application.cc`, change the `Schedule([this]() { audio_service_.PlaySound(Lang::Sounds::OGG_SUCCESS); });` block to:
```cpp
    Schedule([this]() {
#ifdef CONFIG_LANGUAGE_VI_VN
        // Tuni spoken "ready" line instead of the success chime.
        // Best-effort flush of any still-queued boot audio (scanning/connecting) so the
        // ready line plays immediately and auto-listen isn't delayed by backlog.
        audio_service_.ResetDecoder();
        audio_service_.PlaySound(Lang::Sounds::OGG_READY);
        // Wait for it to finish before auto-listen, else entering Listening's ResetDecoder
        // clips the tail. Bounded poll on IsIdle (no unbounded block) + a short settle to
        // cover the in-flight decode/output chunk that the queue-empty check can't see.
        for (int i = 0; i < 50 && !audio_service_.IsIdle(); ++i) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(150));
#else
        // Play the success sound to indicate the device is ready
        audio_service_.PlaySound(Lang::Sounds::OGG_SUCCESS);
#endif
    });
```
Leave the following `#ifdef CONFIG_AUTO_START_LISTENING` `Schedule([this]() { ToggleChatState(); });` exactly as-is — it runs after the block above completes (main-loop callbacks are serialized), so the wait gates it.

- [ ] **Step 2: Build green**

```bash
source /Users/tung/esp/esp-idf/export.sh
cd /Users/tung/xiaozhi-esp32 && idf.py build
```
Expected: compiles and links (proves `OGG_READY` resolves under the vi-VN config).

- [ ] **Step 3: Sanity-check the non-vi path compiles too (optional, cheap confidence)**

Confirm by inspection that the `#else` branch is byte-identical to the original
(`PlaySound(OGG_SUCCESS)`), so other-language builds are unaffected. No separate build
required.

- [ ] **Step 4: Commit**

```bash
git add main/application.cc
git commit -m "feat(tuni-p4): spoken ready line (vi-VN), flush+wait so auto-listen doesn't clip it

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Task 6: Hardware acceptance

Flash the gated build and run the spec's acceptance checks on the device. No code unless a
check fails.

**Files:** none (verification).

- [ ] **Step 1: Build (gated) + flash**

```bash
source /Users/tung/esp/esp-idf/export.sh
cd /Users/tung/xiaozhi-esp32 && idf.py build
# find the port, then (no serial reader running):
ls /dev/cu.usbmodem*
cd build && python -m esptool --chip esp32p4 -p <PORT> -b 230400 \
  --before default_reset --after hard_reset write_flash @flash_args
```
Expected: flash completes; device reboots. (Port re-enumerates — replug if it vanishes.)

- [ ] **Step 2: Cold-boot sequence (known WiFi)**

Power-cycle. Listen for: finding-wifi → connecting → **ready** lines in Tuni's voice; the
ready line is **not clipped**; auto-listen then starts (device responds to speech).
Expected: all three lines audible and complete; voice matches conversational Tuni.

- [ ] **Step 3: Setup-mode line**

Enter WiFi-config mode (hold BOOT during `Starting`, or clear creds).
Expected: the `wificonfig` line plays in Tuni's voice.

- [ ] **Step 4: Disconnect / reconnect lines**

Kill the AP mid-session, then restore it.
Expected: `disconnected` line on drop, `reconnected` line on recovery.

- [ ] **Step 5: Build durability**

```bash
cd /Users/tung/xiaozhi-esp32
grep -n "CONFIG_LANGUAGE_VI_VN" build/config/sdkconfig | head
```
Expected: `CONFIG_LANGUAGE_VI_VN=y` (locale survived the build).

- [ ] **Step 6: If the ready line clips (only if Step 2 failed)**

Escalate the wait precision: add an in-flight-playback flag to `AudioService` (set under
lock immediately before `codec_->OutputData()` in `AudioOutputTask`, clear + `notify_all`
after) and a `WaitForPlaybackFinished()` that also requires `!playback_in_flight_`; call it
in place of the `IsIdle` poll. Re-flash and re-run Step 2.

---

## Self-Review

- **Spec coverage:** voice clone (T1–2), format incl. `-b:a 16k` (T2 Global Constraints), six lines (T2–3), five zero-code swaps (T3), `ready.ogg`+`OGG_READY` build-safety via DEPENDS+regen (T4), `CONFIG_LANGUAGE_VI_VN` pin (T4) + guard (T5), flush+precise wait (T5), release.py false-pass avoided via sdkconfig check (T4/T6), locale-scope reality (Global Constraints), hardware acceptance (T6). Covered.
- **Placeholders:** none — every code/command step is concrete. The only conditional is Task 1's unary-vs-streaming spike, with complete code for both branches and a decision gate.
- **Type/name consistency:** `synthesize`/`encode_to_ogg` defined in T2 and used in T2/T3; `OGG_READY` produced in T4, consumed in T5; `ResetDecoder`/`PlaySound`/`IsIdle` are existing `AudioService` methods.
