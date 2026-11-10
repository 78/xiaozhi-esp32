# CI, branching and release standard

## Branching

- **CI-1** `main` is protected. Every change arrives through a pull request from
  `devs/<user>/<type>/<slug>`; admins are not exempt.
- **CI-2** One logical change per PR. Refactors do not ride along with features.
- **CI-3** Squash merge only; the PR title becomes the commit subject and must
  follow Conventional Commits.
- **CI-4** Force-push is allowed only on your own feature branch, never on
  `main`.

Enforcement is two-layered:

1. GitHub branch protection (preventive) — see
   `.github/BRANCH_PROTECTION.md`.
2. `Main branch guard` workflow (detective) — fails when a commit on `main` has
   no merged PR associated with it.

## Required checks

| Workflow | Trigger | Must pass |
| --- | --- | --- |
| `CI` | every PR, every non-`main` push | yes |
| `Build Boards` (upstream) | PRs touching firmware, manual | only when the change touches covered boards |
| `Firmware build (manual)` | `workflow_dispatch` | not a merge gate; record the run in the PR |
| `Main branch guard` | push to `main` | informational alert, should never fire |

`CI` runs:

1. `python -m compileall -q scripts`
2. `python scripts/sync_skills.py --check`
3. `python scripts/check_workflow.py`
4. `python -m unittest discover -s scripts/tests -v`
5. `python scripts/check_format.py --base origin/main` (pinned clang-format 23.1.5, changed lines only)
6. `python scripts/build_lila.py --dry-run`

A red check blocks the merge. Do not "fix" a flaky check by disabling it in the
same PR; open a separate PR with the reason.

## Versioning

- Semver for the whole project: `MAJOR.MINOR.PATCH`.
- A feature release bumps `MINOR`; fixes bump `PATCH`; removing or renaming a
  user-facing option bumps `MAJOR`.
- The version lives in the release tag and `CHANGELOG.md`; firmware artifacts are
  named `<version>_<board>.zip`.

## Releases

1. All PRs for the milestone merged.
2. `CHANGELOG.md` has an entry for the version, `process/ROADMAP.md` is current.
3. Tag `vX.Y.Z` on `main` after the merge.
4. Build the Lila firmware with the manual workflow or locally; verify the
   `merged-binary.bin` size fits the partition table.
5. Publish the artifacts and the changelog excerpt in the GitHub release.

## Secrets

- CI uses only the automatic `GITHUB_TOKEN` with `contents: read`.
- Never add repository secrets for firmware builds that can be reproduced with
  an empty `.env`.
- A workflow that needs a credential must fail closed when it is absent.
