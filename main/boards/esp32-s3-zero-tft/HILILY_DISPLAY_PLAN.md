# Hi Lily 240x240 LCD — screen and data integration contract

Status: **UI feature specification**. Battery icon/threshold is implemented
conditionally; weather/lunar, mascot animation and music lyric transport below
are **not yet end-to-end implemented** on the device. Do not claim these screens
are functional until firmware, Worker/API bridge, and hardware testing pass.

## Screen priorities

| Priority | Screen | Display |
| --- | --- | --- |
| 1 | Critical battery <20% (not charging) | Show "Pin yếu, vui lòng sạc thiết bị" over current screen |
| 2 | Music playing/paused | Song title + artist, actual elapsed/total timeline, current highlighted lyric |
| 3 | Speaking/listening/thinking | Small expressive character/GIF, live microphone or output state and subtitle |
| 4 | Idle | Current local time, weather **for the default place configured by Worker**, today's Vietnamese lunar date |
| 5 | Wi-Fi provisioning | SoftAP SSID and portal URL; screen does not need a keyboard |

Always keep compact Wi-Fi/network icon and **real** battery percentage.
While charging show a lightning/bolt next to percentage. If voltage sensing
is unavailable, display a neutral missing-data indicator instead of inventing %.

## Worker Master 4.3 (existing implementation verified from source file)

Existing:
- `GET /api/weather?location=<place>`: returns
  `{ok, location, current: {temperature_c, weather, ...}, meta}`
- `GET /api/lunar?date=YYYY-MM-DD&timezone=7`: returns
  `{ok, solar, lunar:{day,month,year,leap_month,display},can_chi,...}`
- `GET /stream_pcm?song=...&artist=...` and `/api/music/search?q=...`:
  song metadata `{title,artist,duration,audio_url,lyric_url}`
- `GET /proxy_lyric?path=...`: returns lyric content (may be LRC or JSON).

**Missing**: No `GET /api/device/display` endpoint and no implicit default
location in existing `/api/weather`: with neither `location` nor lat/lon
it requests place clarification. The device must NOT infer location from IP.
The location selection should be stored/owned by the Worker/admin layer, not
hardcoded into the S3 firmware.

**Proposed small endpoint** in Worker:
`GET /api/display/home?device_id=<stable-device-id>`
```json
{
  "ok": true,
  "location": "Tên địa điểm cài mặc định trên Worker",
  "weather": {"temperature_c": 29, "description": "Có mây", "weather_code": 2},
  "lunar": {"day": 5, "month": 9, "year": 2026, "leap_month": false},
  "updated_at": "ISO-8601"
}
```
Numbers here illustrate schema, **not live weather or lunar data**.
Worker should resolve authoritative default via its device/family configuration,
then call existing weather and lunar functions. HTTPS/TLS required. Retry on
error, keep last good data visibly marked as stale; refresh weather every
15–30 minutes, lunar at the next Vietnam-local midnight (TZ UTC+7).
Do not change the active location merely because a weather tool was queried
for another place during conversation.

## Music: required live timing integration

The Go desktop `xiaozhi-go/music_player.go` has its own clock and lyric parser.
The ESP32 firmware does not have the equivalent music player/MP3 decoder yet.
Worker Master returns metadata/proxy URLs, **not PCM frames or timing events**
to the ESP's LVGL UI automatically.

A proper music player or Gateway-to-ESP device state channel must report:
```json
{
  "event": "music_state",
  "track_id": "stable-id",
  "title": "Song title",
  "artist": "Artist",
  "duration_ms": 254000,
  "position_ms": 102500,
  "playing": true,
  "lyric_lines": [
    {"time_ms": 100000, "text": "Example line"},
    {"time_ms": 106000, "text": "Next line"}
  ]
}
```

Use output samples actually consumed by DAC as the time source, or player
decoded-audio timestamp with latency correction; **never start a fake timer
when only music metadata is received**. Seek/pause/resume must update lyrics
without drift. LRC syntax should support `[mm:ss.xx]`, `[mm:ss.xxx]`, and
multiple timestamps per line. If no lyric exists show "Chưa có lời bài hát".
Display `mm:ss / mm:ss` and a short sliding lyric panel. Do not stream
music MP3 bytes inside MCP tool JSON.

## Mascot selection from xiaozhi-go/assets/mascot

- idle.gif — 82,400 bytes
- thinking.gif — 23,688 bytes
- loving.gif — 48,185 bytes
- sad.gif — 23,149 bytes
- angry.gif — 23,435 bytes
- speaking.gif — **1,177,332 bytes** (cannot fit the 1MB assets partition alone)
- qoo-bee-qoo-bee-agapi.gif — 82,400 bytes

Choose a handful of compact animations/selected frames only, rescale to
at most 80–112 physical pixels and budget combined assets together with
WakeNet9 Hi Lily and font resources. Current XiaoZhi emotion icons should
remain an automatic fallback if resource fitting fails. Check GIF decoder
working memory and CPU overhead, rather than just file sizes.

## Hardware-dependent battery support

Opt-in board setting: `CONFIG_HILILY_BATTERY_MONITOR=y`.

- Single Li-ion/LiPo cell VBAT -> 100k resistor -> GPIO10 ADC1 CH9 ->
  100k resistor -> GND; add ~100nF cap GPIO10 to ground for ADC stability
- **LCD DC moves from GPIO10 to GPIO15** when this option is enabled
- Charger module's **3.3V-safe active-low CHRG# status** -> GPIO16;
  open-drain requires a suitable pull-up. Must verify exact module pin polarity.
- The firmware's generic `AdcBatteryMonitor` estimates capacity by voltage;
  it is an approximation under load and requires validation/calibration.
  Do not connect charger battery terminals to ESP GPIO or use a 5V status pin.
- When telemetry is valid: top right icon shows bolt while charging and
  `NN%`; under 20% *and discharging* popup reads:
  **Pin yếu, vui lòng sạc thiết bị**.

Hardware isn't yet specified or attached, so the battery feature remains
**disabled by default**. This avoids a false 100% reading on unconnected ADC.

## Acceptance tests

1. 4MB factory image and assets both fit; WakeNet9 Hi Lily survives 2MB
   Quad PSRAM allocation during speech and UI animations.
2. With Worker location changed, idle weather follows Worker location after
   refresh; lunar date switches at Vietnam local midnight.
3. All state transitions do not leak LVGL objects and keep FPS acceptable.
4. Lyric line follows measured audio position through pause/resume/seek.
5. Battery 21%: no warning; 20%: no warning; 19% discharging: warning;
   charging 19%: lightning and warning dismissed.
6. Battery sensing disconnected/unavailable: don't display fabricated percent.

## Music lyric UI implementation (2026-10-14)

`HiLilyDisplay` now implements `SetMusicTrack(title, artist, duration_ms, lrc)`,
`SetMusicPosition(position_ms, playing)` and `ClearMusic()` for a one-current-line
LRC display, title/artist and `mm:ss / mm:ss` progress. It parses multiple LRC
stamps per line and sorts by timestamp. It **does not** run an independent
clock. The real audio player must call `SetMusicPosition` with actual playback
position after pause, resume and seek. Worker metadata/lyric URLs alone do
not provide that position. The ESP32 PCM music transport and Worker lyric
fetch bridge are **not yet wired**; do not describe this as fully working
music playback. The UI is board-local and does not affect other devices.

## Direct Worker streaming architecture

Firmware has a board-specific local MP3 streaming engine. Device MCP calls
`self.music.play(song,artist)`, `self.music.pause`, `self.music.resume`,
`self.music.stop` enqueue local actions. The same authenticated XiaoZhi
device websocket may deliver `{"type":"music","action":"play","song":"..."}`
without involving an LLM **if the gateway supports STT intent routing**.

- Device HTTP GET `/stream_pcm?song=...&artist=...` (metadata JSON)
- HTTP GET `/proxy_lyric?... ` (time-synced LRC)
- HTTP GET `/proxy_audio?...` (raw MP3; never put audio bytes in MCP JSON)
- On-device Espressif MP3 decoder -> mono 24kHz resample -> PCM5102A I2S
- Update lyric from consumed PCM samples (pause freezes the timeline).
- AudioService Opus output gated while local MP3 uses the DAC.

A local MCP tool *being advertised to Xiaozhi* does NOT mean voice is
automatically routed without DeepSeek. The backend gateway must recognize a
voice music command after STT and forward the `type:music` message directly.
This repository alone cannot replace the STT/server routing.

Use the host in `HILILY_WORKER_BASE_URL`; it must match the deployed Worker.
Worker Master music API needs working upstream provider and lyric content.
No commercial music availability is guaranteed. Flash/RAM and hardware
integration still require real ESP-IDF 6.x build and on-board test.

### Voice command fast-path

Firmware now intercepts finalized incoming XiaoZhi STT text for Vietnamese
patterns such as `phát bài <song>`, `phát bài <song> của <artist>`,
`tạm dừng nhạc`, `tiếp tục nhạc`, and `dừng nhạc`.
It invokes the local MP3 player immediately, bypassing an **LLM tool call on
the device**. This does **not** prevent the upstream XiaoZhi server from also
passing the same utterance to DeepSeek. For guaranteed zero-DeepSeek music
routing, deploy a trusted gateway/server feature that consumes matching
music STT intents before the LLM stage and forwards a direct music event.

## Online music reliability follow-up (2026-10-14)

- Added bounded read-timeout retry (three retries, increasing short delays).
  This is **not** a full reconnect/HTTP Range resume; after retries the player
  exits instead of replaying the beginning or desynchronizing lyrics.
- Pause now sends a frozen position to the LVGL music display. Resume advances
  from the same PCM sample counter.
- MP3 streaming remains online-only: no SD-card player or SD dependencies.
- Further items remain: dedicated producer/consumer ring buffer, radio
  station API/provider verification, spectrum FFT, and arbitration against
  concurrent TTS output. These are not claimed as completed.
- Compile, size, and device testing are required before shipping.
