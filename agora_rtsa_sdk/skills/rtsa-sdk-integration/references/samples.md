# Sample Mapping

Use this reference to choose the closest existing sample before changing project code.

## hello_rtsa

Files:

- `example/hello_rtsa/hello_rtsa.c`
- `example/hello_rtsa/hello_rtsa_multi.c`

Best for:

- end-to-end RTC lifecycle
- single connection or multi-connection app structure
- audio and video send loops
- callback registration
- connection state handling
- channel options setup

Copy from here when the user asks for a general RTSA media integration.

## hello_rtm

Files:

- `example/hello_rtm/hello_rtm.c`
- `example/hello_rtm/hello_rtm_file.c`

Best for:

- RTM login and logout
- peer messaging
- send-result callbacks
- simple throughput tests

Copy from here when the app needs a control or business message channel.

## hello_rdt

Files:

- `example/hello_rdt/hello_rdt.c`
- `example/hello_rdt/hello_rdt_multi.c`

Best for:

- reliable peer transfer
- file-like chunk delivery
- transfer status and progress handling

Copy from here when the user needs reliable payload transfer beyond RTM-style messaging.

## hello_rtcm

Files:

- `example/hello_rtcm/hello_rtcm.c`

Best for:

- custom control payloads tied to a channel session

## hello_stream_message

Files:

- `example/hello_stream_message/hello_stream_message.c`

Best for:

- ordered or reliable in-channel message streams

## Selection Rule

Pick the sample that is structurally closest to the user's actual requirement. Reuse the state
machine and callback wiring first. Only then adapt transport details, media sources, or business
logic.
