---
name: Lila architecture compliance check
description: Use at Step 5.5 of the Lila workflow, after implementation, to verify the AGENTS.md architecture rules and the firmware standard before opening a pull request.
---

# Lila architecture compliance check

Verify that an implemented change respects the firmware architecture and the
repository rules. This is a gate, not a summary: every check reports pass, fail
or not-applicable, and a failure blocks the commit.

## Inputs

- `AGENTS.md` (authoritative rules), `process/standards/firmware-cpp.md`
- the diff: `git diff --stat origin/main...HEAD`
- the spec: `process/specs/FEAT_XXX/FEAT_XXX.md`

## Checks

### 1. Layering

- [ ] Core files (`main/*.cc`, `main/*.h`, `main/audio/`, `main/protocols/`,
  `main/display/`, `main/led/`) include only `Board` abstractions, never a
  concrete board header or a board `config.h`.
- [ ] Board-specific code stays under `main/boards/<board>/`.
- [ ] Shared protocol semantics live in `Protocol`; both WebSocket and MQTT/UDP
  still behave the same when the contract changed.
- [ ] New helpers are in the narrowest owning layer, not in a catch-all file.

### 2. Board and build chain

- [ ] Exactly one `DECLARE_BOARD(...)` per build configuration.
- [ ] No existing board's pins, panel or identity changed to fit new hardware; a
  variant was added instead.
- [ ] Board selection chain updated consistently when a board or variant was
  added: `config.json` → `scripts/build.py` → `main/Kconfig.projbuild` →
  `main/CMakeLists.txt` → board source and `config.h`.
- [ ] New Kconfig options have defaults that keep a stock build upstream-like.
- [ ] Generated files were not edited (`build/`, `releases/`, `managed_components/`,
  `components/`, `sdkconfig*`, `main/assets/lang_config.h`, mmap headers).

### 3. Runtime behaviour

- [ ] State changes go through `Application::SetDeviceState()` and the state
  machine.
- [ ] Work from a non-main task is scheduled with `Application::Schedule()` or
  event bits instead of mutating application state directly.
- [ ] No blocking call on the main event loop or an audio task. Network work from
  an MCP callback is bounded or moved to a worker task with an explicit stack.
- [ ] Optional hardware (camera, display, backlight, LED, battery) is guarded with
  Kconfig or a runtime capability check.
- [ ] Failure paths return `std::unexpected("...")` with an actionable message;
  nothing aborts or reports false success.

### 4. Memory and data ownership

- [ ] `cJSON` objects are owned by one path: consumed, deleted, or wrapped in
  `CJsonUniquePtr` / `CJsonStringUniquePtr`.
- [ ] Raw allocations have one owner and one release path on every branch
  (including early returns and task-creation failure).
- [ ] Network input is validated (`cJSON_IsString` / `cJSON_IsArray`, status
  codes, empty bodies) and sizes are bounded.
- [ ] Audio paths have no unbounded queues or repeated large allocations.
- [ ] NVS keys are treated as persistent API; a rename ships a migration.

### 5. Repository rules

- [ ] `AGENTS.md` and the applicable `process/standards/` rules are satisfied.
- [ ] The change is on a `devs/<user>/<type>/<slug>` branch, not `main`.
- [ ] Generated docs updated when behaviour changed (`process/ROADMAP.md`,
  `CHANGELOG.md`, README/board README).
- [ ] No secret, token or private URL in the diff; logs are redacted.

## Output format

Record this in the PR body:

```markdown
### Architecture compliance
- Layering: pass
- Board/build chain: pass (no board changes)
- Runtime: pass — Telegram send moved to a worker task (`lila_tg`, 6144 B)
- Memory: pass — payload owned by `CJsonUniquePtr`
- Repository: pass — branch `devs/mauri/feat/...`, roadmap + changelog updated
```

A waived item needs a written reason and a follow-up issue. There is no
"partially compliant": fix it or record the debt explicitly in the spec.
