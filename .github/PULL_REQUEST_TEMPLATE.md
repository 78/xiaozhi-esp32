## Summary

<!-- One paragraph: what changed and why. Link the spec. -->

- Spec: `process/specs/FEAT_XXX/FEAT_XXX.md`
- OpenSpec change: `FEAT-XXX-<slug>` (if applicable)

## Type of change

- [ ] Feature (`feat`)
- [ ] Fix (`fix`)
- [ ] Chore / refactor (`chore`)
- [ ] Documentation (`docs`)
- [ ] CI or release (`ci`)

## What was validated

- [ ] `python -m compileall -q scripts`
- [ ] `python scripts/sync_skills.py --check`
- [ ] `python scripts/check_workflow.py`
- [ ] `python -m unittest discover -s scripts/tests -v`
- [ ] `python scripts/build_lila.py --dry-run`
- [ ] `clang-format --dry-run -Werror` on changed C/C++ files
- [ ] Manual firmware workflow (paste run URL) or local ESP-IDF build
- [ ] Nothing (docs-only change)

## Review gates

- [ ] `architecture-compliance-check` passed (record findings below)
- [ ] `code-review-checklist` passed
- [ ] Security scan passed: no token, secret or token-bearing URL in the diff or logs
- [ ] Applies the standards from `process/STANDARDS.md`:
  - [ ] `firmware-cpp` (if `main/**` changed)
  - [ ] `python-tooling` (if `scripts/**` changed)
  - [ ] `ci-release` (if `.github/**` or versions changed)
  - [ ] `docs`
  - [ ] `security`

## Documentation

- [ ] Root or board README updated
- [ ] `process/ROADMAP.md` shipped table updated
- [ ] `CHANGELOG.md` updated
- [ ] `openspec/specs/` updated if a capability contract changed

## Hardware validation

<!-- State what was tested on a device and what is still pending. -->

- Tested:
- Pending:

## Notes for reviewers

<!-- Risky areas, follow-ups, or decisions that need a second opinion. -->

---

By opening this PR I confirm it targets `main` from a `devs/<user>/<type>/<slug>`
branch, and that `main` is never pushed to directly (see `process/WORKFLOW.md`
and `process/adr/0001-pr-only-workflow.md`).
