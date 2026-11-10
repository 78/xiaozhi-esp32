---
name: Lila code review checklist
description: Use at Step 5.5 of the Lila workflow, after implementation, to run the pull-request review checklist before opening or merging a PR.
---

# Lila code review checklist

The review everyone applies to their own PR before asking for a review. Each
line is verifiable in the diff or from a command; "looks fine" is not an answer.

## 1. Scope and spec

- [ ] The PR implements one logical change and matches
  `process/specs/FEAT_XXX/FEAT_XXX.md`.
- [ ] `tasks.md` is checked off for the work that landed; deferred tasks say so.
- [ ] Refactors are not mixed with behaviour changes.
- [ ] Every acceptance criterion is either met or explicitly reported as not
  met.

## 2. Firmware diff (when `main/**` changed)

- [ ] Layering respected: no concrete board include in core files.
- [ ] Board tools registered in that board's `InitializeTools()`, guarded by
  non-empty Kconfig credentials.
- [ ] No blocking call added to the main loop or an audio task.
- [ ] `cJSON` ownership is clear and `std::unique_ptr` / RAII is used where the
  codebase already does.
- [ ] Network input validated; response sizes bounded; HTTP timeout set.
- [ ] Logs contain no token, secret or token-bearing URL (`SEC-5`).
- [ ] User-visible strings follow the project locale mechanism, not hardcoded
  English in a localized path.
- [ ] `python scripts/check_format.py --base origin/main` passes (clang-format on the changed lines).

## 3. Tooling diff (when `scripts/**` changed)

- [ ] Standard library only (`PY-3`); Python 3.10+ syntax.
- [ ] Success path and rejection path both tested (`PY-15`).
- [ ] Tests are offline, hermetic, use `tempfile`, and pass `encoding="utf-8"`
  (`PY-12`, `PY-13`).
- [ ] Errors tell the user what to do, name the key, never print the value
  (`PY-7`, `PY-9`).
- [ ] Commands are argument lists, never `shell=True` (`PY-11`).
- [ ] `--dry-run` covers anything that mutates the tree or starts a build.

## 4. CI and release (when `.github/**`, versions or `.env.example` changed)

- [ ] Workflow permissions stay at `contents: read` unless justified (`SEC-11`).
- [ ] No repository secret added; a missing credential fails closed.
- [ ] Required checks in `process/standards/ci-release.md` still describe reality.
- [ ] Branch protection guide (`.github/BRANCH_PROTECTION.md`) updated when a
  check name changes.

## 5. Tests

- [ ] New behaviour has a test that fails without the change.
- [ ] The failure mode has a regression test.
- [ ] No test depends on ESP-IDF, a device, the network or the local timezone.
- [ ] Full suite run locally: `python -m unittest discover -s scripts/tests -v`.
- [ ] Windows-only environment failures are named in the PR, not hidden.

## 6. Documentation

- [ ] Root or board README updated for user-visible change (`DOC-1`, `DOC-3`).
- [ ] `process/ROADMAP.md` shipped table updated (`DOC-13`).
- [ ] `CHANGELOG.md` has an entry under the right version.
- [ ] `openspec/specs/` updated when a capability contract changed.
- [ ] Commands in the docs were run exactly as written (`DOC-6`).
- [ ] What was not validated on hardware is stated (`DOC-9`).

## 7. Security

- [ ] `python scripts/check_workflow.py` passes (secret scan included).
- [ ] No `.env`, `sdkconfig`, token, chat id or private URL staged.
- [ ] Payloads in logs are truncated and non-sensitive (`SEC-7`).
- [ ] A leaked secret would be rotated first, not just deleted (`SEC-4`).

## 8. Git hygiene

- [ ] Branch starts from an up-to-date `main`; never pushed to `main`.
- [ ] Commits are Conventional Commits and the PR title matches the squash
  subject.
- [ ] No generated or unrelated files staged (`git status --short` reviewed).
- [ ] The PR body uses `.github/PULL_REQUEST_TEMPLATE.md` and lists the checks
  run plus the hardware validation still pending.

## Verdict

Write one of:

- **Approve** — all applicable items pass.
- **Approve with follow-up** — pass, with issues filed for the deferred items.
- **Request changes** — one or more blocking items fail; name the item and the
  file.

Paste the verdict, the checks run and the pending hardware validation into the
PR body.
