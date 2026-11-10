# FEAT_001 — Tasks

Status: done · Owner: @mauricioz · Branch: `devs/mauri/chore/lila-workflow`

## 1. Spec and standards

- [x] Write `FEAT_001.md` against the repository
- [x] Select standards: `docs`, `ci-release`, `python-tooling`, `firmware-cpp`, `security`
- [x] Skip OpenSpec: process-only change, no firmware contract

## 2. Implementation

- [x] Process entry point and map (`process/README.md`)
- [x] Pipeline and quality gates (`process/WORKFLOW.md`)
- [x] Standards index and five area standards (`process/STANDARDS.md`, `process/standards/`)
- [x] Roadmap with shipped Lila features and planned releases (`process/ROADMAP.md`)
- [x] Tooling guide for OpenSpec, CodeGraph and RTK (`process/TOOLING.md`)
- [x] Spec template and this feature's spec (`process/specs/`)
- [x] ADR 0001 for the PR-only policy (`process/adr/`)
- [x] OpenSpec initialised (`openspec/`)
- [x] Four skills in `.opencode/skills/`, mirrored to `.claude/skills/`
- [x] `scripts/sync_skills.py` with `--check` and `--write`
- [x] `scripts/check_workflow.py` policy checks
- [x] `ci.yml`, `firmware-build.yml`, `pr-guard.yml`
- [x] PR template and branch protection guide
- [x] Redact the Telegram URL in HTTP error logs

## 3. Tests

- [x] Skill mirror parity, drift, prune and CRLF normalisation
- [x] Changed-line range detection and span merging (`scripts/tests/test_check_format.py`)
- [x] Secret scanner: token detection, placeholders, binary skip
- [x] `.env.example` key coverage and empty secret values
- [x] Required files, roadmap markers and documentation links
- [x] `python -m unittest discover -s scripts/tests -p "test_workflow.py" -v` → 19 tests

## 4. Documentation

- [x] Root `README.md` workflow section
- [x] `AGENTS.md` workflow section and authoritative documentation
- [x] `process/ROADMAP.md` shipped table
- [x] `CHANGELOG.md` 0.2.0 entry
- [x] `.github/BRANCH_PROTECTION.md`

## 5. Verification

- [x] `python -m compileall -q scripts`
- [x] `python scripts/sync_skills.py --check`
- [x] `python scripts/check_workflow.py`
- [x] `python -m unittest discover -s scripts/tests -v` (169 tests, 12 pre-existing Windows environment errors)
- [x] `python scripts/build_lila.py --dry-run`
- [x] `python scripts/check_format.py --base origin/main` (pinned clang-format 23.1.5, changed lines only)
- [ ] Firmware build with the manual workflow

## 6. Review and merge

- [x] `architecture-compliance-check` passed (only the board helper changed)
- [x] `code-review-checklist` passed
- [x] Security scan passed
- [x] Hardware validation needs recorded in the PR
- [x] PR opened from `devs/mauri/chore/lila-workflow` (never push to `main`)
