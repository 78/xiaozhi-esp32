# Realtime Debug

Standalone Go collector and local browser view for XiaoZhi Realtime traces. It does not proxy the Aliyun connection. The Waveshare 3.5B firmware includes a bounded device-side trace sender.

## Run

Requires Go 1.22 or newer. From this directory:

```sh
go run ./cmd/realtime-debug --collector-listen 192.168.1.20:7319
```

Replace `192.168.1.20` with the development computer's LAN address. The browser opens at `http://127.0.0.1:7320`. The device uses `tcp://192.168.1.20:7319`. `--web-listen`, `--data-dir`, and `--max-session-mib` override the defaults. Recordings default to the OS user cache directory under `xiaozhi-realtime-debug`, outside this repository.

After the board joins Wi-Fi, open `http://<device-ip>/`. Under **Debug capture**, enter the collector address, enable capture, and choose level 0 (local events), 1 (events and WebSocket payloads), or 2 (plus PCM). Save and reboot the board. The same page selects XiaoZhi or Aliyun and configures the Aliyun region, workspace ID, and API Key. A blank Key field retains the saved Key; the page never returns its value.

The collector accepts one device connection at a time. The device must send a `hello` frame first. The stream is plain TCP with no pairing token; use it only on a controlled development LAN. The browser listener defaults to loopback. The trace sender must never include Aliyun API keys or HTTP authentication headers.

## Frame protocol

Each frame is `uint32_be length`, followed by `uint16_be version` (`1`), `uint16_be kind`, `uint32_be metadata_length`, UTF-8 JSON metadata, and payload bytes. The maximum frame length is 256 KiB and metadata is limited to 4 KiB. Large messages are split into numbered frames by the sender. See `internal/trace/frame.go` for kinds and metadata fields.

The first `hello` frame needs `boot_id`, `device_id`, and `firmware_version`. Every later frame needs the same `boot_id`, a `trace_session_id`, decimal-string `seq`, and `mono_us`. PCM frames also require `sample_rate`, `channels`, `bits_per_sample`, `sample_format: "signed_int"`, and `byte_order: "little"`. Raw WebSocket payloads retain their exact application-level bytes; `ws_tx_result` reports the local send call result separately.

## Recordings

Each session has `manifest.json`, `events.jsonl`, `payloads.dat`, `pcm-uplink.dat`, and `pcm-playback.dat`. The browser reads event summaries, streams PCM as WAV without rewriting the source data, and exports a session ZIP. Audio format changes create separate WAV segments. A session that reaches `--max-session-mib` is marked truncated. On restart, the service rebuilds summaries from complete event-index lines, trims unindexed payload tails, and marks an unfinished session as interrupted. It does not append new frames to that old trace ID. Automatic retention cleanup is not implemented.

Go service UTs are intentionally not part of this tool. Validate it by running the collector, inspecting a known trace in the browser, and connecting a device with the trace sender enabled. The sender has a 128 KiB queue; overflow creates a visible `trace_gap` marker and voice traffic continues.

For a manual smoke check without hardware, start the collector and run `node scripts/send-sample.mjs 127.0.0.1 7319`. Open the browser page and inspect the sample session, JSON payload, uplink WAV, and ZIP export. This script sends a synthetic trace; it does not contact Aliyun.
