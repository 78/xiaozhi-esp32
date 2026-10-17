# R2D2 mini-jarvis

Room satellite of the R2D2 home agent: the Spotpear ESP32-S3 1.28 Box
hardware (pins unchanged from `spotpear/sp-esp32-s3-1.28-box`) with the R2D2
HUD's look on the round screen and the R2D2 Core as its server.

- `r2d2_display.*`: idle screen (Wi-Fi, clock, date, weather) and the Jarvis
  face during conversations; the stock UI shows through while starting,
  configuring Wi-Fi, activating or upgrading.
- `r2d2_face.*`: port of the HUD face (r2d2 repo, `edge/dashboard/face.py`).
- `r2d2_status.*`: polls `GET /satellite/status` on the Core for weather and
  clock sync.

Build:

```bash
python3 scripts/build.py r2d2/mini-jarvis --name r2d2-mini-jarvis \
    --wake-word wn9_jarvis_tts --language pt-BR
```

The Core side and the integration plan live in the r2d2 repo
(`agent_core/channels/satellite/`, `satellites/esp32s3/README.md`).
