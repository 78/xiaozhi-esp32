# MQTT + UDP PING/PONG Extension Proposal

[简体中文](mqtt-udp-probe_zh.md) | [Base MQTT + UDP protocol](mqtt-udp.md)

**Status: client and gateway server implemented; normal-path hardware validation passed.**
Probes are enabled only after bilateral negotiation. Packet-loss, blocked-UDP and legacy fallback
fault injection still require hardware validation; the acceptance checklist is not a record of completed tests.

## 1. Purpose and scope

A successful local UDP `Connect()` or `Send()` does not establish that the audio server can exchange datagrams with the device. MQTT can remain connected while UDP is blocked. The extension adds client-initiated, authenticated UDP probes to detect this condition when opening an audio channel and throughout its lifetime.

The design keeps the existing audio format and MQTT protocol version `3`. Probe verification runs concurrently with the existing wake-word and conversation flow. It does not require an acknowledgment or retransmission for every audio packet, and it does not add automatic WebSocket fallback. A successful probe verifies a UDP round trip; it does not verify audio decoding, speaker playback, every packet size, or sustained stream quality.

## 2. Capability negotiation

Both hello messages must explicitly contain the JSON boolean `features.udp_probe: true` to enable probes for that session. A missing field or `false` selects the existing behavior. If `features` is present, it must be an object; if `udp_probe` is present, it must be a boolean. Invalid types are negotiation errors, not a reason to silently select legacy behavior.

The following examples show the relevant fields. Other existing hello fields and features remain unchanged.

Device hello:

```json
{
  "type": "hello",
  "version": 3,
  "transport": "udp",
  "features": {
    "mcp": true,
    "udp_probe": true
  },
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1,
    "frame_duration": 60
  }
}
```

Server hello:

```json
{
  "type": "hello",
  "transport": "udp",
  "session_id": "example-session",
  "features": {
    "udp_probe": true
  },
  "audio_params": {
    "format": "opus",
    "sample_rate": 24000,
    "channels": 1,
    "frame_duration": 60
  },
  "udp": {
    "server": "udp.example.com",
    "port": 8888,
    "key": "000102030405060708090a0b0c0d0e0f",
    "nonce": "01000000112233440000000000000000"
  }
}
```

The key and nonce above are test values, not values to reuse in deployed sessions. This extension introduces no additional hello fields besides `features.udp_probe`.

A server selects the extension only after receiving the device's explicit `true`. Once both parties have selected it, probe failure must not silently downgrade that session to legacy mode.

## 3. UDP packet format

PING and PONG use a fixed 32-byte datagram: a 16-byte header followed by a 16-byte authentication tag. All integer fields use network byte order. The SSRC is copied as four raw bytes.

| Offset | Size | Field | Definition |
| --- | --- | --- | --- |
| 0 | 1 | `type` | `0x02` for PING; `0x03` for PONG |
| 1 | 1 | `flags` | Must be `0` |
| 2 | 2 | `payload_len` | Unsigned 16-bit integer; must be `16` |
| 4 | 4 | `ssrc` | Bytes `[4:8]` of the hex-decoded `udp.nonce` |
| 8 | 4 | `reserved` | Must be four zero bytes |
| 12 | 4 | `probe_seq` | Unsigned 32-bit probe sequence, starting at `1` |
| 16 | 16 | `auth_tag` | First 16 bytes of HMAC-SHA256 over the header |

For probe packets, bytes 8–11 are reserved; they are not an audio timestamp or acknowledgment. There is no `ack_seq`, challenge, additional version field, or additional key.

The existing `type = 0x01` audio packet remains unchanged. Probe packets do not enter the AES-CTR encryption/decryption path, the Opus decoder, or audio sequence-number and loss accounting.

## 4. HMAC and session material

```text
key    = hex_decode(udp.key)             # existing 16-byte audio key
header = datagram[0:16]
tag    = HMAC-SHA256(key, header)[0:16]
```

The HMAC input is exactly the 16 header bytes, with no prefix. There is no key derivation or separately exchanged probe key. The packet type is covered by the MAC, so a PING cannot be accepted as a PONG by changing only its type byte.

Compare tags in constant time. Authentication failure or a malformed packet must not update health, sequence, session-routing, or peer-address state.

### 4.1 Session and sequence rules

- The server must make SSRC routing unambiguous among active sessions at the same UDP endpoint. The received SSRC identifies the candidate session and existing key; a valid MAC is still required.
- The client maintains a probe counter independent of the audio counter. The first round uses `1`; each new round increments it by one. A retransmission keeps the same sequence and identical packet bytes.
- Only one probe round may be pending per channel. Start a new round only after the previous round succeeds. A failed round closes the channel.
- Re-establish the session before an increment would exceed `UINT32_MAX`; never wrap the probe counter to zero.
- Every reset of the probe counter requires a new `(key, SSRC)` combination. A newly negotiated session must receive fresh key/nonce material; changing only the JSON `session_id` does not isolate old UDP packets.
- Local generation checks protect callbacks and queued work. They do not replace the on-wire key/SSRC separation between sessions.

## 5. Requests, responses, and duplicates

### 5.1 Server processing

The client sends PING through the same UDP socket and destination IP/port used for audio. The server replies from that same server IP/port to the source address and port observed on the authenticated request. It responds immediately, without waiting for ASR, LLM, or TTS processing. Probes must not change the existing audio destination-address binding.

For each PING, the server:

1. Checks the exact 32-byte length, type, zero flags, `payload_len = 16`, zero reserved bytes, nonzero sequence, and active SSRC routing.
2. Verifies the HMAC using that session's existing key before changing state.
3. Accepts a new sequence, or a retransmission of the current sequence. Older sequences may be discarded.
4. Copies the 16-byte PING header, changes only `type` from `0x02` to `0x03`, recomputes the tag, and sends the PONG.

A duplicate current PING must still receive a PONG: the previous response may have been lost. Processing a probe must not create a conversation, replay a wake notification, or change audio sequence tracking.

Sending a PONG does not prove that the device received it. Because the client advances to a new probe sequence only after success, a later authenticated PING with a new sequence indirectly indicates that the preceding round succeeded. Absence of a new sequence is not, by itself, proof of downlink failure: the device may have closed or stopped scheduling the session.

### 5.2 Client processing

Publish the pending header, sequence, generation, and deadline before the first send attempt, so a fast response cannot arrive before the pending state exists.

At the UDP callback boundary, dispatch PONG before acquiring `channel_mutex_`, including the
audio generation gate. `ReceivePong()` claims reception and records the monotonic time before its
first lock wait. Under the lock it checks that the callback's captured probe channel is still the
current channel, then validates the round and MAC. Closing or reopening replaces that identity, so
an old callback cannot revive a channel or complete a new round. Audio keeps its generation gate.

A PONG completes the round only when:

- The channel generation is still current and the round is still pending.
- It arrives before the round's monotonic deadline.
- It has exactly 32 bytes and the required field values.
- Its header, apart from the type byte, exactly equals the pending PING header.
- Its tag is valid under the current session key.

On success, clear the pending round, mark the channel healthy, and schedule the next round. A duplicate completed PONG, stale session response, unsolicited response, or response after cancellation must not refresh health. In particular, retransmitting a PING does not invalidate a valid response to its earlier transmission. Discard other invalid packets without immediately terminating the conversation for one bad packet.

Each successfully authenticated, matching PONG logs one timing sample at INFO level:

```text
UDP RTT: 42.137 ms
UDP RTT: unknown after retransmission (attempts=2, elapsed=1100.123 ms)
```

An unretransmitted round reports milliseconds with three decimal places, measured from the first
send attempt to the receive callback's monotonic timestamp. This includes local dispatch/HMAC,
server response processing, and modem AT overhead where applicable. A retransmitted round instead
reports the attempt count and elapsed time since the first PING: its response cannot be attributed
to a particular attempt, so that elapsed time is not labeled as a valid RTT. Duplicate, invalid,
expired, and old-session PONGs do not produce samples. Logging occurs after releasing the channel
lock, and uses the existing timing state without changing the packet format.

## 6. Channel state and concurrent wake flow

Socket readiness and verified health are separate concepts. Preserve `OpenAudioChannel()`, `IsAudioChannelOpened()`, and `OnAudioChannelOpened()` as the existing indication that the socket can carry business traffic. These APIs do not certify bidirectional UDP reachability.

| Health state | Business traffic allowed | Meaning |
| --- | --- | --- |
| `CLOSED` | No | No audio channel |
| `NEGOTIATING` | No | Waiting for hello/UDP socket setup |
| `VERIFYING` | Yes | Socket ready; first probe is pending |
| `HEALTHY` | Yes | Last round succeeded, including a periodic probe's first wait before timeout |
| `SUSPECT` | Yes | A wait timed out and the pending round is being retried |
| `FAILED` | No | The probe round's total deadline expired |
| `LEGACY_UNVERIFIED` | Yes | Probe support was not negotiated |

Traffic allowance is also subject to the existing channel checks, including the receive timeout.

After socket setup, install the first pending round and send its PING once in the open path, without waiting for PONG. Continue the existing audio upload, MQTT wake/listen notifications, AI processing, and downlink audio without waiting for PONG. Preserve the existing business ordering: wake-word audio upload precedes the MQTT wake notification when that upload is enabled.

```mermaid
sequenceDiagram
    participant Device as Device
    participant Control as MQTT control service
    participant Audio as UDP audio service

    Note over Device: Wake detected; start wake-audio encoding
    Device->>Control: hello, features.udp_probe = true
    Control-->>Device: hello, udp_probe = true, existing UDP parameters
    Note over Device: Socket ready; publish pending probe; VERIFYING
    Device->>Audio: First PING(seq = 1), sent once before Open returns
    par Asynchronous UDP response
        Audio-->>Device: PONG(seq = 1)
        Note over Device: Verify response; HEALTHY
    and Existing conversation flow
        Device->>Audio: Wake-word audio, if enabled
        Device->>Control: Existing wake/listen messages
        Note over Control,Audio: Existing AI processing
        Control-->>Device: Existing TTS/control messages
        Audio-->>Device: Existing downlink audio
    end
    Note over Device: After successful probe, schedule next round
    Device->>Audio: PING(seq = 2), while channel remains open
    Audio-->>Device: PONG(seq = 2)
```

This is logical concurrency; it does not require two simultaneous socket sends. A 4G modem may serialize transmission through its AT command channel.

All transports use one-shot `esp_timer` scheduling, with no dedicated probe task or task stack.
There are exactly two processing entry points: the initial PING in `OpenAudioChannel()` once the
socket is ready, and subsequent coalesced timer events handled by the main task. `SendAudio()` and
`SendText()` do not call `ProcessPending()` or alter probe scheduling. Sends already share the main
task, so no additional send mutex or busy-sender retry loop is needed. Timers track only retries,
round deadlines, and periodic rounds; they introduce no business-idle clock or reclamation policy.
On C3 this avoids the previous 4 KB probe task stack and its task control block; timer and session
state still use memory.

No path waits for PONG before continuing business traffic. Individual sends retain the existing
synchronous UDP interface: a 4G send may wait for its AT response, just as an audio send does.
That wait never runs in the timer callback. Main-task dispatch and send latency may delay timeout
reporting; response acceptance still uses the round's absolute monotonic deadline.

If `CONFIG_SEND_WAKE_WORD_DATA` is disabled, verification remains asynchronous; there is simply no wake-audio upload to overlap. Reusing an already open channel does not force a new synchronous PING/PONG exchange for each wake. An in-progress probe never causes the application to replay its wake or conversation messages.

## 7. Retry and periodic checks

The following are proposed product defaults, not STUN protocol requirements. Initial and periodic rounds use the same retry state machine.

Set `t0` from a monotonic clock immediately before the first transmission attempt. The pending state must already exist. Use one fixed deadline for the entire round:

| Time from `t0` | Action if no valid PONG has completed the round |
| --- | --- |
| 0 s | Send the initial PING |
| 1 s | Retransmit the identical PING |
| 3 s | Retransmit the identical PING |
| 5 s | Fail the round |

Retransmissions do not restart the deadline or allocate a new sequence. A valid response ends the round immediately; subsequent scheduled retries become no-ops. Do not accumulate retry jobs while a previous send is in progress. Check the deadline before starting queued work and ignore stale send results after the generation changes. Local send failure is not a remote acknowledgment; retry within the remaining budget. The receive callback records the monotonic receive time. PONG success and final timeout must be arbitrated under synchronization: a validated response whose recorded receive time is before the deadline must prevent timeout closure, regardless of the order in which the main task processes events.

After success, schedule the next round after `15 s + U(0, 1.5 s)`, using independent positive jitter per round. A client configuration may choose a 30-second base interval with the same 0–10% jitter, yielding 30–33 seconds, without adding a wire field. A periodic round retains `HEALTHY` during its first wait, enters `SUSPECT` after a wait times out, and returns to `HEALTHY` on success.

The first timeout triggers retransmission, not conversation teardown. Only exhausting the round deadline triggers probe failure. With a 15-second base interval and a 5-second budget, failure detection can take about 20 seconds after the last success, plus jitter and scheduling delays; a 30-second interval gives about 35 seconds before those additions. Actual Wi-Fi and 4G timing must be measured.

Continue periodic checks during playback, capture, and silence while the UDP channel exists. Closing the audio channel stops all probes. Business traffic may begin before the first successful probe; when UDP is blocked, the conversation may therefore start briefly, but the initial deadline bounds that period before failure is reported.

## 8. Timeout, idle tracking, and teardown

Only a valid PONG completes a UDP probe round; MQTT traffic and audio cannot substitute for it.
The original `Protocol::IsTimeout()` behavior remains unchanged: incoming MQTT/control or audio
updates `last_incoming_time_`, while outgoing traffic and PONGs do not. `IsAudioChannelOpened()`
continues to apply that existing 120-second receive-timeout predicate for both legacy and probed
channels, in addition to probe failure. Channel reclamation remains with the original lifecycle and
callers; probing adds no separate business-activity clock or automatic idle-close timer. Probe
scheduling stops when the channel is actually closed.

On deadline failure, report `udp_probe_timeout` once and close the channel directly in the main-task
handler. Do not enqueue another `Application::Schedule()` for this work. Existing application
callbacks still drive the idle state through `SetDeviceState()` and preserve the error notification.
A local HMAC computation failure likewise closes directly with `udp_probe_auth_error`, without
claiming a network timeout or downgrading. Follow existing goodbye behavior: send one goodbye for
client-initiated closure, and do not echo goodbye after server-initiated closure. A probe timeout
means verification failed; it does not prove a firewall was the cause or every UDP packet was blocked.

This proposal does not automatically retry the business conversation, silently downgrade an already negotiated session, or switch to WebSocket. Transport fallback requires a separate design and usable server configuration.

## 9. Compatibility and rollout

| Device | Server response | Session behavior |
| --- | --- | --- |
| New, advertises `true` | New, explicitly selects `true` | Enable PING/PONG and health deadlines |
| New, advertises `true` | Field missing or `false` | Existing flow; `LEGACY_UNVERIFIED`; send no probes |
| Old, does not advertise support | New server | Existing flow; server does not require probes |
| Either peer sends an invalid feature type, or existing UDP parameters are invalid | Malformed negotiation | Negotiation error; do not reinterpret as legacy |
| Both selected `true`, but probes time out | Negotiated extension fails | Fail the session; do not downgrade to legacy |

Legacy compatibility preserves current operation but cannot provide this reachability guarantee. Deploy optional server support first, followed by devices that advertise it. Keeping version `3` and the unchanged audio format allows each session to select the extension independently.

## 10. Implementation boundaries

- **`main/protocols/mqtt_protocol.*`:** negotiate the boolean capability, hold the separate health state/counter/pending packet/deadline/generation, dispatch probe types before audio parsing, perform HMAC, schedule probes, and report failure. Keep audio sequence numbers and AES-CTR behavior unchanged.
- **`main/application.*`:** retain socket-ready semantics and wake business ordering; dispatch the coalesced protocol event through `ProcessPending()` in the existing sender. Handle probe failure directly in that main-task handler, with runtime state changes through the existing `SetDeviceState()` callbacks.
- **Protocol/shared behavior:** do not change the meaning of shared MQTT/WebSocket control messages. If shared interfaces or idle-timeout code need modification, verify the WebSocket path too.
- **Server:** add opt-in negotiation, unique active SSRC routing, authenticated idempotent PING handling, and replies from the existing audio endpoint.
- **Documentation:** keep this proposal and the base MQTT/UDP documents synchronized when implementation lands. Do not describe a local socket connect as a remote UDP handshake.

Guard session-specific receive callbacks and deferred results with the generation or channel identity.
Timer events carry no old-session data and only re-evaluate the current channel, so a stale timer event
cannot replay an old packet. Closing invalidates the generation first, then cancels the timer and clears
pending state before destroying the channel. A late receive callback must not update a newly opened
session. Process a server hello only while waiting for the current open attempt's response; a repeated
hello must not reset an active probe session.

That receive window is not on-wire request correlation: it cannot identify an old hello response that happens to arrive during a new window. The base hello flow must still prevent overlapping or interleaved request/response exchanges for the same device, so an old response cannot reinstall a previously used `(key, SSRC)` combination. A local generation cannot resolve that ambiguity. This extension adds no hello request ID.

UDP/AT receive callbacks and timer callbacks must not perform a blocking send, close the socket, or wait for another task. Receive callbacks validate bounded state; the timer callback only sets an application event and retains no protocol pointer. Reuse the existing application/network-send context for both Wi-Fi and modem sends, and never hold `channel_mutex_` across `Send()`. Keep only bounded pending work and fixed-size probe storage; probes must not create unbounded queues or repeated large audio-path allocations. The shared `Protocol::ProcessPending()` defaults to a no-op for transports such as WebSocket.

Firmware may need an HMAC-compatible PSA key object containing the same existing 16-byte key material, because the AES-CTR key object's attributes are cipher-specific. This does not introduce a new exchanged or derived key. Do not run probe packets through the CTR operation. No vendor/component changes are required by this design; implement the extension in the owning protocol/application layers.

## 11. Acceptance criteria

These checks cover the complete extension. Client host tests exercise retries, deadlines, packet
matching, and authenticated PONG processing; server and physical integration checks remain pending.

| Area | Required cases and expected behavior |
| --- | --- |
| Encoding | Match both 32-byte reference vectors and network byte order |
| Validation | Reject wrong length/type/flags/payload length/reserved bytes/SSRC, sequence zero, and altered tags without changing health |
| Authentication | Reject a reflected PING and a PING whose type is changed without recomputing its tag |
| Initial concurrency | Exactly one initial PING at socket readiness before wake upload; no PONG wait and no probe processing in audio/control send functions |
| Wake configuration | Verify wake-audio enabled and disabled configurations, plus reuse of an already open channel |
| Loss and reordering | Drop initial PING, initial PONG, and multiple packets; accept a valid response to an earlier transmission before the deadline |
| Callback lock contention | Run the registered UDP callback while holding the channel lock across the deadline; force timeout processing before releasing the receiver; accept a pre-deadline PONG, reject a truly late one, and ignore old blocked callbacks after close/reopen |
| Duplicates | Duplicate current PING receives a PONG; duplicate completed PONG does not extend health; one wake creates only one conversation |
| Failure isolation | MQTT remains active while UDP uplink, downlink, or both are blocked; only valid PONG refreshes health |
| Periodic operation | Capture, long playback, and silence retain periodic checks; valid PONGs do not bypass or renew the existing receive-timeout predicate; no new automatic idle close |
| Lifecycle | Close/reopen, server goodbye, network changes, cancellation, and destruction invalidate old callbacks and pending responses |
| Session separation | Counter reset with fresh material rejects old-session PONGs; sequence exhaustion cannot wrap |
| Compatibility | Cover old/new device and server combinations, explicit `false`, missing fields, malformed types, and negotiated timeout without downgrade |
| Scheduling | Verify Wi-Fi and 4G AT serialization, delayed sends, bounded work queues, and timely failure handling |
| Timer and SRAM | No dedicated probe task; one-shot deadlines and fast-PONG rearming; stale timer events after close are harmless; measure minimum free heap and existing-task stack high-water marks on C3 |
| Regression | Verify existing audio send/receive, interruption, reconnect, and the WebSocket path where shared behavior changes |

Run repository host tests and representative Wi-Fi/4G builds when implementing the change. Packet-loss and firewall injection, wake-to-first-audio measurements, modem scheduling, playback/capture, and idle behavior still require integration and physical hardware validation; a successful build alone is insufficient.

## 12. Reference encoding, test vectors, and references

The following Python helpers define byte encoding and the header/tag check. Pass raw decoded bytes and an integer sequence; obtain `ssrc` from `hex_decode(udp.nonce)[4:8]`. The caller must additionally enforce current generation, pending-round existence, and the monotonic deadline. The helper's `pending_ping` must be the locally generated 32-byte request for that round.

```python
import hashlib
import hmac
import struct


def encode_probe(kind, key, ssrc, probe_seq):
    if kind not in (0x02, 0x03):
        raise ValueError("invalid probe type")
    if len(key) != 16 or len(ssrc) != 4:
        raise ValueError("invalid key or SSRC length")
    if not 1 <= probe_seq <= 0xFFFFFFFF:
        raise ValueError("invalid probe sequence")
    header = struct.pack("!BBH4sII", kind, 0, 16, ssrc, 0, probe_seq)
    return header + hmac.new(key, header, hashlib.sha256).digest()[:16]


def matches_pending_pong(data, pending_ping, key):
    if len(data) != 32 or len(pending_ping) != 32 or len(key) != 16:
        return False
    if data[0] != 0x03 or data[1:16] != pending_ping[1:16]:
        return False
    expected = hmac.new(key, data[:16], hashlib.sha256).digest()[:16]
    return hmac.compare_digest(data[16:], expected)
```

Test inputs:

```text
key       = 000102030405060708090a0b0c0d0e0f
nonce     = 01000000112233440000000000000000
ssrc      = 11223344
probe_seq = 1

PING header = 02000010112233440000000000000001
PING tag    = daef4dd63a34feb152d16095500e9506
PING packet = 02000010112233440000000000000001daef4dd63a34feb152d16095500e9506

PONG header = 03000010112233440000000000000001
PONG tag    = 7cd8501761355f1009447bad2c4f8a56
PONG packet = 030000101122334400000000000000017cd8501761355f1009447bad2c4f8a56
```

- [Base MQTT + UDP protocol](mqtt-udp.md)
- [RFC 8489, UDP request retransmissions](https://www.rfc-editor.org/rfc/rfc8489.html#section-6.2.1): background for limited retries and response matching; this proposal does not implement STUN.
- [RFC 8085, middlebox traversal](https://www.rfc-editor.org/rfc/rfc8085.html#section-3.5): background for UDP keepalive frequency and jitter.
- [PSA Crypto API, message authentication codes](https://arm-software.github.io/psa-api/crypto/1.2/api/ops/macs.html): implementation reference for MAC operations.
