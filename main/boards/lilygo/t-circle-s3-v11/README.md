# LILYGO T-Circle-S3 V1.1

Board support for the **V1.1** revision of the LILYGO T-Circle-S3.

## Why this is a separate board

V1.0 and V1.1 differ only in the microphone:

| | V1.0 | V1.1 |
| --- | --- | --- |
| Microphone | MSM261S4030H0R (I2S) | MP34DT05-A (**PDM**) |
| Mic pins | BCLK 7, WS 9, DATA 8 | CLK 9, DATA 8 |
| Speaker | MAX98357A: BCLK 5, LRCLK 4, DATA 6, SD_MODE 45 | same |

A PDM microphone cannot be read by an I2S standard-mode receiver, so the V1.0
board (`lilygo/t-circle-s3`) captures no audio at all on V1.1 hardware: the
wake word never triggers because nothing reaches the detector.

Board identity affects OTA compatibility, so V1.1 is a separate board rather
than a change to the existing one. Check the silkscreen on the back of the
board to tell the revisions apart.

## Audio

Uses the shared `NoAudioCodecSimplexPdm` codec: PDM capture from the
microphone, I2S playback into the MAX98357A, with `SD_MODE` (GPIO45) raised
only while the output channel is running.

The codec is simplex, so there is no playback reference channel. Device-side
AEC (`kAecOnDeviceSide`) therefore cannot engage; requesting it logs
"Device AEC requires a playback reference channel" and leaves AEC off.
Server-side AEC is unaffected.

In practice this does not affect turn-based conversation. While the device is
speaking, `Application` disables voice processing, so it cannot transcribe its
own output; only wake-word detection stays active, for barge-in. A multi-turn
conversation was run on hardware with the device speaking several long
responses: no false wake-word triggers, no self-transcription, and no spurious
state transitions.

### Capture rate

Capture runs at 32 kHz, not the 16 kHz the wake-word engine uses. The ESP32-S3
derives the PDM clock as 64x the sample rate, so 16 kHz would give only about
1.02 MHz - below the MP34DT05-A minimum, where the microphone stays in
power-down and its data line reads as a constant, which is indistinguishable
from absent hardware. 32 kHz gives about 2.05 MHz; the audio service resamples
down to 16 kHz for the detector.

## Build

```bash
python scripts/build.py lilygo/t-circle-s3-v11 --name lilygo-t-circle-s3-v11
```

Vendor reference: <https://github.com/Xinyuan-LilyGO/T-Circle-S3>
