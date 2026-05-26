# Core Lifecycle

Use this reference when the task is about bootstrapping RTSA in an app, mapping SDK ownership, or
adding the minimum lifecycle code.

## Public Entry Point

All public SDK APIs are declared in:

- `agora_sdk/include/agora_rtc_api.h`

Treat that header as the only supported C integration surface.

## Minimal Lifecycle

The canonical application flow is:

1. Prepare an `agora_rtc_event_handler_t`
2. Fill `rtc_service_option_t`
3. Call `agora_rtc_init`
4. Call `agora_rtc_create_connection`
5. Fill `rtc_channel_options_t`
6. Call `agora_rtc_join_channel` or `agora_rtc_join_channel_with_user_account`
7. Wait for `on_join_channel_success`
8. Enter application send or receive loop
9. On shutdown, call `agora_rtc_leave_channel`
10. Call `agora_rtc_destroy_connection`
11. Call `agora_rtc_fini`

## What the Application Owns

The host application is expected to own:

- App ID and token provisioning
- Channel name and local identity selection
- Event loop or worker thread model
- Audio and video capture or encoded frame source
- Local buffering and pacing outside the SDK when needed
- Shutdown signals and reconnect policy at the app layer

## Required Structures

The most common structures to fill first are:

- `agora_rtc_event_handler_t`
- `rtc_service_option_t`
- `rtc_channel_options_t`
- `audio_frame_info_t`
- `video_frame_info_t`

## Integration Notes

- Initialize the SDK once per process unless the user has a very specific multi-process design.
- Keep connection IDs in app state. They are the handle for nearly all follow-up API calls.
- Use callback-driven state transitions instead of assuming `join_channel` means media can be sent
  immediately.
- If the application already has a main loop, integrate RTSA state into that loop instead of
  spawning extra threads by default.

## Real References

Use these files when you need concrete code patterns:

- SDK lifecycle walkthrough: `AGENTS.md`
- Single-connection sample: `example/hello_rtsa/hello_rtsa.c`
- Multi-connection sample: `example/hello_rtsa/hello_rtsa_multi.c`
