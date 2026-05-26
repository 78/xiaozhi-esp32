# RTM Messaging

Use this reference when the task is peer-to-peer signaling, command exchange, text messaging, or
small custom payload transfer outside the media path.

## Core RTM Calls

The main RTM APIs are:

- `agora_rtc_login_rtm`
- `agora_rtc_logout_rtm`
- `agora_rtc_send_rtm_data`

The main RTM callbacks are:

- `on_rtm_event`
- `on_rtm_data`
- `on_rtm_send_data_result`

## When To Use RTM

Prefer RTM when the application needs:

- peer-to-peer control or business messages
- text or JSON-like payloads
- low-frequency non-media signaling
- app-layer interaction that should not be coupled to audio or video frame loops

Do not use RTM as a replacement for large reliable transfer or for media payloads.

## Integration Guidance

- Keep RTM user identity management in the application's account layer.
- Convert SDK callbacks into the host app's existing message dispatch mechanism.
- Surface send result callbacks if the app needs delivery feedback or retries.
- Keep payload sizing and send rate limits in mind when adapting an existing signaling channel.

## Real References

- API walkthrough: `AGENTS.md`
- Main RTM sample: `example/hello_rtm/hello_rtm.c`
- File-oriented RTM sample:
  `example/hello_rtm/hello_rtm_file.c`
- RTM customer-facing notes: `README.RTM.md`
