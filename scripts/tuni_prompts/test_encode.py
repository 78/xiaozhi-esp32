"""Format test for encode_to_ogg: output must match the embedded-prompt format.

Run: cd scripts/tuni_prompts && python3 -m pytest test_encode.py -v
(Only exercises the ffmpeg encode — no network/TTS needed.)
"""
import json
import os
import subprocess
import tempfile

from gen_prompts import encode_to_ogg


def _make_sine_wav(path):
    # 1s 440Hz tone at 24k mono, with leading/trailing silence so the trim runs.
    subprocess.run(
        ["ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "lavfi",
         "-i", "sine=frequency=440:duration=1",
         "-af", "adelay=300|300,apad=pad_dur=0.3",
         "-ar", "24000", "-ac", "1", path, "-y"], check=True)


def _stream0(path):
    out = subprocess.run(
        ["ffprobe", "-hide_banner", "-v", "error", "-show_streams",
         "-of", "json", path], capture_output=True, text=True).stdout
    return json.loads(out)["streams"][0]


def test_encode_to_ogg_matches_embedded_format():
    with tempfile.TemporaryDirectory() as d:
        wav, ogg = os.path.join(d, "in.wav"), os.path.join(d, "out.ogg")
        _make_sine_wav(wav)
        encode_to_ogg(wav, ogg)
        st = _stream0(ogg)
        assert st["codec_name"] == "opus", st["codec_name"]
        assert int(st["channels"]) == 1, st["channels"]
        assert os.path.getsize(ogg) > 0


def test_encode_trims_silence():
    """The 1.6s padded tone should come back close to ~1s after trimming."""
    with tempfile.TemporaryDirectory() as d:
        wav, ogg = os.path.join(d, "in.wav"), os.path.join(d, "out.ogg")
        _make_sine_wav(wav)
        encode_to_ogg(wav, ogg)
        dur = float(subprocess.run(
            ["ffprobe", "-v", "error", "-show_entries", "format=duration",
             "-of", "default=nk=1:nw=1", ogg],
            capture_output=True, text=True).stdout.strip())
        assert dur < 1.4, f"silence not trimmed: {dur}s"
