# Tuni boot/connection voice prompts

Generates the Vietnamese boot/connection prompts the device speaks before it can
talk (finding wifi, connecting, ready, wifi-setup, disconnected, reconnected),
in **Tuni's voice** — the same Chirp 3 Instant Custom Voice *clone* the backend
uses for conversation. Output OGGs go straight into `main/assets/locales/vi-VN/`
and are embedded in the firmware at build time.

## Prerequisites

- `ffmpeg` + `ffprobe` on PATH.
- `gcloud` logged in (`gcloud auth login`) with access to the Cloud Text-to-Speech
  API on a quota project. The script uses `gcloud auth print-access-token` and
  `gcloud config get-value project` (override the project with `GCP_QUOTA_PROJECT`).
- The voice clone key at `/Users/tung/robo-bridge/clone_key_vi.txt` (override with
  `--clone-key`). This is what makes the output sound like Tuni — **not** a named
  voice. `language_code` is `vi-VN`.

## Usage

```bash
cd scripts/tuni_prompts
python3 gen_prompts.py            # regenerate all six lines
python3 gen_prompts.py --only ready.ogg   # just one line
```

Each line prints its output path and duration. Audition with `afplay`:

```bash
afplay ../../main/assets/locales/vi-VN/ready.ogg
```

To change a line, edit `prompts.vi-VN.json` and re-run.

## Format / pacing (don't change without reason)

`encode_to_ogg` produces the **exact** format of the existing embedded prompts so
the firmware's Ogg demuxer + Opus decoder play them correctly:
`libopus, -b:a 16k, mono, -ar 16000, -frame_duration 60`, with leading/trailing
silence trimmed and loudness normalized to ~-16 LUFS. (ffprobe will report
`48000 Hz` — an Opus header quirk — that is correct.)

`SPEAKING_RATE = 0.9` in `gen_prompts.py` gives a calm, clear pace for 5-8 yr-olds
(slightly slower than native). The drafted lines are intentionally full sentences,
so each runs ~5-6 s; that's accepted (the firmware flushes stale boot audio before
the "ready" line, so only that line's length gates auto-start-listening).

## Test

```bash
cd scripts/tuni_prompts && python3 -m pytest test_encode.py -v
```

Exercises only the ffmpeg encode (format + silence-trim) — no network/TTS needed.
