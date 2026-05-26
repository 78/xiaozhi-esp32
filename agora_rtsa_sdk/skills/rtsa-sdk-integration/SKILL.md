---
name: rtsa-sdk-integration
description: Integrate the RTSA SDK from a packaged release, explain how to use agora_rtc_api.h end to end, adapt application code to the SDK lifecycle and callbacks, and map user requirements to the right shipped demo and API flow.
---

# RTSA SDK Integration

Use this skill when the task is to integrate the RTSA SDK into an application from a packaged SDK
release, explain how to use `agora_rtc_api.h`, adapt existing code to the RTSA lifecycle, or build
a minimal RTSA-based sample from a real project requirement.

This skill is for customer integration work. Use only files that exist in the packaged SDK and
prefer public APIs and shipped examples.

## What to Read First

Start with the minimum context that matches the task:

- Customer-facing integration guide: `AGENTS.md`
- Public API declarations: `agora_sdk/include/agora_rtc_api.h`

Then load only the reference file needed for the requested feature:

- Core lifecycle and app structure: `references/core.md`
- Audio and video send or receive: `references/media.md`
- RTM messaging: `references/rtm.md`
- RDT transfer: `references/rdt.md`
- Data stream and license: `references/advanced.md`
- Real sample entry points: `references/samples.md`

## Integration Workflow

Follow this sequence when helping with RTSA integration:

1. Inspect the target project's build system, runtime loop, threading model, and media sources.
2. Confirm the integration surface:
   - New RTSA-based module
   - Adaptation of an existing media or messaging pipeline
   - Replacement of another SDK
3. Use only `agora_rtc_api.h` as the SDK interface boundary.
4. Build the integration around the real lifecycle:
   - `agora_rtc_init`
   - `agora_rtc_create_connection`
   - `agora_rtc_join_channel` or `agora_rtc_join_channel_with_user_account`
   - send or receive loop driven by callbacks and app state
   - `agora_rtc_leave_channel`
   - `agora_rtc_destroy_connection`
   - `agora_rtc_fini`
5. Reuse patterns from the closest shipped sample instead of inventing a new wrapper shape.
6. Keep the first patch minimal. Add abstractions only when the host project already uses them.

## Output Rules

When explaining or implementing integration:

- Speak in terms of SDK capabilities and public APIs.
- Prefer concrete call sequences, data flow, and callback ownership over generic summaries.
- Keep code aligned with the host project's existing style and build system.
- Register only the callbacks the application actually uses.
- Wait for join success before starting send loops.
- Use `agora_rtc_err_2_str` in error paths that surface SDK return codes.

## Constraints

- Do not include SDK internal headers. Use `agora_sdk/include/agora_rtc_api.h`.
- Do not assume unpublished internal modules or source-tree files exist in the customer package.
- Do not default to a large wrapper layer if a direct integration is enough.

## Sample Selection

Choose the closest real sample before writing code:

- Basic channel join plus audio or video send or receive: `hello_rtsa`
- RTM peer messaging: `hello_rtm`
- Reliable large-payload or file-like transfer: `hello_rdt`
- Channel control payloads: `hello_rtcm`
- Ordered or reliable in-channel signaling: `hello_stream_message`

Use `references/samples.md` for file-level entry points and what to copy from each sample.
