# Firmware standard (C/C++)

Applies to `main/**`, board sources, `main/Kconfig.projbuild` and
`main/CMakeLists.txt`. The rules mirror `AGENTS.md`; if they disagree, `AGENTS.md`
wins and this file must be fixed.

## Layering

- **FW-1** Core code depends only on `Board` interfaces. Never include a
  concrete board class or a board `config.h` from `main/*.cc`.
- **FW-2** Board-specific behaviour lives in `main/boards/<board>/`. Custom MCP
  tools are registered in that board's `InitializeTools()`, not in
  `AddCommonTools()`.
- **FW-3** Do not change an existing board's pins, display choice or identity to
  support different hardware. Add a new variant and document it.
- **FW-4** Exactly one `DECLARE_BOARD(...)` per build; the board identity is part
  of OTA compatibility.

## Runtime behaviour

- **FW-5** Change state through `Application::SetDeviceState()` and the state
  machine, never by assigning fields directly.
- **FW-6** Never block the main task or an audio task. Network calls from an MCP
  callback run on the main task, so either keep them bounded or move them to a
  short-lived worker task with an explicit stack size.
- **FW-7** MCP callbacks must report failures as `std::unexpected("...")` with an
  actionable message, never abort or silently succeed.
- **FW-8** Treat camera, display, LED, battery and network as optional. Guard
  target-specific features with Kconfig and handle a missing device.

## Memory and data

- **FW-9** Preserve `cJSON` ownership: create, consume, `cJSON_Delete`, or wrap in
  `CJsonUniquePtr` / `CJsonStringUniquePtr` from `cjson_utils.h`.
- **FW-10** Prefer `std::string` and RAII over manual `malloc`/`free`. Any raw
  allocation must have one owner and one release path on every branch.
- **FW-11** Validate network input before use: check `cJSON_IsString`,
  `cJSON_IsArray`, status codes, empty bodies, and bound sizes (response body,
  snippet length, message length).
- **FW-12** No unbounded queues or repeated large allocations on audio paths.

## Build configuration

- **FW-13** New user-facing options need a Kconfig entry in
  `main/Kconfig.projbuild` with a sensible default, and a guard so a stock build
  behaves like upstream.
- **FW-14** Keep credentials out of source. String Kconfig options are compiled
  into the binary, so document that `.env` is the delivery mechanism and never
  commit a filled value.
- **FW-15** Do not edit generated files: `build/`, `releases/`,
  `managed_components/`, `components/`, `sdkconfig*`, `main/assets/lang_config.h`,
  mmap headers.

## Security

- **FW-16** Never log a URL or payload that embeds a secret. Telegram bot URLs
  contain the token: log a label such as `"Telegram API"` instead of the URL.
- **FW-17** Bound and validate every HTTP response before parsing; close the
  client on every error path.

## Style

- **FW-18** Format the C/C++ lines you touch with the pinned clang-format
  (23.1.5, `python -m pip install clang-format==23.1.5`). CI runs
  `python scripts/check_format.py --base origin/main`, which validates only the
  changed lines; do not reformat unrelated code.
- **FW-19** Comments in English; user-facing strings follow the project locale.
- **FW-20** Prefer the existing helper (`Http`, `Board`, MCP `Property`) over new
  abstractions. Read the closest implementation before adding one.
