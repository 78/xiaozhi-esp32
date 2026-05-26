# Audio And Video

Use this reference when the task is about media frame ingress or egress, subscriptions, encoder
coordination, or callback wiring.

## Audio Path

The main send-side audio API is:

- `agora_rtc_send_audio_data`

The main receive-side audio callbacks are:

- `on_audio_data`
- `on_mixed_audio_data`

Use `audio_frame_info_t` to describe the audio payload format. The integration must match the
application's real source format:

- PCM input
- Opus
- G.711 A-law
- G.711 u-law
- G.722
- AAC variants

If the app sends PCM, configure `channel_options.audio_codec_opt` consistently with the sample rate,
channel count, and intended codec behavior.

## Video Path

The main send-side video API is:

- `agora_rtc_send_video_data`

The main receive-side video callback is:

- `on_video_data`

Use `video_frame_info_t` to describe:

- payload type such as H.264, H.265, or JPEG
- frame type or auto-detect mode
- frame rate assumptions
- stream type

## Bandwidth And Key Frames

These APIs and callbacks matter for video integrations:

- `agora_rtc_set_bwe_param`
- `on_target_bitrate_changed`
- `on_key_frame_gen_req`

Use them to connect RTSA network feedback to the application's encoder behavior. If the host
project already owns the encoder, wire bitrate updates into that existing control path rather than
adding a parallel configuration layer.

## Subscription And Mute Controls

The common channel controls are:

- `agora_rtc_mute_local_audio`
- `agora_rtc_mute_local_video`
- `agora_rtc_mute_remote_audio`
- `agora_rtc_mute_remote_video`
- `agora_rtc_request_video_key_frame`

Use these when the app needs selective publish or subscribe logic instead of custom filtering in the
callback path.

## Real References

- App-side media flow and channel setup:
  `example/hello_rtsa/hello_rtsa.c`
- Multi-connection send loops:
  `example/hello_rtsa/hello_rtsa_multi.c`
- High-level API guide:
  `AGENTS.md`
