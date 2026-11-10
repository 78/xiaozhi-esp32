# Development workflow

The pipeline every feature follows. Steps 0-7 mirror the skills in
`.opencode/skills/`; when a step says "load skill", the agent must load it before
acting.

## Branching and PRs

| Rule | Detail |
| --- | --- |
| Never push to `main` | `main` only receives merges from pull requests, admins included |
| Branch name | `devs/<user>/<type>/<slug>` — `type` is `feat`, `fix`, `chore`, `docs`, `ci` |
| Base | always branch from an up-to-date `main` |
| PR size | one logical change; split refactors from features |
| Merge | squash merge, PR title becomes the commit subject |
| Deletion | delete the branch after merge |
| Hotfix | still a PR; prefix `fix:` and link the failing run or issue |

Branch protection and the exact required checks are documented in
[`.github/BRANCH_PROTECTION.md`](../.github/BRANCH_PROTECTION.md). Direct pushes are
also detected after the fact by the `Main branch guard` workflow, which fails on
any commit that is not associated with a merged PR.

Commit messages use Conventional Commits:

```
feat(FEAT_012): add Telegram photo sending
fix(build): keep quoted sdkconfig values on Windows
docs(process): describe the release checklist
```

## Pipeline

### Step 0 — Write the spec

Create `process/specs/FEAT_XXX/FEAT_XXX.md` from
`process/specs/_template/FEAT_000.md`. It records: metadata (id, owner, status,
priority), summary, acceptance criteria, technical blueprint, scope (in/out) and
dependencies.

Skip the spec only for typo fixes and dependency bumps. Anything that changes
firmware behaviour needs one.

### Step 1 — Create the branch

```bash
git checkout main && git pull origin main
git checkout -b devs/<user>/<type>/<slug>
```

### Step 2 — Enrich the spec

Load the `feature-pipeline` skill and refine the spec against the real code:
name the files, the Kconfig symbols, the tests, and the hardware that must be
validated. Split the work into `tasks.md` next to the spec.

### Step 2.5 — Inject standards

Load the `standards-injector` skill. It inspects the diff (or the planned files)
and selects the standards to read:

| Touched files | Standard |
| --- | --- |
| `main/**/*.cc`, `*.h`, `Kconfig.projbuild`, `CMakeLists.txt` | [`standards/firmware-cpp.md`](standards/firmware-cpp.md) |
| `scripts/**/*.py` | [`standards/python-tooling.md`](standards/python-tooling.md) |
| `.github/**`, release or version changes | [`standards/ci-release.md`](standards/ci-release.md) |
| `*.md`, board READMEs, `docs/**` | [`standards/docs.md`](standards/docs.md) |
| credentials, Kconfig strings, logging, network payloads | [`standards/security.md`](standards/security.md) |

### Step 3 — Propose the change (cross-cutting work)

For anything that changes a contract (protocol, Kconfig surface, build CLI,
release process) record an OpenSpec change:

```bash
openspec list
openspec change new FEAT-XXX-<slug>
```

Fill in the proposal, design, spec deltas and tasks. Small, single-area changes
may go straight from the FEAT spec.

### Step 4 — Implement

- Work task by task from `tasks.md`.
- Write or update tests with the code, not after it.
- Keep the build green in small increments: `python -m unittest discover -s
  scripts/tests -v` after each group.
- Use CodeGraph to find the real entry points before changing shared code
  (`codegraph explore "<area>"`, `codegraph impact <symbol>`).
- Run commands through RTK when the agent reads their output (`rtk git status`,
  `rtk pytest`, ...) to keep context small.

### Step 5 — Verify locally

```bash
python -m compileall -q scripts
python scripts/sync_skills.py --check
python scripts/check_workflow.py
python -m unittest discover -s scripts/tests -v
python scripts/build_lila.py --dry-run      # command is what we expect
python scripts/check_format.py --base origin/main   # clang-format on changed lines
```

`check_format.py` checks only the C/C++ lines the branch changed, so upstream
style inside a patched file is not a failure. It uses the pinned clang-format
23.1.5: `python -m pip install clang-format==23.1.5`.

Firmware compilation and hardware checks are reported separately; a passing host
suite is not hardware validation. If ESP-IDF is available:

```bash
python scripts/build.py wk-esp32s3-dev --name wk-esp32s3-dev --language es-ES \
  --wake-word custom:lila
```

### Step 5.5 — Review gates

Load both skills and record the result in the PR:

1. `architecture-compliance-check` — layering, board rules, state machine,
   blocking, ownership.
2. `code-review-checklist` — the checklist in
   `.opencode/skills/code-review-checklist/SKILL.md`.

### Step 5.75 — Security scan

Check the diff for credentials, tokens in logs, unvalidated network input and
unsafe string handling. The rules live in
[`standards/security.md`](standards/security.md). `check_workflow.py` performs a
mechanical secret scan; review the payload paths manually.

### Step 6 — Docs, roadmap, changelog

- Update the board README / root README for user-visible change.
- Add the entry to `process/ROADMAP.md` (shipped section) and `CHANGELOG.md`.
- Update `openspec/specs/` when a capability contract changed.

### Step 7 — Commit, push, open the PR

```bash
git add -A
git commit -m "feat(FEAT_XXX): <description>"
git push -u origin HEAD
```

Open the PR against `main` with the template. The PR body links the spec, lists
the checks run, and states what still needs hardware validation. Do not merge
with failing required checks; do not bypass branch protection.

## Quality gates

A PR is mergeable only when all of these are true:

- [ ] The spec and `tasks.md` are updated to the final state.
- [ ] `check_workflow`, skill parity, compile checks and the full host suite pass.
- [ ] Changed C/C++ lines are clang-format clean (`python scripts/check_format.py --base origin/main`).
- [ ] `python scripts/build_lila.py --dry-run` produces the expected command.
- [ ] `architecture-compliance-check` and `code-review-checklist` pass.
- [ ] No secret, token or private URL is added; logs are redacted.
- [ ] README/board docs, roadmap and changelog are updated.
- [ ] Hardware validation needs are stated explicitly (what was tested, what was not).

## Firmware builds in CI

`.github/workflows/firmware-build.yml` is manual (`workflow_dispatch`). Use it
when a change touches firmware code, a board, Kconfig or assets; it builds the
Lila configuration in the ESP-IDF 6.1 container and uploads the binaries. Host
checks in `ci.yml` run on every PR.
