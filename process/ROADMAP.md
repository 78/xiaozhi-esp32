# Lila roadmap

Lila is a Spanish-first family voice assistant built on the open-source
[XiaoZhi AI Chatbot](https://github.com/78/xiaozhi-esp32). Upstream features stay
available; this file tracks what Lila adds on top and what is planned next.

Status: ✅ shipped · 🚧 in progress · 📋 planned · 💡 idea

## Shipped — v0.1.0 (current)

The first Lila release adds a configurable build and two optional device tools,
all opt-in so a stock configuration still behaves like upstream.

| Feature | Status | Where | Notes |
| --- | --- | --- | --- |
| Lila identity and docs | ✅ | `README.md`, `main/boards/wk-esp32s3-dev/README.md` | Fork credited to upstream, features listed |
| `.env`-driven build wrapper | ✅ | `scripts/build_lila.py`, `.env.example` | Board, variant, language, wake word, panel, keys, zip; `--dry-run` |
| Custom MultiNet wake word | ✅ | `scripts/build.py`, `scripts/build_default_assets.py`, `main/audio/wake_words/custom_wake_word.cc` | `--wake-word custom:<word>`; default `custom:lila` |
| Raw sdkconfig overrides | ✅ | `scripts/build.py` (`--sdkconfig KEY=VALUE`) | Applied after derived options; used for keys and panel choice |
| Web search tool | ✅ | `main/boards/wk-esp32s3-dev/wk-esp32s3-dev.cc` | `web.search` via Tavily, enabled by `XIAOZHI_TAVILY_API_KEY` |
| Telegram tool | ✅ | `main/boards/wk-esp32s3-dev/wk-esp32s3-dev.cc` | `self.telegram.send`, enabled by `XIAOZHI_TELEGRAM_BOT_TOKEN` + `XIAOZHI_TELEGRAM_CHAT_ID` |
| Spanish + ST7789 hardware profile | ✅ | `scripts/build_lila.py`, `main/Kconfig.projbuild` | `es-ES`, `LCD_ST7789_240X240_7PIN` or `LCD_ST7789_240X240` |
| Tool documentation | ✅ | `main/boards/wk-esp32s3-dev/README.md` | Wrapper, display variants, both tools |

### Upstream baseline inherited by Lila v0.1.0

Wi-Fi / Ethernet / 4G networking, WebSocket and MQTT+UDP transports, offline
wake word with ESP-SR, Opus audio pipeline, speaker recognition, OLED/LCD UI,
camera input on supported boards, device-side and cloud-side MCP, 39 interface
languages, 138 board directories / 171 release variants. Supported chips:
ESP32, C3, C5, C6, S3, P4.

## 🚧 v0.2.0 — Development workflow

This release is about making changes safe and repeatable.

| Item | Status | Where |
| --- | --- | --- |
| Spec-driven development with OpenSpec and `FEAT_XXX` specs | ✅ | `openspec/`, `process/specs/` |
| Process documentation (workflow, standards, roadmap, tooling) | ✅ | `process/` |
| Vendored workflow skills for OpenCode and Claude Code | ✅ | `.opencode/skills/`, `.claude/skills/` |
| PR-only `main` with detection guard and branch protection guide | ✅ | `.github/workflows/pr-guard.yml`, `.github/BRANCH_PROTECTION.md` |
| CI host checks (tests, policy, skill parity, formatting) | ✅ | `.github/workflows/ci.yml`, `scripts/check_workflow.py` |
| Manual firmware build in the ESP-IDF 6.1 container | ✅ | `.github/workflows/firmware-build.yml` |
| CodeGraph and RTK onboarding | ✅ | `process/TOOLING.md` |

## 📋 v0.3.0 — Quality and automation

| Item | Status | Notes |
| --- | --- | --- |
| Firmware build on every firmware-touching PR | 📋 | Path-filtered job reusing `firmware-build.yml` |
| Static analysis for C/C++ | 📋 | `clang-tidy` on touched files, warnings as errors for new code |
| Coverage report for Python tooling | 📋 | `coverage.py`, minimum threshold on `scripts/` |
| Release automation | 📋 | Tag `vX.Y.Z`, build artifacts, publish `releases/<version>_<board>.zip` |
| Hardware-in-the-loop smoke test | 📋 | Documented manual checklist per board revision |

## 📋 v0.4.0 — Product features

| Item | Status | Notes |
| --- | --- | --- |
| Lila OTA identity and boot branding | 📋 | Product name, board identity, boot screen |
| More MCP tools | 💡 | Calendar, weather, reminders, home automation |
| Conversation memory controls | 💡 | Per-family voice profiles and privacy settings |
| Additional boards and panels | 💡 | Each new board needs a spec, variant and README |
| Spanish documentation set | 💡 | `README_es.md`, Spanish board guides |

## 💡 Backlog

- Local wake-word training workflow for other family names.
- TTS voice selection in `.env`.
- Device diagnostics command exposed over MCP.
- Dashboard for `rtk gain` and build times.

## Versioning and how this file changes

- Versions follow semver; firmware and tooling share one version line.
- Lila milestones are independent of the upstream `PROJECT_VER` in
  `CMakeLists.txt` (currently `2.5.3`, which tracks upstream). The Lila version
  is the release tag and the `CHANGELOG.md` heading, for example `v0.2.0`.
- Every merged PR updates the shipped table or adds a roadmap entry (Step 6 of
  the workflow).
- Moving an item between sections is a normal PR; changing the ordering or scope
  of a release needs maintainer approval.
