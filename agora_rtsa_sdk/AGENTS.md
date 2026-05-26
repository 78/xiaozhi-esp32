# RTSA SDK Developer Guide

## Overview

This directory contains demo programs for the RTSA (Real-Time media Stream Accelerate) SDK.
Each `hello_xxx` demo demonstrates a specific SDK feature with minimal code, serving as a
starting point for developers to build their own applications.

The only header you need is `agora_rtc_api.h` — it contains all public APIs, data types,
callbacks, and error codes.

## Demo List

| Demo | Feature | Description |
|------|---------|-------------|
| `hello_rtsa` | Audio/Video Streaming | Send and receive audio/video data over Agora SD-RTN. Supports H.264/H.265/JPEG video and PCM/Opus/AAC/G.711/G.722 audio. The most comprehensive demo covering the core SDK workflow. |
| `hello_rtm` | RTM Messaging | Point-to-point messaging via RTM (Real-Time Messaging) channel. Supports both interactive text chat and automated throughput testing with configurable message size/rate. |
| `hello_rdt` | Reliable Data Transfer | Reliable file transfer between peers via RDT channel. Sends files with MD5 verification, progress tracking, and configurable bitrate. |
| `hello_rtcm` | Media Control Message | Send/receive custom control messages within a channel. Supports both broadcast (to all users) and unicast (to specific peer). |
| `hello_stream_message` | Data Stream | Create ordered/reliable data streams for sending structured messages within a channel. |

## Directory Structure

The SDK package ships with the following structure (after packaging by `build-lite.sh`):

```
example/
├── hello_rtsa/                # Core audio/video streaming demo
│   ├── hello_rtsa.c           # Main demo: single connection
│   ├── hello_rtsa_multi.c     # Multi-connection variant
│   ├── app_config.h           # Configuration and argument parsing
│   └── CMakeLists.txt
├── hello_rtm/                 # RTM messaging demo (if enabled)
├── hello_rdt/                 # Reliable data transfer demo (if enabled)
├── hello_rtcm/                # Media control message demo (if enabled)
├── hello_stream_message/      # Data stream demo (if enabled)
├── audio_player/              # ALSA audio playback demo (if enabled)
├── out/
│   └── {ARCH}/                # Built demo binaries and runtime output
├── test_data/                 # Shared test media files
│   ├── send_video.h264        # H.264 test video
│   ├── send_video.h265        # H.265 test video
│   ├── send_audio_16k_1ch.pcm # 16kHz mono PCM audio
│   ├── send_audio_8k_1ch.pcm  # 8kHz mono PCM audio
│   ├── send_audio.opus        # Opus encoded audio
│   ├── send_audio.g722        # G.722 encoded audio
│   ├── send_audio.pcma        # G.711 A-law audio
│   ├── send_audio.pcmu        # G.711 μ-law audio
│   └── send_audio_*.aac       # AAC encoded audio (8k/16k/32k/48k)
├── third-party/               # Pre-built third-party libraries
│   ├── file_parser/           # Media file parser
│   ├── json_parser/           # JSON parser (jsmn)
│   └── build.sh               # Third-party rebuild script
├── scripts/                   # CMake helper scripts
│   ├── env.cmake              # Environment detection
│   ├── toolchain.cmake        # Cross-compilation toolchain
│   └── check.cmake            # Dependency checks
├── build.sh                   # Unified build script (`-a/-f/-t/-b`)
└── CMakeLists.txt             # Top-level CMake (auto-discovers all demo subdirectories)
```

Note: The included demos vary depending on the SDK build configuration. `hello_rtsa` is always
included; other demos (hello_rtm, hello_rdt, hello_rtcm, hello_stream_message, audio_player)
are included only when the corresponding feature is enabled during SDK packaging.

## Build

### Prerequisites

- SDK installed at `../agora_sdk/` (headers in `include/`, libraries in `lib/{ARCH}/`)
- Third-party libraries in `third-party/` (rebuild if needed: `./third-party/build.sh -a <ARCH> -f <TOOLCHAIN> -t <BUILD_TYPE>`)
- CMake >= 2.4, GCC with C99 support

### Build Commands

In the SDK package, all demos are built through the unified `build.sh` script. Build parameters are
passed by command-line options instead of environment variables.

```bash
# Build all demos (uses default toolchain)
./build.sh -a <ARCH>

# Build with custom toolchain
./build.sh -a <ARCH> -f /path/to/toolchain.cmake

# Build with specific type
./build.sh -a <ARCH> -t debug

# Rebuild
./build.sh -a <ARCH> -f /path/to/toolchain.cmake -t release -b rebuild
```

The `CMakeLists.txt` auto-discovers all demo subdirectories containing a `CMakeLists.txt` and
builds them. Built binaries are output to `out/{ARCH}/`, while bundled test media files stay in
`test_data/`.

## SDK API Reference

All APIs are declared in `agora_rtc_api.h`. Include it with:

```c
#include "agora_rtc_api.h"
```

### SDK Lifecycle

Every application follows this lifecycle:

```
agora_rtc_init()                       Initialize SDK (once per process)
  └─► agora_rtc_create_connection()    Create a connection instance
        └─► agora_rtc_join_channel()   Join a channel
              ├─► send/recv loop       Send and receive audio/video/data
              └─► agora_rtc_leave_channel()
        └─► agora_rtc_destroy_connection()
  └─► agora_rtc_fini()                Release all SDK resources
```

### Initialization

```c
// 1. Define event handler
agora_rtc_event_handler_t handler = { 0 };
handler.on_join_channel_success = my_on_join_success;
handler.on_audio_data           = my_on_audio_data;
handler.on_video_data           = my_on_video_data;
handler.on_error                = my_on_error;
// ... register more callbacks as needed

// 2. Configure service options
rtc_service_option_t opt = { 0 };
opt.area_code = AREA_CODE_GLOB;                // Region: AREA_CODE_CN, AREA_CODE_NA, etc.
opt.log_cfg.log_level = RTC_LOG_INFO;
opt.log_cfg.log_path  = "io.agora.rtc_sdk";

// 3. Initialize SDK
int ret = agora_rtc_init(app_id, &handler, &opt);
```

| Field | Type | Description |
|-------|------|-------------|
| `app_id` | `const char *` | Agora App ID |
| `event_handler` | `agora_rtc_event_handler_t *` | Callback handler (see Event Callbacks section) |
| `option` | `rtc_service_option_t *` | Service options: area_code, log config, domain_limit |

### Connection & Channel

```c
// Create connection
connection_id_t conn_id;
agora_rtc_create_connection(&conn_id);

// Configure channel options
rtc_channel_options_t ch_opt = { 0 };
ch_opt.auto_subscribe_audio = true;
ch_opt.auto_subscribe_video = true;
ch_opt.enable_audio_jitter_buffer = true;       // Smooth audio playback
ch_opt.enable_audio_mixer = false;              // Mix all remote audio into one stream

// Audio codec for PCM input (skip if sending encoded audio like AAC/Opus)
ch_opt.audio_codec_opt.audio_codec_type = AUDIO_CODEC_TYPE_OPUS;  // Encode PCM as Opus
ch_opt.audio_codec_opt.pcm_sample_rate  = 16000;
ch_opt.audio_codec_opt.pcm_channel_num  = 1;

// Encryption (optional)
ch_opt.crypto_opt.enable = true;
ch_opt.crypto_opt.mode   = AES_128_GCM2;       // Recommended mode
sprintf(ch_opt.crypto_opt.key, "%s", key_str);
memcpy(ch_opt.crypto_opt.salt, salt_bytes, 32); // Required for GCM2 modes

// RDT (optional)
ch_opt.enable_rdt = true;

// Join channel
agora_rtc_join_channel(conn_id, "my_channel", uid, token, &ch_opt);
// Wait for on_join_channel_success callback before sending data
```

### Sending Audio

```c
audio_frame_info_t info = { 0 };
info.data_type = AUDIO_DATA_TYPE_PCM;           // Or AUDIO_DATA_TYPE_OPUS, AUDIO_DATA_TYPE_AACLC, etc.
agora_rtc_send_audio_data(conn_id, pcm_buf, pcm_len, &info);
```

Supported `audio_data_type_e` values:

| Enum | Codec | Sample Rate |
|------|-------|-------------|
| `AUDIO_DATA_TYPE_PCM` (100) | Raw PCM (requires audio_codec enabled) | Configured via `pcm_sample_rate` |
| `AUDIO_DATA_TYPE_OPUS` (1) | Opus | 16kHz |
| `AUDIO_DATA_TYPE_OPUSFB` (2) | Opus Full Band | 48kHz |
| `AUDIO_DATA_TYPE_PCMA` (3) | G.711 A-law | 8kHz |
| `AUDIO_DATA_TYPE_PCMU` (4) | G.711 μ-law | 8kHz |
| `AUDIO_DATA_TYPE_G722` (5) | G.722 | 16kHz |
| `AUDIO_DATA_TYPE_AACLC_8K` (6) | AAC-LC | 8kHz |
| `AUDIO_DATA_TYPE_AACLC_16K` (7) | AAC-LC | 16kHz |
| `AUDIO_DATA_TYPE_AACLC` (8) | AAC-LC | 48kHz |
| `AUDIO_DATA_TYPE_HEAAC` (9) | HE-AAC | 32kHz |

Built-in audio codec (`audio_codec_type_e`, PCM input only):

| Enum | Codec |
|------|-------|
| `AUDIO_CODEC_DISABLED` (0) | No encoding (pass-through) |
| `AUDIO_CODEC_TYPE_OPUS` (1) | Opus encoder |
| `AUDIO_CODEC_TYPE_G722` (2) | G.722 encoder |
| `AUDIO_CODEC_TYPE_G711A` (3) | G.711 A-law encoder |
| `AUDIO_CODEC_TYPE_G711U` (4) | G.711 μ-law encoder |

### Sending Video

```c
video_frame_info_t info = { 0 };
info.data_type   = VIDEO_DATA_TYPE_H264;        // Or H265, GENERIC_JPEG
info.frame_type  = VIDEO_FRAME_AUTO_DETECT;     // SDK auto-detects key/delta frame
info.frame_rate  = 25;                          // 0 = use real timestamp
info.stream_type = VIDEO_STREAM_HIGH;
agora_rtc_send_video_data(conn_id, frame_buf, frame_len, &info);
```

Supported `video_data_type_e` values:

| Enum | Format |
|------|--------|
| `VIDEO_DATA_TYPE_H264` (2) | H.264 |
| `VIDEO_DATA_TYPE_H265` (3) | H.265 |
| `VIDEO_DATA_TYPE_GENERIC_JPEG` (20) | JPEG |
| `VIDEO_DATA_TYPE_YUV420` (0) | YUV420 |
| `VIDEO_DATA_TYPE_GENERIC` (6) | Generic |

Bandwidth estimation — adjust your encoder bitrate based on network conditions:

```c
// Set initial BWE parameters
agora_rtc_set_bwe_param(conn_id, min_bps, max_bps, start_bps);

// React to network changes via callback
void on_target_bitrate_changed(connection_id_t conn_id, uint32_t target_bps) {
    // Adjust your encoder bitrate to target_bps
}

// Handle key frame requests (triggered by packet loss)
void on_key_frame_gen_req(connection_id_t conn_id, uint32_t uid, video_stream_type_e type) {
    // Generate and send a key frame immediately
}
```

### Receiving Audio & Video

Data is delivered via event callbacks. Register them before `agora_rtc_init()`:

```c
// Receive audio from individual remote users
void on_audio_data(connection_id_t conn_id, uint32_t uid, uint16_t sent_ts,
                   const void *data, size_t len, const audio_frame_info_t *info) {
    // info->data_type tells you the audio format
    // Process or save the audio data
}

// Receive mixed audio (all remote users mixed into one stream)
// Requires: ch_opt.enable_audio_mixer = true
void on_mixed_audio_data(connection_id_t conn_id, const void *data, size_t len,
                         const audio_frame_info_t *info) {
    // Called every 20ms with mixed PCM data
}

// Receive video from remote users
void on_video_data(connection_id_t conn_id, uint32_t uid, uint16_t sent_ts,
                   const void *data, size_t len, const video_frame_info_t *info) {
    // info->data_type, info->frame_type, info->stream_type
    // Process or save the video data
}
```

### Mute Control

```c
// Stop/resume sending local audio/video
agora_rtc_mute_local_audio(conn_id, true);      // Mute local audio
agora_rtc_mute_local_video(conn_id, true);      // Mute local video

// Stop/resume receiving remote audio/video
agora_rtc_mute_remote_audio(conn_id, remote_uid, true);  // uid=0 for all users
agora_rtc_mute_remote_video(conn_id, remote_uid, true);  // uid=0 for all users
```

### RTM (Real-Time Messaging)

RTM provides a separate point-to-point messaging channel, independent of RTC channels.

```c
// Define RTM event handler
agora_rtm_handler_t rtm_handler = { 0 };
rtm_handler.on_rtm_event = my_on_rtm_event;              // Login/logout/kickoff events
rtm_handler.on_rtm_data  = my_on_rtm_data;               // Incoming messages
rtm_handler.on_rtm_send_data_result = my_on_send_result;  // Send confirmation

// Login
agora_rtc_login_rtm(rtm_uid, rtm_token, &rtm_handler);
// Wait for on_rtm_event with RTM_EVENT_TYPE_LOGIN

// Send message (max 31KB, up to 60 msgs/sec)
agora_rtc_send_rtm_data(peer_rtm_uid, msg, msg_len, msg_id, custom_type);

// Logout
agora_rtc_logout_rtm();
```

RTM event types (`rtm_event_type_e`): `RTM_EVENT_TYPE_LOGIN` (0), `RTM_EVENT_TYPE_KICKOFF` (1), `RTM_EVENT_TYPE_EXIT` (2).

### RDT (Reliable Data Transmission)

RDT provides reliable peer-to-peer data tunnels within a channel. Enable via `ch_opt.enable_rdt = true`.

```c
// Send data (two stream types)
// RDT_STREAM_CMD:  Reliable, high priority, max 256 bytes/pkt, 100 pkt/s
// RDT_STREAM_DATA: Reliable, congestion-controlled, max 1024 bytes/pkt
agora_rtc_send_rdt_msg(conn_id, remote_uid, RDT_STREAM_DATA, data, len);

// Query tunnel status
rdt_status_info_t info;
agora_rtc_get_rdt_status_info(conn_id, remote_uid, &info);
// info.state: RDT_STATE_CLOSED / OPENED / BLOCKED / PENDING / BROKEN
```

RDT callbacks:
- `on_rdt_state(conn_id, uid, state)` — Tunnel state changed
- `on_rdt_msg(conn_id, uid, type, msg, len)` — Received RDT message

### RTCM (Media Control Message)

Send custom control messages within a channel (max 1024 bytes).

```c
// Broadcast to all users in channel (remote_uid = 0)
agora_rtc_send_media_ctrl_msg(conn_id, 0, payload, length);

// Unicast to specific user
agora_rtc_send_media_ctrl_msg(conn_id, peer_uid, payload, length);
```

Callback: `on_media_ctrl_msg(conn_id, uid, payload, length)`.

### Data Stream

Create ordered/reliable data streams for structured messaging (max 1KB/msg, 60 PPS, 6 Kbps).

```c
int stream_id;
agora_rtc_create_data_stream(conn_id, &stream_id, true /* reliable */, true /* ordered */);
agora_rtc_send_stream_message(conn_id, stream_id, data, length);
```

Callback: `on_stream_message(conn_id, uid, stream_id, data, length, sent_ts)`.

### Utility APIs

| API | Description |
|-----|-------------|
| `agora_rtc_get_version()` | Get SDK version string |
| `agora_rtc_err_2_str(err)` | Convert error code to human-readable string |
| `agora_rtc_set_log_level(level)` | Set log level (RTC_LOG_DEBUG ~ RTC_LOG_EMERG) |
| `agora_rtc_config_log(size, count)` | Configure log file size and rotation count |
| `agora_rtc_renew_token(conn_id, token)` | Renew token before expiration |
| `agora_rtc_set_params(conn_id, json)` | Set private parameters via JSON string |
| `agora_rtc_get_connection_info(conn_id, info)` | Get connection info (uid, channel_name) |

### Event Callbacks Summary

Register callbacks in `agora_rtc_event_handler_t` before calling `agora_rtc_init()`:

| Callback | Trigger |
|----------|---------|
| `on_join_channel_success` | Successfully joined channel (safe to send data) |
| `on_connection_lost` | Disconnected from server for >10s |
| `on_reconnecting` | Connection interrupted, SDK is reconnecting |
| `on_rejoin_channel_success` | Reconnected after disconnection |
| `on_user_joined` | Remote user joined the channel |
| `on_user_offline` | Remote user left (quit/dropped/became audience) |
| `on_user_mute_audio` | Remote user muted/unmuted audio |
| `on_user_mute_video` | Remote user muted/unmuted video |
| `on_audio_data` | Received audio frame from remote user |
| `on_mixed_audio_data` | Received mixed audio (requires audio_mixer) |
| `on_video_data` | Received video frame from remote user |
| `on_target_bitrate_changed` | Network bandwidth changed, adjust encoder bitrate |
| `on_key_frame_gen_req` | Remote requests key frame (packet loss detected) |
| `on_token_privilege_will_expire` | Token is about to expire, call `renew_token` |
| `on_media_ctrl_msg` | Received RTCM message |
| `on_rdt_state` | RDT tunnel state changed |
| `on_rdt_msg` | Received RDT message |
| `on_stream_message` | Received data stream message |
| `on_error` | Runtime error occurred |
| `on_rtc_stats` | Periodic connection statistics |

### Error Codes

All APIs return `int`: `0` = success, `< 0` = failure. Use `agora_rtc_err_2_str(err)` to get the error message.

Key error codes (`agora_err_code_e`):

| Code | Name | Description |
|------|------|-------------|
| 0 | `ERR_OKAY` | Success |
| 1 | `ERR_FAILED` | General error |
| 2 | `ERR_INVALID_PARAM` | Invalid argument |
| 3 | `ERR_INVALID_STATE` | Invalid state (e.g. send while muted) |
| 7 | `ERR_NOT_INITIALIZED` | SDK not initialized |
| 101 | `ERR_INVALID_APP_ID` | Invalid App ID |
| 109 | `ERR_TOKEN_EXPIRED` | Token expired |
| 110 | `ERR_INVALID_TOKEN` | Invalid token |
| 300 | `ERR_VIDEO_SEND_OVER_BANDWIDTH_LIMIT` | Sending video too fast |

## Running Demos

Binaries are located in `out/{ARCH}/` (e.g. `out/x86_64/`, `out/aarch64/`). Test media files are
located in `test_data/`. `hello_rtsa` must be launched from the `out/` directory so its default
`../test_data/...` paths resolve correctly.

### hello_rtsa — Audio/Video Streaming

Default files: video=`send_video.h264`, audio=`send_audio_16k_1ch.pcm` (PCM 16kHz mono).
When audio type is PCM with G.711 codec, defaults to `send_audio_8k_1ch.pcm`.
If no file path is provided, `hello_rtsa` uses fixed relative paths under `../test_data/`, so run
it from the `out/` directory.

Parameters:

| Short | Long | Description |
|-------|------|-------------|
| `-i` | `--app-id` | App ID (required) |
| `-t` | `--token` | Authentication token |
| `-c` | `--channel-id` | Channel name, default `hello_demo` |
| `-u` | `--user-id` | User ID, default 0 |
| `-U` | `--user-name` | User name (string UID) |
| `-l` | `--license` | License value |
| `-L` | `--log-level` | Log level, -1 to disable |
| `-v` | `--video-type` | Video type: 2=H264, 3=H265, 20=JPEG |
| `-a` | `--audio-type` | Audio type: 3=PCMA 4=PCMU 5=G722 6=AACLC-8K 7=AACLC-16K 8=AACLC-48K 9=HEAAC 100=PCM |
| `-C` | `--audio-codec` | Audio codec (PCM only): 0=Disable 1=OPUS 2=G722 3=G711A 4=G711U |
| `-f` | `--fps` | Video frame rate, default 25 |
| `-s` | `--send-video-file` | Video file path |
| `-S` | `--send-audio-file` | Audio file path |
| `-A` | `--area` | Area code (hex), default 0xFFFFFFFF (global) |
| `-j` | `--audio-jitter-buffer` | Enable audio jitter buffer |
| `-m` | `--audio-mixer` | Enable audio mixer |
| `-d` | `--audio-decode` | Enable audio decode |
| `-R` | `--recv-only` | Receive only, do not send |
| `-D` | `--domain-limit` | Enable domain limit |
| | `--pcm-sample-rate` | PCM sample rate |
| | `--pcm-channel-num` | PCM channel number |
| | `--pcm-duration` | PCM frame duration (ms), default 20 |
| | `--lan-accelerate` | Enable LAN accelerate |
| | `--audio-ai-qos` | Enable audio AI QoS |

```bash
# Long params
cd out
./{ARCH}/hello_rtsa --app-id <APPID> --channel-id <CHANNEL> --user-id <UID> \
    --send-video-file ../test_data/send_video.h264 --send-audio-file ../test_data/send_audio_16k_1ch.pcm

# Short params (equivalent)
cd out
./{ARCH}/hello_rtsa -i <APPID> -c <CHANNEL> -u <UID> \
    -s ../test_data/send_video.h264 -S ../test_data/send_audio_16k_1ch.pcm

# Receive only
cd out
./{ARCH}/hello_rtsa -i <APPID> -c <CHANNEL> -u <UID> -R

# Send H.265 video with PCM/Opus audio (uses default audio file)
cd out
./{ARCH}/hello_rtsa -i <APPID> -c <CHANNEL> -v 3 -a 100 -C 1 \
    -s ../test_data/send_video.h265
```

### hello_rtm — RTM Messaging

Default send size: 1024 bytes, default send rate: 80 kbps (10 msgs/s).

Parameters:

| Short | Long | Description |
|-------|------|-------------|
| `-i` | `--appId` | App ID (required) |
| `-t` | `--token` | Authentication token |
| `-l` | `--license` | License value |
| `-L` | `--log` | SDK log directory |
| `-u` | `--rtmUid` | Self RTM UID (required) |
| `-p` | `--peerUid` | Peer RTM UID (required) |
| `-M` | `--linkMode` | Link mode: 1=TCP 2=TCP_TLS 3=AUT 4=AUT_TLS |
| `-T` | `--testMode` | Test mode: 1=recv only, 2=send+recv |
| `-S` | `--sendSize` | Send data size (bytes), default 1024 |
| `-K` | `--sendKbps` | Send bitrate (kbps), default 80 |
| `-C` | `--sendCnts` | Send count, default -1 (infinite) |

```bash
# Long params — interactive text chat
./out/{ARCH}/hello_rtm --appId <APPID> --rtmUid <SELF_UID> --peerUid <PEER_UID>

# Short params (equivalent)
./out/{ARCH}/hello_rtm -i <APPID> -u <SELF_UID> -p <PEER_UID>

# Throughput test
./out/{ARCH}/hello_rtm -i <APPID> -u <SELF_UID> -p <PEER_UID> -T 2 -S 1024 -K 100
```

### hello_rdt — Reliable Data Transfer

Default channel: `demo`, default send file: `hello_rdt`, default bitrate: 1000 kbps.

Parameters:

| Short | Long | Description |
|-------|------|-------------|
| `-i` | `--app-id` | App ID (required) |
| `-t` | `--token` | Authentication token |
| `-c` | `--channel-id` | Channel name, default `demo` |
| `-u` | `--local-id` | Local UID (required) |
| `-p` | `--peer-id` | Peer UID (required) |
| `-s` | `--send` | Enable send mode |
| `-k` | `--send-kbps` | Send bitrate (kbps), default 1000 |
| `-f` | `--send-file` | File to send, default `hello_rdt` |
| `-r` | `--send-repeat` | Repeat sending when file ends |
| `-l` | `--log-dir` | SDK log directory |
| `-L` | `--log-level` | Log level (1-8) |
| | `--crypto-mode` | Encryption mode |
| | `--crypto-key` | Encryption key |
| | `--crypto-salt` | Encryption salt |
| | `--lan-accelerate` | Enable LAN accelerate |

```bash
# Long params — send file
./out/{ARCH}/hello_rdt --app-id <APPID> --channel-id <CHANNEL> \
    --local-id <UID> --peer-id <PEER_UID> --send --send-file <FILE_PATH>

# Short params (equivalent)
./out/{ARCH}/hello_rdt -i <APPID> -c <CHANNEL> -u <UID> -p <PEER_UID> -s -f <FILE_PATH>

# Receive mode
./out/{ARCH}/hello_rdt -i <APPID> -c <CHANNEL> -u <UID> -p <PEER_UID>
```

### hello_rtcm — Media Control Message

Default channel: `demo`, default message: `hello agora`.

Parameters:

| Short | Long | Description |
|-------|------|-------------|
| `-i` | `--app-id` | App ID (required) |
| `-t` | `--token` | Authentication token |
| `-c` | `--channel-id` | Channel name, default `demo` |
| `-u` | `--local-id` | Local UID (required) |
| `-p` | `--peer-id` | Peer UID (required) |
| `-s` | `--send` | Send message content |
| `-r` | `--receive` | Enable receive |
| `-l` | `--log-dir` | SDK log directory |

```bash
# Long params
./out/{ARCH}/hello_rtcm --app-id <APPID> --channel-id <CHANNEL> \
    --local-id <UID> --peer-id <PEER_UID> --send "hello agora"

# Short params (equivalent)
./out/{ARCH}/hello_rtcm -i <APPID> -c <CHANNEL> -u <UID> -p <PEER_UID> -s "hello agora"
```

### hello_stream_message — Data Stream

Default send size: 4 bytes, default PPS: 10.

Parameters:

| Short | Long | Description |
|-------|------|-------------|
| `-i` | `--app-id` | App ID (required) |
| `-t` | `--token` | Authentication token |
| `-c` | `--channel-id` | Channel name (required) |
| `-u` | `--user-id` | User ID, default 0 |
| `-L` | `--log-level` | Log level, -1 to disable |
| `-R` | `--recv-only` | Receive only |
| | `--ordered` | Create ordered data stream |
| | `--reliable` | Create reliable data stream |
| | `--log_path` | Log path |
| | `--send_size` | Message size (bytes), default 4 |
| | `--pps` | Packets per second, default 10 |

```bash
# Long params
./out/{ARCH}/hello_stream_message --app-id <APPID> --channel-id <CHANNEL> --user-id <UID> \
    --ordered --reliable --send_size 64 --pps 10

# Short params
./out/{ARCH}/hello_stream_message -i <APPID> -c <CHANNEL> -u <UID>

# Receive only
./out/{ARCH}/hello_stream_message -i <APPID> -c <CHANNEL> -u <UID> -R
```

## Developing Your Own Application

1. Copy `hello_rtsa/` as a template for your project
2. Include `agora_rtc_api.h` from `agora_sdk/include/`
3. Link against `libagora-rtc-sdk.so` (or `.a` for static linking)
4. Follow the SDK lifecycle: init → create_connection → join → send/recv → leave → destroy → fini
5. Register event callbacks for the features you need (audio/video, RTM, RDT, RTCM, data stream)
6. Use test media files in `test_data/` for initial testing

### Quick Start Code

```c
#include "agora_rtc_api.h"

int main() {
    // Init
    agora_rtc_event_handler_t h = { 0 };
    h.on_join_channel_success = on_join;
    h.on_audio_data = on_audio;
    h.on_video_data = on_video;
    h.on_error = on_error;

    rtc_service_option_t opt = { 0 };
    opt.area_code = AREA_CODE_GLOB;
    agora_rtc_init("YOUR_APP_ID", &h, &opt);

    // Create connection and join
    connection_id_t conn;
    agora_rtc_create_connection(&conn);

    rtc_channel_options_t ch = { 0 };
    ch.auto_subscribe_audio = true;
    ch.auto_subscribe_video = true;
    agora_rtc_join_channel(conn, "test_channel", 0, NULL, &ch);

    // ... wait for on_join_channel_success, then send/recv in a loop ...

    // Cleanup
    agora_rtc_leave_channel(conn);
    agora_rtc_destroy_connection(conn);
    agora_rtc_fini();
    return 0;
}
```
