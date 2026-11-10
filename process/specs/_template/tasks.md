# FEAT_000 — Tasks

Status: draft · Owner: @github-handle · Branch: `devs/<user>/feat/<slug>`

Each task is one reviewable commit or less. Mark `[x]` only when the code, tests
and docs for that task are in the same commit.

## 1. Spec and standards

- [ ] Refine `FEAT_000.md` against the code
- [ ] Load and record the applicable standards (`process/STANDARDS.md`)
- [ ] Create the OpenSpec change, if the contract changes

## 2. Implementation

- [ ] Task 1 — …
- [ ] Task 2 — …
- [ ] Task 3 — …

## 3. Tests

- [ ] Unit test: …
- [ ] Regression test for the failure mode: …
- [ ] Update an existing test if behaviour changed

## 4. Documentation

- [ ] Root or board README
- [ ] `process/ROADMAP.md` shipped table
- [ ] `CHANGELOG.md`
- [ ] `openspec/specs/` (if a capability contract changed)

## 5. Verification

- [ ] `python -m compileall -q scripts`
- [ ] `python scripts/sync_skills.py --check`
- [ ] `python scripts/check_workflow.py`
- [ ] `python -m unittest discover -s scripts/tests -v`
- [ ] `python scripts/build_lila.py --dry-run`
- [ ] `clang-format --dry-run -Werror <changed C/C++ files>`
- [ ] Firmware build (manual workflow or local ESP-IDF)

## 6. Review and merge

- [ ] `architecture-compliance-check` passed
- [ ] `code-review-checklist` passed
- [ ] Security scan passed
- [ ] Hardware validation needs recorded in the PR
- [ ] PR opened from the feature branch (never push to `main`)
