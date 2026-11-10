# Changelog

All notable changes to Lila are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Lila is a fork of [XiaoZhi AI Chatbot](https://github.com/78/xiaozhi-esp32).
Changes made upstream are not repeated here; this file tracks what Lila adds and
what it changes on top of the upstream baseline. Lila versions are independent of
the upstream `PROJECT_VER` in `CMakeLists.txt` (currently `2.5.3`); the Lila
version is the release tag and the heading below.

## [0.2.0] - 2026-11-10

The development workflow release: specs, standards, skills, and CI.

### Added

- Spec-driven development with OpenSpec (`openspec/`) and per-feature specs in
  `process/specs/FEAT_XXX/`, including a template and task list.
- Process documentation: `process/README.md`, `WORKFLOW.md`, `STANDARDS.md`,
  `ROADMAP.md`, `TOOLING.md`, plus five area standards and an ADR for the
  PR-only policy.
- Vendored workflow skills in `.opencode/skills/` and `.claude/skills/`:
  `feature-pipeline`, `standards-injector`, `architecture-compliance-check` and
  `code-review-checklist`, with `scripts/sync_skills.py` enforcing parity.
- `scripts/check_workflow.py`: required files and links, `.env` policy, roadmap
  feature markers, skills parity and a secret scan.
- PR-only `main`: `.github/workflows/pr-guard.yml` detects direct pushes,
  `.github/BRANCH_PROTECTION.md` documents the preventive ruleset, and
  `.github/PULL_REQUEST_TEMPLATE.md` carries the review gates.
- CI: `.github/workflows/ci.yml` runs compile checks, policy checks, skill
  parity, the host test suite, the build preview and `clang-format` on changed
  C/C++ files for every pull request.
- Manual firmware build workflow: `.github/workflows/firmware-build.yml` builds
  the configurable Lila image in the ESP-IDF 6.1 container.
- CodeGraph and RTK onboarding, including `.codegraph/` in `.gitignore`.

### Changed

- `AGENTS.md` now states the repository workflow and links the process docs.
- The root `README.md` documents the development workflow, the tooling and the
  checks every pull request runs.
- Telegram and Tavily HTTP failures log a provider label instead of the request
  URL, so a bot token embedded in the Telegram URL never reaches the log.

### Security

- Documented the rule that URLs or bodies embedding a secret are never logged,
  and added a mechanical scan for committed token shapes.

## [0.1.0] - 2026-11-10

The first Lila release: a configurable build and two opt-in device tools.

### Added

- Lila identity and documentation, with credit to the upstream project.
- `.env`-driven build wrapper `scripts/build_lila.py` (board, variant, language,
  wake word, panel, credentials, extra sdkconfig, zip) with `--dry-run`.
- Custom MultiNet wake words from the CLI (`scripts/build.py --wake-word
  custom:<word>`), defaulting to `custom:lila`.
- Repeatable `--sdkconfig KEY=VALUE` overrides applied after derived options.
- `web.search` MCP tool backed by Tavily, registered only when
  `XIAOZHI_TAVILY_API_KEY` is set.
- `self.telegram.send` MCP tool delivered from a one-shot worker task, registered
  only when the bot token and chat id are set.
- Spanish + ST7789 240x240 hardware profile for the `wk-esp32s3-dev` board,
  including the 7-pin variant without a CS line.
- Board documentation for the wrapper, the display variants and both tools.

### Notes

- Upstream baseline: Wi-Fi/Ethernet/4G networking, WebSocket and MQTT+UDP
  transports, ESP-SR wake word, Opus audio pipeline, speaker recognition,
  OLED/LCD UI, camera input on supported boards, MCP, 39 interface languages and
  the full board matrix.
