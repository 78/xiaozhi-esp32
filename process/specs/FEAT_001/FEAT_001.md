# FEAT_001 — Lila development workflow

| Field | Value |
| --- | --- |
| ID | FEAT_001 |
| Title | Add the spec-driven development workflow, standards, skills and CI |
| Owner | @mauricioz |
| Status | done |
| Priority | high |
| Branch | `devs/mauri/chore/lila-workflow` |
| OpenSpec change | — (process-only; no firmware contract) |
| Created | 2026-11-10 |
| Updated | 2026-11-10 |

## Summary

Lila was edited directly on `main` without a spec, a review point or CI. This
feature adds a complete, versioned development workflow: spec-driven
development, per-area standards, agent skills, host checks on every pull request,
an on-demand firmware build, and a PR-only policy for `main`.

## Motivation

The fork already carries opt-in tools and credentials, so an unreviewed push can
publish a broken or leaky build. Contributors (human or agent) also need one
place that states how work is proposed, implemented, reviewed and released. The
workflow must be enforceable rather than advisory, and it must not change
firmware behaviour for upstream users.

## Acceptance criteria

- [x] `process/` documents the pipeline, standards, roadmap, tooling and an ADR
      for the PR-only policy, and the root `README.md` links them.
- [x] A feature spec template exists and this feature uses it.
- [x] Four workflow skills are versioned in `.opencode/skills/` and mirrored to
      `.claude/skills/`; `scripts/sync_skills.py --check` fails on drift.
- [x] `scripts/check_workflow.py` validates required files and links, `.env`
      policy, roadmap markers, skill parity and token-shaped secrets.
- [x] `.github/workflows/ci.yml` runs the host checks and `clang-format` on
      changed C/C++ files for every pull request.
- [x] `.github/workflows/firmware-build.yml` builds a configurable Lila image on
      demand in the ESP-IDF 6.1 container.
- [x] `.github/workflows/pr-guard.yml` detects a direct push to `main`, and
      `.github/BRANCH_PROTECTION.md` documents the preventive ruleset.
- [x] The PR template carries the spec, check, standards, security and hardware
      validation gates.
- [x] OpenSpec is initialised for contract-changing work.
- [x] `CHANGELOG.md` records the release; `process/ROADMAP.md` lists current and
      planned features.
- [x] No secret is introduced and the Telegram HTTP error path no longer logs the
      token-bearing URL.

## Technical blueprint

| Area | Change |
| --- | --- |
| Process | `process/README.md`, `WORKFLOW.md`, `STANDARDS.md`, `ROADMAP.md`, `TOOLING.md`, `specs/`, `standards/`, `adr/` |
| OpenSpec | `openspec/config.yaml`, `openspec/changes/`, `openspec/specs/` |
| Skills | `.opencode/skills/{feature-pipeline,standards-injector,architecture-compliance-check,code-review-checklist}/SKILL.md`, mirrored to `.claude/skills/` |
| Tooling | `scripts/sync_skills.py` (`--check`/`--write`), `scripts/check_workflow.py`, `scripts/tests/test_workflow.py` |
| CI | `ci.yml` (host checks + formatting), `firmware-build.yml` (manual), `pr-guard.yml` (detective) |
| Repository | `AGENTS.md` workflow section, `README.md` workflow section, `.gitignore` (`.codegraph/`), `CHANGELOG.md` |
| Firmware | `main/boards/wk-esp32s3-dev/wk-esp32s3-dev.cc`: `LilaHttpPostJson` takes a label for logging instead of the URL |
| PR policy | `PULL_REQUEST_TEMPLATE.md`, `BRANCH_PROTECTION.md`, ADR 0001 |

### Data and contracts

No firmware contract changed. The build CLI, Kconfig surface, MQTT/UDP and
WebSocket protocols are untouched; only the log text of a failed HTTP request
changes (provider label instead of URL). CI runs with `contents: read` and no
repository secrets.

### Risks

- Risk: branch protection is a GitHub setting and cannot be committed. →
  mitigation: documented setup plus the `Main branch guard` detective workflow.
- Risk: a new required file makes the check fail on forks. → mitigation: the
  check names the missing file and the error message says how to fix it.
- Risk: `clang-format` version drift in CI. → mitigation: format only changed
  files and use the repository `.clang-format` on ubuntu-latest.

## Scope

**In scope**

- Process documentation, standards, skills, scripts, CI workflows and templates.
- A minimal log-redaction fix in the Lila board tooling.
- Changelog and roadmap entries.

**Out of scope**

- Firmware behaviour changes, new boards or new MCP tools.
- Release automation and a firmware build on every PR (tracked in the roadmap).
- GitHub repository settings themselves; they are applied manually once.

## Dependencies

- GitHub Actions (`ubuntu-latest`, `espressif/idf:release-v6.1` container).
- Python 3.10+ standard library. `openspec`, `codegraph` and `rtk` are optional
  local tools documented in `process/TOOLING.md`.

## Hardware validation

Not applicable: no firmware behaviour changed. The log message change compiles
with the same ESP-IDF version but was not flashed; the next Lila build validates
it in practice.

- [x] No device validation required for this change
- [ ] Build the Lila variant with the manual workflow and flash it (next release)

## Notes

- The workflow is deliberately enforceable only through files in the repository
  plus one manual GitHub setting.
- Follow-ups: path-filtered firmware builds, `clang-tidy`, coverage and release
  automation in v0.3.0.
