# RDT Transfer

Use this reference when the app needs reliable ordered payload transfer larger than typical RTM
messages, including file-like transfer and progress-aware delivery logic.

## Core RDT Calls

The main RDT APIs are:

- `agora_rtc_send_rdt_msg`
- `agora_rtc_get_rdt_status_info`

The main RDT callbacks are:

- `on_rdt_state`
- `on_rdt_msg`

## When To Use RDT

Prefer RDT when the task requires:

- reliable delivery to a specific peer
- larger payloads than RTM is designed for
- flow-controlled or tunnel-state-aware transfer
- file-like transfer behavior

Do not use RDT as a substitute for media frame transport inside the normal audio or video path.

## Integration Guidance

- Model RDT as a peer-scoped data channel in application state.
- Handle tunnel readiness before sending high-volume payloads.
- Keep application-level chunking, resend policy, and file assembly logic outside the SDK wrapper.
- Surface status transitions so the host app can report transfer readiness and failure.

## Real References

- RDT sample: `example/hello_rdt/hello_rdt.c`
- Multi-connection RDT sample:
  `example/hello_rdt/hello_rdt_multi.c`
- RDT customer-facing notes: `README.RDT.md`
