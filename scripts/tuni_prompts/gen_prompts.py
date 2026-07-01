#!/usr/bin/env python3
"""Generate Tuni-voiced vi-VN boot/connection prompts.

Synthesizes each line in the production Chirp voice *clone* (the same voice the
backend uses for conversation) via Google Cloud TTS, then encodes to the exact
Opus/Ogg format the firmware's embedded prompts use, writing into
main/assets/locales/vi-VN/.

See README.md for prerequisites and usage.
"""
import argparse
import base64
import json
import os
import subprocess
import tempfile
import urllib.request

# --- Voice / pacing -------------------------------------------------------
# The production voice is a Chirp 3 Instant Custom Voice CLONE keyed by this
# file (NOT the named vi-VN-Chirp3-HD-Achernar). language_code must be vi-VN.
CLONE_KEY_DEFAULT = "/Users/tung/robo-bridge/clone_key_vi.txt"
LANGUAGE_CODE = "vi-VN"
# Slightly faster than native. Range [0.25, 2.0]; <1.0 slows, >1.0 speeds up.
SPEAKING_RATE = 1.1

# --- Output ---------------------------------------------------------------
LOCALE_DIR_DEFAULT = os.path.normpath(os.path.join(
    os.path.dirname(__file__), "..", "..", "main", "assets", "locales", "vi-VN"))

# Encode target = exact embedded-prompt format (see Task spec). Opus reports
# 48000 Hz in the Ogg header regardless; -ar 16000 is the pre-encode resample.
OPUS_ARGS = ["-c:a", "libopus", "-b:a", "16k", "-ac", "1", "-ar", "16000",
             "-frame_duration", "60"]
# Trim leading + trailing silence (reverse trick), then normalize loudness to
# match the embedded prompts (~-16 LUFS).
TRIM = ("silenceremove=start_periods=1:start_silence=0.05:start_threshold=-45dB,"
        "areverse,"
        "silenceremove=start_periods=1:start_silence=0.05:start_threshold=-45dB,"
        "areverse,"
        "loudnorm=I=-16:TP=-1.5:LRA=11")


def _quota_project():
    return os.environ.get("GCP_QUOTA_PROJECT") or subprocess.run(
        ["gcloud", "config", "get-value", "project"],
        capture_output=True, text=True).stdout.strip()


def _access_token():
    return subprocess.run(["gcloud", "auth", "print-access-token"],
                          capture_output=True, text=True).stdout.strip()


def synthesize(text, clone_key_path=CLONE_KEY_DEFAULT, language_code=LANGUAGE_CODE,
               speaking_rate=SPEAKING_RATE):
    """Return WAV (LINEAR16 mono 24k) bytes for `text` in the Tuni clone voice."""
    key = open(clone_key_path).read().strip()
    body = {
        "input": {"text": text},
        "voice": {"languageCode": language_code,
                  "voiceClone": {"voiceCloningKey": key}},
        "audioConfig": {"audioEncoding": "LINEAR16",
                        "sampleRateHertz": 24000,
                        "speakingRate": speaking_rate},
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
    """Encode a wav to the exact embedded-prompt Opus/Ogg format (trim + loudnorm)."""
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error",
                    "-i", wav_path, "-af", TRIM, *OPUS_ARGS, out_path, "-y"],
                   check=True)


def _duration(path):
    return subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "default=nk=1:nw=1", path],
        capture_output=True, text=True).stdout.strip()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--manifest",
                    default=os.path.join(os.path.dirname(__file__), "prompts.vi-VN.json"))
    ap.add_argument("--locale-dir", default=LOCALE_DIR_DEFAULT)
    ap.add_argument("--clone-key", default=CLONE_KEY_DEFAULT)
    ap.add_argument("--tts-language", default=LANGUAGE_CODE,
                    help="Google TTS language_code (e.g. en-US). Default vi-VN.")
    ap.add_argument("--speaking-rate", type=float, default=SPEAKING_RATE,
                    help="TTS speaking rate multiplier [0.25, 2.0]. Default 1.1.")
    ap.add_argument("--only", help="generate only this output filename (e.g. ready.ogg)")
    args = ap.parse_args()

    manifest = json.load(open(args.manifest))
    os.makedirs(args.locale_dir, exist_ok=True)
    for name, text in manifest.items():
        if args.only and name != args.only:
            continue
        with tempfile.TemporaryDirectory() as d:
            wav = os.path.join(d, "s.wav")
            open(wav, "wb").write(synthesize(text, args.clone_key, args.tts_language,
                                             args.speaking_rate))
            out = os.path.join(args.locale_dir, name)
            encode_to_ogg(wav, out)
            print(f"{name}: {text!r} -> {out} ({_duration(out)}s)")


if __name__ == "__main__":
    main()
