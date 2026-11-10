---
name: Lila standards injector
description: Use at Step 2.5 of the Lila workflow, before editing files, to detect the touched areas and load the standards that apply.
---

# Lila standards injector

Select the standards that govern the files a change touches, read them, and
record the selection in the feature's `tasks.md`. Standards live in
`process/standards/`; the index and precedence rules are in
`process/STANDARDS.md`.

## Procedure

1. Determine the change set. Prefer the real diff, otherwise the files the spec
   plans to touch:

   ```bash
   git diff --name-only origin/main...HEAD
   git status --short
   ```

2. Map every path through the table below. Intersect areas: a PR that edits
   firmware and the build wrapper loads two standards.

3. Read each selected standard fully before editing. Do not skim; the rules are
   numbered (`FW-*`, `PY-*`, `CI-*`, `DOC-*`, `SEC-*`) and reviews cite them.

4. Record the result in `process/specs/FEAT_XXX/tasks.md`:

   ```markdown
   Standards: firmware-cpp (FW-6, FW-9, FW-16), security (SEC-5), docs (DOC-13)
   ```

5. Re-check once before the PR. If the diff grew into a new area, load the
   standard you missed.

## Mapping

| Path pattern | Standard |
| --- | --- |
| `main/**/*.cc`, `main/**/*.h`, `main/**/*.hpp`, `main/**/*.c` | `process/standards/firmware-cpp.md` |
| `main/Kconfig.projbuild`, `main/CMakeLists.txt`, `main/idf_component.yml` | `process/standards/firmware-cpp.md` + `process/standards/ci-release.md` |
| `main/boards/**/config.h`, `main/boards/**/config.json` | `process/standards/firmware-cpp.md` |
| `scripts/**/*.py`, `scripts/tests/**` | `process/standards/python-tooling.md` |
| `.github/**`, `.env.example`, release or version changes | `process/standards/ci-release.md` |
| `process/**/*.md`, `README*.md`, `main/boards/**/README.md`, `docs/**` | `process/standards/docs.md` |
| anywhere: credentials, tokens, logging, HTTP/JSON payloads | `process/standards/security.md` |
| `partitions/**`, `sdkconfig.defaults*` | `process/standards/firmware-cpp.md` + `process/standards/ci-release.md` |

If a path is not listed, fall back on the closest rule: anything compiled into
the firmware is firmware, anything that runs on a developer machine or CI is
tooling.

## Always-on rules

These apply to every change, whatever the diff:

- `SEC-5` — never log a URL or body that embeds a secret.
- `SEC-12` — `python scripts/check_workflow.py` passes.
- `DOC-13` — user-visible changes update `process/ROADMAP.md` and
  `CHANGELOG.md`.
- Never commit `.env`, `sdkconfig`, or a filled credential.

## Output

Reply with the selected standards and the specific rule IDs that are likely to
matter, so the implementation and the review use the same checklist. If no
standard applies (rare), say so explicitly instead of skipping silently.
