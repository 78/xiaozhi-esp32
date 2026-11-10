---
name: Lila feature pipeline
description: Use when the user says "implement FEAT_XXX", "build feature X", or asks to run the Lila development workflow end to end.
---

# Lila feature pipeline

Run every step; do not describe the pipeline instead of executing it. The
repository rules live in `process/WORKFLOW.md` and `AGENTS.md` — read them once
per session before starting.

## Pre-flight (mandatory)

1. Read `process/README.md`, `process/WORKFLOW.md` and `AGENTS.md`.
2. Confirm the current branch is not `main`. If it is, create the feature branch
   first. **Never commit or push to `main`.**
3. Confirm the spec exists: `process/specs/FEAT_XXX/FEAT_XXX.md`. If not, create
   it from `process/specs/_template/`.
4. Load `standards-injector` before editing files.

## Steps

### 0. Spec

Create or refine `process/specs/FEAT_XXX/FEAT_XXX.md` and `tasks.md`:
metadata, summary, observable acceptance criteria, technical blueprint naming
exact files/symbols/Kconfig options, scope, dependencies, hardware validation.

### 1. Branch

```bash
git checkout main && git pull origin main
git checkout -b devs/<user>/<type>/FEAT-XXX-<slug>
```

`<user>` comes from `git config user.name` (lowercase), `<type>` is `feat`,
`fix`, `chore`, `docs` or `ci`.

### 2. Enrich

Refine the spec against the code. Use CodeGraph when available:

```bash
codegraph explore "<area>"
codegraph impact <symbol>
```

Name the files to change, the tests to add, and what must be validated on
hardware.

### 2.5. Standards

Load `standards-injector` and record the selected standards in `tasks.md`.

### 3. OpenSpec (contract changes only)

```bash
openspec change new FEAT-XXX-<slug>
```

Fill proposal, design, spec deltas and tasks. Skip for single-area changes that
do not change a contract.

### 4. Implement

- One task at a time from `tasks.md`; keep edits consistent with neighbouring
  code and `process/standards/`.
- Add or update tests with the code.
- After each task group:

```bash
python -m unittest discover -s scripts/tests -v
python scripts/check_workflow.py
```

### 5. Verify

```bash
python -m compileall -q scripts
python scripts/sync_skills.py --check
python scripts/check_workflow.py
python -m unittest discover -s scripts/tests -v
python scripts/build_lila.py --dry-run
clang-format --dry-run -Werror <changed C/C++ files>
```

If ESP-IDF is installed, build the affected variant. If not, say so explicitly
and list the remaining hardware checks.

### 5.5. Review gates

Run `architecture-compliance-check` and `code-review-checklist`. Fix findings
before committing. Record the result in the PR body.

### 5.75. Security

Scan the diff for credentials, token-bearing URLs in logs, unbounded response
parsing and missing timeouts. `scripts/check_workflow.py` covers the mechanical
part.

### 6. Docs

Update the board/root README, `process/ROADMAP.md`, `CHANGELOG.md` and
`openspec/specs/` when a contract changed.

### 7. Commit, push, PR

```bash
git add -A
git commit -m "feat(FEAT_XXX): <description>"
git push -u origin HEAD
```

Open the PR against `main` using `.github/PULL_REQUEST_TEMPLATE.md`. Never merge
with failing required checks and never push to `main`.

## Stop conditions

Stop and ask the user when:

- the change would alter an existing board's pins or OTA identity;
- a credential or private URL would be committed;
- a required check cannot run locally and the PR would claim it passed;
- the spec and the requested change disagree.
