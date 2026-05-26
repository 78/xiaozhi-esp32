# Advanced Features

Use this reference when the task involves data stream, license handling, cloud proxy, or other
non-core integration features.

## Data Stream

The main APIs are:

- `agora_rtc_create_data_stream`
- `agora_rtc_send_stream_message`

The main callback is:

- `on_stream_message`

Use data stream for ordered or reliable in-channel signaling that should follow the RTC session
context instead of a separate peer messaging identity.

## License

The main APIs are:

- `agora_rtc_license_gen_credential`
- `agora_rtc_license_verify`

The main callback is:

- `on_license_validation_failure`

If the task involves license flow, make sure the integration sequence reflects the API contract and
places license verification before normal SDK initialization when required by the calling pattern.

## Other Useful APIs

These often appear in real integrations:

- `agora_rtc_set_cloud_proxy`
- `agora_rtc_set_fec_config`
- `agora_rtc_set_log_level`
- `agora_rtc_config_log`
- `agora_rtc_set_params`
- `agora_rtc_notify_network_event`
- `agora_rtc_get_memory_used`

Only introduce them when the user requirement calls for them. Do not inflate a basic integration
patch with optional advanced controls.

## Real References

- Stream message sample:
  `example/hello_stream_message/hello_stream_message.c`
- Public API guide:
  `AGENTS.md`
