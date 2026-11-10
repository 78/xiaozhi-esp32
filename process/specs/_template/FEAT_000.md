# FEAT_000 — Template

| Field | Value |
| --- | --- |
| ID | FEAT_000 |
| Title | Short imperative title |
| Owner | @github-handle |
| Status | draft |
| Priority | high / medium / low |
| Branch | `devs/<user>/feat/<slug>` |
| OpenSpec change | `FEAT-000-<slug>` (optional) |
| Created | YYYY-MM-DD |
| Updated | YYYY-MM-DD |

## Summary

Two or three sentences: the problem, the user-visible outcome, and why now.

## Motivation

Who needs this, what breaks without it, and what the current workaround is.
Link issues, discussions or upstream PRs.

## Acceptance criteria

- [ ] Observable behaviour 1
- [ ] Observable behaviour 2
- [ ] Failure mode is explicit (what happens without credentials/hardware)
- [ ] Documentation updated (`README.md`, board README, `process/ROADMAP.md`)
- [ ] Tests cover the new behaviour

## Technical blueprint

| Area | Change |
| --- | --- |
| Firmware | `main/...` files, symbols, Kconfig options |
| Build tooling | `scripts/...` functions and CLI flags |
| Tests | `scripts/tests/...` cases |
| Docs | Files to update |
| CI | Jobs or checks touched |

### Data and contracts

Inputs, outputs, persisted state (NVS keys), network payloads, and what must stay
backwards compatible. Mention ownership and error handling for network or cJSON
data.

### Risks

- Risk: … → mitigation: …

## Scope

**In scope**

- …

**Out of scope**

- …

## Dependencies

- Upstream components, ESP-IDF version, hardware, credentials, other FEATs.

## Hardware validation

What cannot be verified on the host and must be checked on a device:

- [ ] Build and flash the affected variant
- [ ] Wake word / audio path
- [ ] Display and input
- [ ] Network tool behaviour (search, Telegram, …)

## Notes

Decisions taken while implementing, links to ADRs, follow-ups.
